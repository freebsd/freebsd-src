#if defined(LIBC_RCS) && !defined(lint)
static const char rcsid[] =
  "$FreeBSD$";
#endif /* LIBC_RCS and not lint */

extern const char *__progname;

void
setprogname(const char *progname)
{

	__progname = progname;
}
