/*
 *
 * passwdauth.c  - pjchilds@imforei.apana.org.au
 *
 * authenticate user via the password file
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the Peter Childs.  The name of the author may not be used to
 * endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 */

#include <sys/types.h>
#include <utmp.h>
#include <time.h>
#include <libutil.h>
#include <pwd.h>
#include "fsm.h"
#include "passwdauth.h"

int
PasswdAuth(name, key)
char *name, *key;
{
  static int logged_in = 0;
  struct passwd *pwd;
  char *salt, *ep;
  struct utmp utmp;

#ifdef DEBUG
  logprintf( "passwdauth called with name= %s, key= %s\n", name, key );
#endif /* DEBUG */

  if(( pwd = getpwnam( name ) ))
    salt = pwd->pw_passwd;
  else
  {
    endpwent();
    LogPrintf( LOG_LCP, "PasswdAuth - user (%s) not in passwd file\n", name );
    return 0; /* false - failed to authenticate (password not in file) */
  }

#ifdef LOCALHACK
  /*
   * All our PPP usernames start with 'P' so i check that here... if you
   * don't do this i suggest all your PPP users be members of a group
   * and you check the guid
   */

  if( name[0] != 'P' )
  { 
    LogPrintf( LOG_LCP, "PasswdAuth - user (%s) not a PPP user\n", name );
    endpwent();
    return 0;
  }

#endif /* LOCALHACK */

  ep = crypt( key, salt );

  /* strcmp returns 0 if same */
  if( strcmp( ep, pwd->pw_passwd ) != 0 )
  {
    LogPrintf( LOG_LCP, "PasswdAuth - user (%s,%s) authentication failed\n",
	name, key );
    endpwent();
    return 0;  /* false - failed to authenticate (didn't match up) */
  }

  /*
   * now we log them in... we have a static login flag so we don't
   * do it twice :)
   */

  if( ! logged_in )
  {
    (void)time(&utmp.ut_time);
    (void)strncpy(utmp.ut_name, name, sizeof(utmp.ut_name));

    /*
     * if the first three chacters are "pap" trim them off before doing
     * utmp entry (see sample.ppp-pap-dialup 
     */

    if( strncmp( "pap", dstsystem, 3 ) == 0 )
      (void)strncpy(utmp.ut_line, (char *)(dstsystem + 3), sizeof(utmp.ut_line));
    else
      (void)strncpy(utmp.ut_line, dstsystem, sizeof(utmp.ut_line));

    (void)strcpy(utmp.ut_host, "auto-ppp" );
    login(&utmp);
    (void)setlogin( pwd->pw_name );

    LogPrintf( LOG_LCP, "PasswdAuth has logged in user %s\n", name );

    logged_in = 1;
  }

  endpwent();

  return 1;
}
