/* Author: Wietse Venema, Eindhoven University of Technology. 
 */

#ifndef lint
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

#include <stdio.h>
#include <string.h>
#include <pwd.h>

#include <skey.h>

/* skey_challenge - additional password prompt stuff */

char   *skey_challenge(name, pwd, pwok)
char   *name;
struct passwd *pwd;
int    pwok;
{
    static char buf[128];
    struct skey skey;

    /* Display s/key challenge where appropriate. */

    *buf = '\0';
    if (pwd == NULL || skeychallenge(&skey, pwd->pw_name, buf))
	snprintf(buf, sizeof(buf), "Password required for %s.", name);
    else if (!pwok)
	strcat(buf, " (s/key required)");
    return (buf);
}
