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

/* Client-side structure for scanning data stream for challenge */
struct mc {
	char buf[256];
	int skip;
	int cnt;
};

void f __P((char *x));
int keycrunch __P((char *result,char *seed,char *passwd));
char *btoe __P((char *engout,char *c));
char *put8 __P((char *out,char *s));
int etob __P((char *out,char *e));
void rip __P((char *buf));
int skeychallenge __P((struct skey *mp,char *name, char *challenge));
int skeylookup __P((struct skey *mp,char *name));
int skeyverify __P((struct skey *mp,char *response));

#endif /* _SKEY_H_ */
