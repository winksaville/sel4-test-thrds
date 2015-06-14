/*
 * Copyright 2014, Wink Saville
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 */

#include <stdio.h>
#include <sel4thrds/thrds.h>

void test_thrd_initialize() {
  printf("test_thrd_initialize:+\n");
  thrd_initialize();
  printf("test_thrd_initialize:-\n");
}

void test_configure_cleanup() {
  thrd_env_t env;
  thrd_t thread;

  printf("test_configure_cleanup:+\n");

  thrd_configure(&env, &thread);
  thrd_cleanup(&env, &thread);

  printf("test_configure_cleanup:-\n");
}
