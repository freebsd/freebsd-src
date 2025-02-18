/*
 * System call argument to DTrace register array conversion.
 *
 * This file is part of the DTrace syscall provider.
 *
 * DO NOT EDIT-- this file is automatically @generated.
 */

static void
systrace_args(int sysnum, void *params, uint64_t *uarg, int *n_args)
{
	int64_t *iarg = (int64_t *)uarg;
	int a = 0;
	switch (sysnum) {
#ifdef PLATFORM_FOO
	/* syscall1 */
	case 1: {
		struct syscall1_args *p = params;
		iarg[a++] = p->arg1; /* int */
		*n_args = 1;
		break;
	}
#else
#endif
#ifdef PLATFORM_FOO
#else
	/* syscall2 */
	case 2: {
		*n_args = 0;
		break;
	}
#endif
	default:
		*n_args = 0;
		break;
	};
}
static void
systrace_entry_setargdesc(int sysnum, int ndx, char *desc, size_t descsz)
{
	const char *p = NULL;
	switch (sysnum) {
#ifdef PLATFORM_FOO
	/* syscall1 */
	case 1:
		switch (ndx) {
		case 0:
			p = "int";
			break;
		default:
			break;
		};
		break;
#else
#endif
#ifdef PLATFORM_FOO
#else
	/* syscall2 */
	case 2:
		break;
#endif
	default:
		break;
	};
	if (p != NULL)
		strlcpy(desc, p, descsz);
}
static void
systrace_return_setargdesc(int sysnum, int ndx, char *desc, size_t descsz)
{
	const char *p = NULL;
	switch (sysnum) {
#ifdef PLATFORM_FOO
	/* syscall1 */
	case 1:
		if (ndx == 0 || ndx == 1)
			p = "int";
		break;
#else
#endif
#ifdef PLATFORM_FOO
#else
	/* syscall2 */
	case 2:
#endif
	default:
		break;
	};
	if (p != NULL)
		strlcpy(desc, p, descsz);
}
