/*
 * Copyright (c) 1992/3 Theo de Raadt <deraadt@fsa.ca>
 * Copyright (c) 1994 Olaf Kirch <okir@monad.swb.de>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <time.h>
#include <sys/types.h>
#include <pwd.h>
#include <errno.h>
#include <unistd.h>
#include <limits.h>
#include <rpc/rpc.h>
#include <rpcsvc/yp_prot.h>
#include <rpcsvc/ypclnt.h>
#include <rpcsvc/yppasswd.h>

extern int    use_yp_passwd, opt_fullname, opt_shell;
extern char *prog_name;
uid_t	uid;

static unsigned char itoa64[] =		/* 0 ... 63 => ascii - 64 */
"./0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

char *getnewyppasswd(struct passwd *);

char *
getnewyppasswd(register struct passwd *pw)
{
  char *buf;
  char salt[9], *p=NULL;
  int tries = 0;
  
  buf = (char *) malloc(30);
  
  printf("Changing YP password for %s.\n", pw->pw_name);
  
  buf[0] = '\0';
  while(1) {
    p = getpass("Please enter new password:");
    if(*p == '\0') {
        printf("Password unchanged.\n");
        return NULL;
    }
#ifndef DEBUG
    if (strlen(p) <= 5 && (uid != 0 || ++tries < 2)) {
        printf("Please enter a longer password.\n");
        continue;
    }
#endif
    strcpy(buf, p);
    p = getpass("Please retype new password:");
    if( strcmp(buf, p) == 0) {
        break;
    } else {
        printf("Mismatch - password unchanged.\n");
        return NULL;
    }
  }
  
  /* grab a random printable character that isn't a colon */
  srandom((int)time((time_t *)NULL));
  to64(&salt[0], random(), 2);
  return strdup(crypt(buf, salt));
}

char *
getfield(char *gecos, char *field, int size)
{
    char *sp;

    for (sp = gecos; *sp != '\0' && *sp != ','; sp++);
    if (*sp != '\0') {
    	*sp++ = '\0';
    }
    strncpy (field, gecos, size-1);
    field[size-1] = '\0';
    return sp;
}

int
newfield(char *prompt, char *deflt, char *field, int size)
{
    char	*sp;

    if (deflt == NULL) {
        deflt = "none";
    }

    printf("%s [%s]: ", prompt, deflt);
    fflush(stdout);
    if (fgets(field, size, stdin) == NULL) {
    	return 1;
    }

    if ((sp = strchr(field, '\n')) != NULL) {
    	*sp = '\0';
    }

    if (!strcmp(field, "")) {
    	strcpy(field, deflt);
    }
    if (!strcmp(field, "none")) {
    	strcpy(field, "");
    }

    if (strchr(field, ':') != NULL) {
    	fprintf(stderr, "%s: no colons allowed in GECOS field... sorry.\n",
    				prog_name);
    	return 1;
    }
    return 0;
}

char *
getnewfullname(struct passwd *pw)
{
    char	gecos[1024], *sp, new_gecos[1024];
    char	name[254], location[254], office[254], phone[254];

    printf ("\nChanging full name for %s.\n"
    	    "To accept the default, simply press return. To enter an empty\n"
    	    "field, type the word \"none\".\n",
    	    pw->pw_name);

    strncpy (gecos, pw->pw_gecos, sizeof(gecos));
    sp = getfield(gecos, name, sizeof(name));
    if (newfield("Name", strtok(gecos, ","), name, sizeof(name))) {
    	return NULL;
    }
    sp = getfield(sp, location, sizeof(location));
    if (newfield("Location", location, location, sizeof(location))) {
    	return NULL;
    }
    sp = getfield(sp, office, sizeof(office));
    if (newfield("Office Phone", office, office, sizeof(office))) {
    	return NULL;
    }
    sp = getfield(sp, phone, sizeof(phone));
    if (newfield("Home Phone", phone, phone, sizeof(phone))) {
    	return NULL;
    }
    sprintf (new_gecos, "%s,%s,%s,%s", name, location, office, phone);

    sp = new_gecos + strlen(new_gecos);
    while (*--sp == ',') *sp = '\0';

    return strdup(new_gecos);
}

char *
getnewshell(struct passwd *pw)
{
    char    new_shell[PATH_MAX];

    printf ("\nChanging login shell for %s.\n"
    	    "To accept the default, simply press return. To use the\n"
    	    "system's default shell, type the word \"none\".\n",
    	    pw->pw_name);

    if (newfield("Login shell", pw->pw_shell, new_shell, sizeof(new_shell))) {
        return NULL;
    }
    return strdup(new_shell);
}

