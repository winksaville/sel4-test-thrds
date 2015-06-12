# sel4-test-thrds

An seL4 application to test the libsel4thrds library.

To see available make parameters:
```
make help
```
To build this app:
(Note: do NOT "make menuconfig" first its a bad configuration)
```
make ia32_simulation_test_thrds_defconfig
```
To run this app:
```
make simulate-test-thrds-ia32
```
See Makefile for other targets.
