#ifndef _KERNEL_TESTS_H_
#define _KERNEL_TESTS_H_

/*
 * Main kernel test function.
 */
void kernel_testmain(void);

/*
 * Submodule test functions.
 */
void testmm(void);

/*
 * Submodule test functions.
 */
void testvm(void);

/*
 * Submodule test functions.
 */
void testlist(void);

/*
 * Submodule test functions.
 */
void testthread(void);

/*
 * Radix tree tests.
 */
void testradixtree(void);

/*
 * IDE tests.
 */
void testide(void);

/*
 * String library tests.
 */
void teststring(void);

/*
 * SFS tests.
 */
void testsfs(void);

#endif /* _KERNEL_TESTS_H_ */
