/* Author: Wietse Venema, Eindhoven University of Technology. 
 *
 *	$Id: skey-stuff.c,v 1.4 1996/10/17 17:06:04 ache Exp $
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
    if (*sflag)
	sprintf(buf, "%s required for %s.",
	    pwok ? "Password" : "S/Key password", name);
    return (buf);
}
