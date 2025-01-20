/*
 * System call names.
 *
 * DO NOT EDIT-- this file is automatically @generated.
 */

const char *test_syscallnames[] = {
	"#0",			/* 0 = unimpl_syscall0 */
#ifdef PLATFORM_FOO
	"syscall1",			/* 1 = syscall1 */
#else
	"#1",			/* 1 = reserved for local use */
#endif
#ifdef PLATFORM_FOO
	"obs_syscall2",			/* 2 = obsolete syscall2 */
#else
	"syscall2",			/* 2 = syscall2 */
#endif
};
