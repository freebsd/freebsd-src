/* $FreeBSD$ */

/*
 * Compat shims for those programs that use this newer interface.  These
 * are more minimal than their libc bretheren as far as namespaces and
 * such go because their use is so limited.  Also, the libc versions
 * have too many depends on libc build environment; it is more of a pain
 * to set that up than to recreate them here shorn of all the other goo.
 */

extern const char *__progname;

void
setprogname(const char *p)
{

	__progname = p;
}

const char *
getprogname(void)
{

	return (__progname);
}
