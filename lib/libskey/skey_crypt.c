/* Author: Wietse Venema, Eindhoven University of Technology. */

#include <string.h>
#include <stdio.h>
#include <pwd.h>
#include <skey.h>

/* skey_crypt - return encrypted UNIX passwd if s/key or regular password ok */

char   *skey_crypt(pp, salt, pwd, pwok)
char   *pp;
char   *salt;
struct passwd *pwd;
int     pwok;
{
    struct skey skey;
    char   *p;
    char   *crypt();

    /* Try s/key authentication even when the UNIX password is permitted. */

    if (pwd != 0 && skeylookup(&skey, pwd->pw_name) == 0
	&& skeyverify(&skey, pp) == 0) {
	/* s/key authentication succeeded */
	return (pwd->pw_passwd);
    }

    /* When s/key authentication does not work, always invoke crypt(). */

    p = crypt(pp, salt);
    if (pwok && pwd != 0 && strcmp(p, pwd->pw_passwd) == 0)
	return (pwd->pw_passwd);

    /* The user does not exist or entered bad input. */

    return (":");
}
