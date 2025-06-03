/*
 * System call switch table.
 *
 * DO NOT EDIT-- this file is automatically @generated.
 */

#include <sys/param.h>
#include <sys/sysent.h>
#include <sys/sysproto.h>

#define AS(name) (sizeof(struct name) / sizeof(syscallarg_t))

/* The casts are bogus but will do for now. */
struct sysent test_sysent[] = {
	{ .sy_narg = 0, .sy_call = (sy_call_t *)nosys, .sy_auevent = AUE_NULL, .sy_flags = 0, .sy_thrcnt = SY_THR_ABSENT },	/* 0 = unimpl_syscall0 */
#ifdef PLATFORM_FOO
	{ .sy_narg = AS(syscall1_args), .sy_call = (sy_call_t *)sys_syscall1, .sy_auevent = AUE_NULL, .sy_flags = 0, .sy_thrcnt = SY_THR_STATIC },	/* 1 = syscall1 */
#else
	{ .sy_narg = 0, .sy_call = (sy_call_t *)nosys, .sy_auevent = AUE_NULL, .sy_flags = 0, .sy_thrcnt = SY_THR_ABSENT },	/* 1 = reserved for local use */
#endif
#ifdef PLATFORM_FOO
	{ .sy_narg = 0, .sy_call = (sy_call_t *)nosys, .sy_auevent = AUE_NULL, .sy_flags = 0, .sy_thrcnt = SY_THR_ABSENT },	/* 2 = obsolete syscall2 */
#else
	{ .sy_narg = 0, .sy_call = (sy_call_t *)sys_syscall2, .sy_auevent = AUE_NULL, .sy_flags = 0, .sy_thrcnt = SY_THR_STATIC },	/* 2 = syscall2 */
#endif
};
