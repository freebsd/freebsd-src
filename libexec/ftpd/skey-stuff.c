/* Author: Wietse Venema, Eindhoven University of Technology. 
 *
 *	$Id: skey-stuff.c,v 1.3 1996/09/22 21:53:34 wosch Exp $
 */

#include <stdio.h>
#include <pwd.h>

#include <skey.h>

/* skey_challenge - additional password prompt stuff */

char   *skey_challenge(name, pwd, pwok, sflag)
char   *name;
struct passwd *pwd;
int     pwok;
int    *sflag;
{
    static char buf[128];
    struct skey skey;
    char *username = pwd ? pwd->pw_name : ":";

    /* Display s/key challenge where appropriate. */

    *sflag = skeychallenge(&skey, username, buf);
    sprintf(buf, "%s required for %s.",
	pwok ? "Password" : "S/Key password", name);
    return (buf);
}
