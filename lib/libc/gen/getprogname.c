#if defined(LIBC_RCS) && !defined(lint)
static const char rcsid[] =
  "$FreeBSD: src/lib/libc/gen/getprogname.c,v 1.1.2.1 2001/06/14 00:06:12 dd Exp $";
#endif /* LIBC_RCS and not lint */

extern const char *__progname;

const char *
getprogname(void)
{

	return (__progname);
}