char *
getserver( void )
{
  char  *domainname, *master;
  int 	port, err;
  int getrpcport();

  if ((err = yp_get_default_domain(&domainname)) != 0) {
      fprintf(stderr, "%s: can't get local yp domain: %s\n",
              				prog_name, yperr_string(err));
      return NULL;
  }
  
  if ((err = yp_master(domainname, "passwd.byname", &master)) != 0) {
      fprintf(stderr, "%s: can't find the master ypserver: %s\n",
              				prog_name, yperr_string(err));
      return NULL;
  }
  port = getrpcport(master, YPPASSWDPROG, YPPASSWDPROC_UPDATE, IPPROTO_UDP);
  if (port==0) {
      fprintf (stderr, "%s: yppasswdd not running on NIS master host\n",
					prog_name);
      return NULL;
  }
  if (port >= IPPORT_RESERVED) {
      fprintf (stderr, "%s: yppasswd daemon running on illegal port.\n",
					prog_name);
      return NULL;
  }
  return master;
}

int
yp_passwd(char *user)
{
  struct timeval timeout;
  struct yppasswd yppasswd;
  struct passwd *pw;
  CLIENT *clnt;
  char   *master;
  char   *what;
  int    c, err, status;
  char   *s;

  if (use_yp_passwd + opt_fullname + opt_shell == 0)
      use_yp_passwd = 1;	/* default to yppasswd behavior */

  if ((master = getserver()) == NULL) {
      exit(1);
  }
  
  /* Obtain the passwd struct for the user whose password is to be changed.
   */
  uid = getuid();
  if (user == NULL) {
      if ((pw = getpwuid(uid)) == NULL) {
          fprintf ( stderr, "%s: unknown user (uid=%d).\n",
		prog_name, (int)uid );
          exit(1);
      }
  } else {
      if ((pw = getpwnam(user)) == NULL) {
          fprintf ( stderr, "%s: unknown user: %s.\n", prog_name, user );
          exit(1);
      }
      if (pw->pw_uid != uid && uid != 0) {
          fprintf ( stderr, "%s: Only root may change account information "
          		    "for others\n", prog_name );
          exit(1);
      }
  }

  /* Initialize password information */
  yppasswd.newpw.pw_passwd = pw->pw_passwd;
  yppasswd.newpw.pw_name = pw->pw_name;
  yppasswd.newpw.pw_uid = pw->pw_uid;
  yppasswd.newpw.pw_gid = pw->pw_gid;
  yppasswd.newpw.pw_gecos = pw->pw_gecos;
  yppasswd.newpw.pw_dir = pw->pw_dir;
  yppasswd.newpw.pw_shell = pw->pw_shell;
  yppasswd.oldpass = NULL;
  
  switch (use_yp_passwd + (opt_fullname << 1) + (opt_shell << 2)) {
  case 1:
      what = "YP password";
      break;
  case 2:
      what = "fullname";
      break;
  case 4:
      what = "login shell";
      break;
  default:
      what = "account information";
  }
  printf("Changing %s for %s on %s.\n",  what, pw->pw_name, master);

  /* Get old password */
  if(pw->pw_passwd) {
    char prompt[40];

    sprintf (prompt, "Please enter %spassword:", use_yp_passwd? "old " : "");
    s = getpass (prompt);
    if( strcmp(crypt(s, pw->pw_passwd), pw->pw_passwd)) {
        fprintf(stderr, "Sorry.\n");
        exit (1);
    }
    yppasswd.oldpass = strdup(s);
  }

  if (use_yp_passwd) {
      if ((s = getnewyppasswd(pw)) == NULL) 
      	  exit (1);
      yppasswd.newpw.pw_passwd = s;
  }
  if (opt_fullname) {
      if ((s = getnewfullname(pw)) == NULL) 
      	  exit (1);
      yppasswd.newpw.pw_gecos = s;
  }
  if (opt_shell) {
      if ((s = getnewshell(pw)) == NULL) 
      	  exit (1);
      yppasswd.newpw.pw_shell = s;
  }
  
  /* The yppasswd.x file said `unix authentication required',
   * so I added it. This is the only reason it is in here.
   * My yppasswdd doesn't use it, but maybe some others out there
   * do. 					--okir
   */
  clnt = clnt_create( master, YPPASSWDPROG, YPPASSWDVERS, "udp" );
  clnt->cl_auth = authunix_create_default();
  bzero( (char*)&status, sizeof(status) );
  timeout.tv_sec = 25; timeout.tv_usec = 0;
  err = clnt_call( clnt, YPPASSWDPROC_UPDATE,
  		 xdr_yppasswd, (char*)&yppasswd,
  		 xdr_int,      (char*)&status,
  		 &timeout );

  if (err) {
      clnt_perrno(err);
      fprintf( stderr, "\n" );
  } else if (status) {
      fprintf( stderr, "Error while changing %s.\n", what );
  }

  printf("\nThe %s has%s been changed on %s.\n", 
  		what, (err || status)? " not" : "", master);

  auth_destroy( clnt->cl_auth );
  clnt_destroy( clnt );
  exit ((err || status) != 0);
}
