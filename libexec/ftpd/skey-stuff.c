/* Author: Wietse Venema, Eindhoven University of Technology. */

#include <stdio.h>
#include <pwd.h>

#include <skey.h>

/* skey_challenge - additional password prompt stuff */

char   *skey_challenge(name, pwd, pwok)
char   *name;
struct passwd *pwd;
int     pwok;
{
    static char buf[128];
    struct skey skey;

    /* Display s/key challenge where appropriate. */

    if (pwd == 0 || skeychallenge(&skey, pwd->pw_name, buf) != 0)
	sprintf(buf, "Password required for %s.", name);
    return (buf);
}
