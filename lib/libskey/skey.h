#ifndef _SKEY_H_
#define _SKEY_H_

#include <sys/cdefs.h>

/* Server-side data structure for reading keys file during login */
struct skey {
	FILE *keyfile;
	char buf[256];
	char *logname;
	int n;
	char *seed;
	char *val;
	long	recstart; /*needed so reread of buffer is efficient*/
};

#ifdef _SKEY_INTERNAL
/* Client-side structure for scanning data stream for challenge */
struct mc {
	char buf[256];
	int skip;
	int cnt;
};

#define atob8           _sk_atob8
#define btoa8           _sk_btoa8
#define btoe            _sk_btoe
#define etob            _sk_etob
#define f               _sk_f
#define htoi            _sk_htoi
#define keycrunch       _sk_keycrunch
#define put8            _sk_put8
#define readpass        _sk_readpass
#define rip             _sk_rip
#define sevenbit        _sk_sevenbit

void f __P((char *x));
int keycrunch __P((char *result,char *seed,char *passwd));
char *btoe __P((char *engout,char *c));
char *put8 __P((char *out,char *s));
int atob8 __P((char *out, char *in));
int btoa8 __P((char *out, char *in));
int htoi __P((char c));
int etob __P((char *out,char *e));
void sevenbit __P((char *s));
char *readpass __P((char *buf, int n));
void rip __P((char *buf));
#endif  /* _SKEY_INTERNAL */

/* Simplified application programming interface. */
#include <pwd.h>
int skeylookup __P((struct skey *mp,char *name));
int skeyverify __P((struct skey *mp,char *response));
int skeychallenge __P((struct skey *mp,char *name, char *challenge));
int skeyinfo __P((struct skey *mp, char* name, char *ss));
int skeyaccess __P((char *user, char *port, char *host, char *addr));
char *skey_getpass __P((char *prompt, struct passwd *pwd, int pwok));
char *skey_crypt __P((char *pp, char *salt, struct passwd *pwd, int pwok));

#endif /* _SKEY_H_ */
