/* This is a generated file */
#ifndef __login_protos_h__
#define __login_protos_h__

#ifdef __STDC__
#include <stdarg.h>
#ifndef __P
#define __P(x) x
#endif
#else
#ifndef __P
#define __P(x) ()
#endif
#endif

void
check_shadow __P((
	const struct passwd *pw,
	const struct spwd *sp));

char *
clean_ttyname __P((char *tty));

int
do_osfc2_magic __P((uid_t uid));

int
login_access __P((
	struct passwd *user,
	char *from));

char *
login_conf_get_string __P((const char *str));

char *
make_id __P((char *tty));

void
prepare_utmp __P((
	struct utmp *utmp,
	char *tty,
	const char *username,
	const char *hostname));

int
read_string __P((
	const char *prompt,
	char *buf,
	size_t len,
	int echo));

void
stty_default __P((void));

void
utmp_login __P((
	char *tty,
	const char *username,
	const char *hostname));

int
utmpx_login __P((
	char *line,
	const char *user,
	const char *host));

#endif /* __login_protos_h__ */
