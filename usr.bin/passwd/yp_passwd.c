/*
 * Copyright (c) 1992/3 Theo de Raadt <deraadt@fsa.ca>
 * Copyright (c) 1994 Olaf Kirch <okir@monad.swb.de>
 * Copyright (c) 1995 Bill Paul <wpaul@ctr.columbia.edu>
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

#ifdef YP
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
#include <pw_yp.h>

extern char *prog_name;
uid_t	uid;

extern char *getnewpasswd __P(( struct passwd * , int ));

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
  int    err, status;
  char   *s;

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

  /* Use the correct password */
  pw = (struct passwd *)&yp_password;

  /* Initialize password information */
  yppasswd.newpw.pw_passwd = pw->pw_passwd;
  yppasswd.newpw.pw_name = pw->pw_name;
  yppasswd.newpw.pw_uid = pw->pw_uid;
  yppasswd.newpw.pw_gid = pw->pw_gid;
  yppasswd.newpw.pw_gecos = pw->pw_gecos;
  yppasswd.newpw.pw_dir = pw->pw_dir;
  yppasswd.newpw.pw_shell = pw->pw_shell;
  yppasswd.oldpass = NULL;

  printf("Changing NIS password for %s on %s.\n", pw->pw_name, master);

  /* Get old password */
  if(pw->pw_passwd[0]) {

    s = getpass ("Old password: ");
    if( strcmp(crypt(s, pw->pw_passwd), pw->pw_passwd)) {
        fprintf(stderr, "Sorry.\n");
        exit (1);
    }
    yppasswd.oldpass = strdup(s);
  } else
    yppasswd.oldpass = "";

  if ((s = getnewpasswd(pw, 1)) == NULL)
	exit (1);
  yppasswd.newpw.pw_passwd = s;

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
      fprintf( stderr, "Error while changing NIS password.\n");
  }

  printf("\nNIS password has%s been changed on %s.\n",
  		(err || status)? " not" : "", master);

  auth_destroy( clnt->cl_auth );
  clnt_destroy( clnt );
  exit ((err || status) != 0);
}
#endif /* YP */
