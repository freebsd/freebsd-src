/*
 * $Id: blank.c,v 1.2 2000/12/04 19:02:33 baggins Exp $
 */

/* Andrew Morgan (morgan@parc.power.net) -- a self contained `blank'
 * application
 *
 * I am not very proud of this code.  It makes use of a possibly ill-
 * defined pamh pointer to call pam_strerror() with.  The reason that
 * I was sloppy with this is historical (pam_strerror, prior to 0.59,
 * did not require a pamh argument) and if this program is used as a
 * model for anything, I should wish that you will take this error into
 * account.
 */

#include <stdio.h>
#include <stdlib.h>

#include <security/pam_appl.h>
#include <security/pam_misc.h>

/* ------ some local (static) functions ------- */

static void bail_out(pam_handle_t *pamh, int really, int code, const char *fn)
{
     fprintf(stderr,"==> called %s()\n  got: `%s'\n", fn,
	     pam_strerror(pamh, code));
     if (really && code)
	  exit (1);
}

/* ------ some static data objects ------- */

static struct pam_conv conv = {
     misc_conv,
     NULL
};

/* ------- the application itself -------- */

int main(int argc, char **argv)
{
     pam_handle_t *pamh=NULL;
     char *username=NULL;
     int retcode;

     /* did the user call with a username as an argument ? */

     if (argc > 2) {
	  fprintf(stderr,"usage: %s [username]\n",argv[0]);
     } else if (argc == 2) {
	  username = argv[1];
     } 

     /* initialize the Linux-PAM library */
     retcode = pam_start("blank", username, &conv, &pamh);
     bail_out(pamh,1,retcode,"pam_start");

     /* test the environment stuff */
     {
#define MAXENV 15
	 const char *greek[MAXENV] = {
	     "a=alpha", "b=beta", "c=gamma", "d=delta", "e=epsilon",
	     "f=phi", "g=psi", "h=eta", "i=iota", "j=mu", "k=nu",
	     "l=zeta", "h=", "d", "k=xi"
	 };
	 char **env;
	 int i;

	 for (i=0; i<MAXENV; ++i) {
	     retcode = pam_putenv(pamh,greek[i]);
	     bail_out(pamh,0,retcode,"pam_putenv");
	 }
	 env = pam_getenvlist(pamh);
	 if (env)
	     env = pam_misc_drop_env(env);
	 else
	     fprintf(stderr,"???\n");
	 fprintf(stderr,"a test: c=[%s], j=[%s]\n"
		 , pam_getenv(pamh, "c"), pam_getenv(pamh, "j"));
     }

     /* to avoid using goto we abuse a loop here */
     for (;;) {
	  /* authenticate the user --- `0' here, could have been PAM_SILENT
	   *	| PAM_DISALLOW_NULL_AUTHTOK */

	  retcode = pam_authenticate(pamh, 0);
	  bail_out(pamh,0,retcode,"pam_authenticate");

	  /* has the user proved themself valid? */
	  if (retcode != PAM_SUCCESS) {
	       fprintf(stderr,"%s: invalid request\n",argv[0]);
	       break;
	  }

	  /* the user is valid, but should they have access at this
	     time? */

	  retcode = pam_acct_mgmt(pamh, 0); /* `0' could be as above */
	  bail_out(pamh,0,retcode,"pam_acct_mgmt");

	  if (retcode == PAM_NEW_AUTHTOK_REQD) {
	       fprintf(stderr,"Application must request new password...\n");
	       retcode = pam_chauthtok(pamh,PAM_CHANGE_EXPIRED_AUTHTOK);
	       bail_out(pamh,0,retcode,"pam_chauthtok");
	  }

	  if (retcode != PAM_SUCCESS) {
	       fprintf(stderr,"%s: invalid request\n",argv[0]);
	       break;
	  }

	  /* `0' could be as above */
	  retcode = pam_setcred(pamh, PAM_ESTABLISH_CRED);
	  bail_out(pamh,0,retcode,"pam_setcred1");

	  if (retcode != PAM_SUCCESS) {
	       fprintf(stderr,"%s: problem setting user credentials\n"
		       ,argv[0]);
	       break;
	  }

	  /* open a session for the user --- `0' could be PAM_SILENT */
	  retcode = pam_open_session(pamh,0);
	  bail_out(pamh,0,retcode,"pam_open_session");
	  if (retcode != PAM_SUCCESS) {
	       fprintf(stderr,"%s: problem opening a session\n",argv[0]);
	       break;
	  }

	  fprintf(stderr,"The user has been authenticated and `logged in'\n");

	  /* close a session for the user --- `0' could be PAM_SILENT
	   * it is possible that this pam_close_call is in another program..
	   */

	  retcode = pam_close_session(pamh,0);
	  bail_out(pamh,0,retcode,"pam_close_session");
	  if (retcode != PAM_SUCCESS) {
	       fprintf(stderr,"%s: problem closing a session\n",argv[0]);
	       break;
	  }
	  
	  retcode = pam_setcred(pamh, PAM_DELETE_CRED);
	  bail_out(pamh,0,retcode,"pam_setcred2");

	  break;                      /* don't go on for ever! */
     }

     /* close the Linux-PAM library */
     retcode = pam_end(pamh, PAM_SUCCESS);
     pamh = NULL;

     bail_out(pamh,1,retcode,"pam_end");

     exit(0);
}
