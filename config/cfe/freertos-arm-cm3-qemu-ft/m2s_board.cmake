# m2s-cfs mission overlay "freertos-arm-cm3-qemu-ft" (step 09c): cFE's functional tests on the freertos-arm-cm3
# mission, board boards/qemu-mps2-an385 (QEMU mps2-an385). Read by toolchain-freertos-arm-cm3.cmake.
# TEST image: cfe_assert uses OSAL's ut_assert, which allocates with malloc, so this mission links the static
# test-only allocator (boards/common-cm3/test_heap.c, D33) instead of the allocator traps. Flight missions
# never set M2S_CM3_TEST_HEAP.
SET(M2S_BOARD_NAME qemu-mps2-an385)
SET(M2S_CM3_TEST_HEAP ON CACHE BOOL "Link the test-only allocator (test images only, never flight)" FORCE)
