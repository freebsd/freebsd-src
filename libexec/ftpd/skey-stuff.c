/* Author: Wietse Venema, Eindhoven University of Technology. 
 *
 *	$Id: skey-stuff.c,v 1.6 1996/10/18 17:09:26 ache Exp $
 */

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

    if (pwd == NULL || skeychallenge(&skey, pwd->pw_name, buf))
	sprintf(buf, "Password required for %s.", name);
    else if (!pwok)
	strcat(buf, " (s/key required)");
    return (buf);
}
