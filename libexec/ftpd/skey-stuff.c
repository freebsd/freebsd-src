/* Author: Wietse Venema, Eindhoven University of Technology. 
 *
 *	$Id: skey-stuff.c,v 1.3 1996/09/22 21:53:34 wosch Exp $
 */

#include <stdio.h>
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
