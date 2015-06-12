/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

/* Include Kconfig variables. */
#include <autoconf.h>

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <limits.h>

#include <allocman/bootstrap.h>
#include <allocman/vka.h>

#include <platsupport/timer.h>

#include <sel4platsupport/platsupport.h>
#include <sel4platsupport/plat/timer.h>
#include <sel4utils/vspace.h>
#include <sel4utils/stack.h>
#include <sel4utils/process.h>

#include <simple/simple.h>
#ifdef CONFIG_KERNEL_STABLE
#include <simple-stable/simple-stable.h>
#else
#include <simple-default/simple-default.h>
#endif

#include <utils/util.h>

#include <vka/object.h>
#include <vka/capops.h>

#include <vspace/vspace.h>

#include <sel4thrds/thrds.h>

#include "tests.h"

typedef struct env {
    /* An initialised vka that may be used by the test. */
    vka_t vka;
    /* virtual memory management interface */
    vspace_t vspace;
    /* abtracts over kernel version and boot environment */
    simple_t simple;
    /* path for the default timer irq handler */
    cspacepath_t irq_path;
#ifdef CONFIG_ARCH_ARM
    /* frame for the default timer */
    cspacepath_t frame_path;
#elif CONFIG_ARCH_IA32
    /* io port for the default timer */
    seL4_CPtr io_port_cap;
#endif
    /* extra cap to the init data frame for mapping into the remote vspace */
    seL4_CPtr init_frame_cap_copy;
} env_t;

/* dimensions of virtual memory for the allocator to use */
#define ALLOCATOR_VIRTUAL_POOL_SIZE ((1 << seL4_PageBits) * 100)

/* static memory for the allocator to bootstrap with */
#define ALLOCATOR_STATIC_POOL_SIZE ((1 << seL4_PageBits) * 10)
static char allocator_mem_pool[ALLOCATOR_STATIC_POOL_SIZE];

/* static memory for virtual memory bootstrapping */
static sel4utils_alloc_data_t data;

/* environment encapsulating allocation interfaces etc */
static env_t env;

/* initialise our runtime environment */
static void
init_env(env_t *env)
{
    allocman_t *allocman;
    UNUSED reservation_t virtual_reservation;
    UNUSED int error;

    /* create an allocator */
    allocman = bootstrap_use_current_simple(&env->simple, ALLOCATOR_STATIC_POOL_SIZE, allocator_mem_pool);
    assert(allocman);

    /* create a vka (interface for interacting with the underlying allocator) */
    allocman_make_vka(&env->vka, allocman);

    /* create a vspace (virtual memory management interface). We pass
     * boot info not because it will use capabilities from it, but so
     * it knows the address and will add it as a reserved region */
    error = sel4utils_bootstrap_vspace_with_bootinfo_leaky(&env->vspace,
	    &data, simple_get_pd(&env->simple), &env->vka, seL4_GetBootInfo());

    /* fill the allocator with virtual memory */
    void *vaddr;
    virtual_reservation = vspace_reserve_range(&env->vspace,
                                               ALLOCATOR_VIRTUAL_POOL_SIZE, seL4_AllRights, 1, &vaddr);
    assert(virtual_reservation.res);
    bootstrap_configure_virtual_pool(allocman, vaddr,
                                     ALLOCATOR_VIRTUAL_POOL_SIZE, simple_get_pd(&env->simple));
}


static void
init_timer_caps(env_t *env)
{
    /* get the timer irq cap */
    seL4_CPtr cap;
    UNUSED int error = vka_cspace_alloc(&env->vka, &cap);
    assert(error == 0);

    vka_cspace_make_path(&env->vka, cap, &env->irq_path);
    error = simple_get_IRQ_control(&env->simple, DEFAULT_TIMER_INTERRUPT, env->irq_path);
    assert(error == 0);

#ifdef CONFIG_ARCH_ARM
    /* get the timer frame cap */
    error = vka_cspace_alloc(&env->vka, &cap);
    assert(error == 0);

    vka_cspace_make_path(&env->vka, cap, &env->frame_path);
    error = simple_get_frame_cap(&env->simple, (void *) DEFAULT_TIMER_PADDR, PAGE_BITS_4K, &env->frame_path);
    assert(error == 0);
#elif CONFIG_ARCH_IA32
    env->io_port_cap = simple_get_IOPort_cap(&env->simple, PIT_IO_PORT_MIN, PIT_IO_PORT_MAX);
    assert(env->io_port_cap != 0);
#else
#error "Unknown architecture"
#endif
}


void *main_continued(void *arg UNUSED)
{
    printf("\nmain_continued:+\n");

    /* get the caps we need to send to tests to set up a timer */
    init_timer_caps(&env);

    /* Code to run */
    test_thrd_doNothing();

    printf("main_continued:-\n\n");
    return NULL;
}

int main(void)
{
    seL4_BootInfo *info = seL4_GetBootInfo();

#ifdef SEL4_DEBUG_KERNEL
    seL4_DebugNameThread(seL4_CapInitThreadTCB, "CapInitThread");
#endif

    //compile_time_assert(init_data_fits_in_ipc_buffer, sizeof(test_init_data_t) < PAGE_SIZE_4K);
    /* initialise libsel4simple, which abstracts away which kernel version
     * we are running on */
#ifdef CONFIG_KERNEL_STABLE
    simple_stable_init_bootinfo(&env.simple, info);
#else
    simple_default_init_bootinfo(&env.simple, info);
#endif

    /* initialise the test environment - allocator, cspace manager, vspace manager, timer */
    init_env(&env);

    /* enable serial driver */
    platsupport_serial_setup_simple(NULL, &env.simple, &env.vka);

    /* switch to a bigger, safer stack with a guard page
     * before starting the tests */
    printf("Switching to a safer, bigger stack\n");
    intptr_t result = (intptr_t)sel4utils_run_on_stack(&env.vspace, main_continued, NULL);
    if (result != 0) {
      printf("sel4utiles_run_on_stack: returned non-zero result=%d\n", result);
    }

    return result;
}

