/*
 * define path names
 *
 * $FreeBSD$
 */

#ifdef HAVE_PATHS_H
#include <paths.h>

#else
#define _PATH_VARRUN 	"/etc/ppp/"
#define _PATH_DEVNULL	"/dev/null"
#endif

#define _PATH_UPAPFILE 	"/etc/ppp/pap-secrets"
#define _PATH_CHAPFILE 	"/etc/ppp/chap-secrets"
#define _PATH_SYSOPTIONS "/etc/ppp/options"
#define _PATH_IPUP	"/etc/ppp/ip-up"
#define _PATH_IPDOWN	"/etc/ppp/ip-down"
#define _PATH_TTYOPT	"/etc/ppp/options."
#define _PATH_CONNERRS	"/etc/ppp/connect-errors"
#define _PATH_USEROPT	".ppprc"
