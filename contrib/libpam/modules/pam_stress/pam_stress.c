/* pam_stress module */

/* $Id: pam_stress.c,v 1.2 2000/11/19 23:54:05 agmorgan Exp $
 *
 * created by Andrew Morgan <morgan@linux.kernel.org> 1996/3/12
 */

#include <security/_pam_aconf.h>

#include <stdlib.h>
#include <stdio.h>

#define __USE_BSD
#include <syslog.h>

#include <stdarg.h>
#include <string.h>
#include <unistd.h>

/*
 * here, we make definitions for the externally accessible functions
 * in this file (these definitions are required for static modules
 * but strongly encouraged generally) they are used to instruct the
 * modules include file to define their prototypes.
 */

#define PAM_SM_AUTH
#define PAM_SM_ACCOUNT
#define PAM_SM_SESSION
#define PAM_SM_PASSWORD

#include <security/pam_modules.h>
#include <security/_pam_macros.h>

static char *_strdup(const char *x)
{
     char *new;
     new = malloc(strlen(x)+1);
     strcpy(new,x);
     return new;
}

/* log errors */

static void _pam_log(int err, const char *format, ...)
{
    va_list args;

    va_start(args, format);
    openlog("PAM-stress", LOG_CONS|LOG_PID, LOG_AUTH);
    vsyslog(err, format, args);
    va_end(args);
    closelog();
}

/* ---------- */

/* an internal function to turn all possible test arguments into bits
   of a ctrl number */

/* generic options */

#define PAM_ST_DEBUG         01
#define PAM_ST_NO_WARN       02
#define PAM_ST_USE_PASS1     04
#define PAM_ST_TRY_PASS1    010
#define PAM_ST_ROOTOK       020

/* simulation options */

#define PAM_ST_EXPIRED       040
#define PAM_ST_FAIL_1       0100
#define PAM_ST_FAIL_2       0200
#define PAM_ST_PRELIM       0400
#define PAM_ST_REQUIRE_PWD 01000

/* some syslogging */

static void _pam_report(int ctrl, const char *name, int flags,
		 int argc, const char **argv)
{
     if (ctrl & PAM_ST_DEBUG) {
	  _pam_log(LOG_DEBUG, "CALLED: %s", name);
	  _pam_log(LOG_DEBUG, "FLAGS : 0%o%s", flags,
		   (flags & PAM_SILENT) ? " (silent)":"");
 	  _pam_log(LOG_DEBUG, "CTRL  = 0%o",ctrl);
	  _pam_log(LOG_DEBUG, "ARGV  :");
	  while (argc--) {
	       _pam_log(LOG_DEBUG, " \"%s\"", *argv++);
	  }
     }
}

static int _pam_parse(int argc, const char **argv)
{
     int ctrl=0;

     /* step through arguments */
     for (ctrl=0; argc-- > 0; ++argv) {

	  /* generic options */

	  if (!strcmp(*argv,"debug"))
	       ctrl |= PAM_ST_DEBUG;
	  else if (!strcmp(*argv,"no_warn"))
	       ctrl |= PAM_ST_NO_WARN;
	  else if (!strcmp(*argv,"use_first_pass"))
	       ctrl |= PAM_ST_USE_PASS1;
	  else if (!strcmp(*argv,"try_first_pass"))
	       ctrl |= PAM_ST_TRY_PASS1;
	  else if (!strcmp(*argv,"rootok"))
	       ctrl |= PAM_ST_ROOTOK;

	  /* simulation options */

	  else if (!strcmp(*argv,"expired"))   /* signal password needs
						  renewal */
	       ctrl |= PAM_ST_EXPIRED;
	  else if (!strcmp(*argv,"fail_1"))    /* instruct fn 1 to fail */
	       ctrl |= PAM_ST_FAIL_1;
	  else if (!strcmp(*argv,"fail_2"))    /* instruct fn 2 to fail */
	       ctrl |= PAM_ST_FAIL_2;
	  else if (!strcmp(*argv,"prelim"))    /* instruct pam_sm_setcred
						  to fail on first call */
	       ctrl |= PAM_ST_PRELIM;
	  else if (!strcmp(*argv,"required"))  /* module is fussy about the
						  user being authenticated */
	       ctrl |= PAM_ST_REQUIRE_PWD;

	  else {
	       _pam_log(LOG_ERR,"pam_parse: unknown option; %s",*argv);
	  }
     }

     return ctrl;
}

static int converse(pam_handle_t *pamh, int nargs
		    , struct pam_message **message
		    , struct pam_response **response)
{
     int retval;
     struct pam_conv *conv;

     if ((retval = pam_get_item(pamh,PAM_CONV,(const void **)&conv))
	 == PAM_SUCCESS) {
	  retval = conv->conv(nargs, (const struct pam_message **) message
			      , response, conv->appdata_ptr);
	  if (retval != PAM_SUCCESS) {
	       _pam_log(LOG_ERR,"(pam_stress) converse returned %d",retval);
	       _pam_log(LOG_ERR,"that is: %s",pam_strerror(pamh, retval));
	  }
     } else {
	  _pam_log(LOG_ERR,"(pam_stress) converse failed to get pam_conv");
     }

     return retval;
}

/* authentication management functions */

static int stress_get_password(pam_handle_t *pamh, int flags
			       , int ctrl, char **password)
{
     char *pass;

     if ( (ctrl & (PAM_ST_TRY_PASS1|PAM_ST_USE_PASS1))
	 && (pam_get_item(pamh,PAM_AUTHTOK,(const void **)&pass)
	     == PAM_SUCCESS)
	 && (pass != NULL) ) {
	  pass = _strdup(pass);
     } else if ((ctrl & PAM_ST_USE_PASS1)) {
	  _pam_log(LOG_WARNING, "pam_stress: no forwarded password");
	  return PAM_PERM_DENIED;
     } else {                                /* we will have to get one */
	  struct pam_message msg[1],*pmsg[1];
	  struct pam_response *resp;
	  int retval;

	  /* set up conversation call */

	  pmsg[0] = &msg[0];
	  msg[0].msg_style = PAM_PROMPT_ECHO_OFF;
	  msg[0].msg = "STRESS Password: ";
	  resp = NULL;

	  if ((retval = converse(pamh,1,pmsg,&resp)) != PAM_SUCCESS) {
	       return retval;
	  }

	  if (resp) {
	       if ((resp[0].resp == NULL) && (ctrl & PAM_ST_DEBUG)) {
		    _pam_log(LOG_DEBUG,
			     "pam_sm_authenticate: NULL authtok given");
	       }
	       if ((flags & PAM_DISALLOW_NULL_AUTHTOK)
		   && resp[0].resp == NULL) {
		    free(resp);
		    return PAM_AUTH_ERR;
	       }

	       pass = resp[0].resp;          /* remember this! */

	       resp[0].resp = NULL;
	  } else if (ctrl & PAM_ST_DEBUG) {
	       _pam_log(LOG_DEBUG,"pam_sm_authenticate: no error reported");
	       _pam_log(LOG_DEBUG,"getting password, but NULL returned!?");
	       return PAM_CONV_ERR;
	  }
	  free(resp);
     }

     *password = pass;             /* this *MUST* be free()'d by this module */

     return PAM_SUCCESS;
}

/* function to clean up data items */

static void wipe_up(pam_handle_t *pamh, void *data, int error)
{
     free(data);
}

PAM_EXTERN
int pam_sm_authenticate(pam_handle_t *pamh, int flags,
			int argc, const char **argv)
{
     const char *username;
     int retval=PAM_SUCCESS;
     char *pass;
     int ctrl;

     D(("called."));

     ctrl = _pam_parse(argc,argv);
     _pam_report(ctrl, "pam_sm_authenticate", flags, argc, argv);

     /* try to get the username */

     retval = pam_get_user(pamh, &username, "username: ");
     if ((ctrl & PAM_ST_DEBUG) && (retval == PAM_SUCCESS)) {
	  _pam_log(LOG_DEBUG, "pam_sm_authenticate: username = %s", username);
     } else if (retval != PAM_SUCCESS) {
	  _pam_log(LOG_WARNING, "pam_sm_authenticate: failed to get username");
	  return retval;
     }

     /* now get the password */

     retval = stress_get_password(pamh,flags,ctrl,&pass);
     if (retval != PAM_SUCCESS) {
	  _pam_log(LOG_WARNING, "pam_sm_authenticate: "
		   "failed to get a password");
	  return retval;
     }

     /* try to set password item */

     retval = pam_set_item(pamh,PAM_AUTHTOK,pass);
     if (retval != PAM_SUCCESS) {
	  _pam_log(LOG_WARNING, "pam_sm_authenticate: "
		   "failed to store new password");
	  _pam_overwrite(pass);
	  free(pass);
	  return retval;
     }

     /* clean up local copy of password */

     _pam_overwrite(pass);
     free(pass);
     pass = NULL;

     /* if we are debugging then we print the password */

     if (ctrl & PAM_ST_DEBUG) {
	  (void) pam_get_item(pamh,PAM_AUTHTOK,(const void **)&pass);
	  _pam_log(LOG_DEBUG,
		   "pam_st_authenticate: password entered is: [%s]\n",pass);
	  pass = NULL;
     }

     /* if we signal a fail for this function then fail */

     if ((ctrl & PAM_ST_FAIL_1) && retval == PAM_SUCCESS)
	  return PAM_PERM_DENIED;

     return retval;
}

PAM_EXTERN
int pam_sm_setcred(pam_handle_t *pamh, int flags,
		   int argc, const char **argv)
{
     int ctrl = _pam_parse(argc,argv);

     D(("called. [post parsing]"));

     _pam_report(ctrl, "pam_sm_setcred", flags, argc, argv);

     if (ctrl & PAM_ST_FAIL_2)
	  return PAM_CRED_ERR;

     return PAM_SUCCESS;
}

/* account management functions */

PAM_EXTERN
int pam_sm_acct_mgmt(pam_handle_t *pamh, int flags,
		     int argc, const char **argv)
{
     int ctrl = _pam_parse(argc,argv);

     D(("called. [post parsing]"));

     _pam_report(ctrl,"pam_sm_acct_mgmt", flags, argc, argv);

     if (ctrl & PAM_ST_FAIL_1)
	  return PAM_PERM_DENIED;
     else if (ctrl & PAM_ST_EXPIRED) {
	  void *text = malloc(sizeof("yes")+1);
	  strcpy(text,"yes");
	  pam_set_data(pamh,"stress_new_pwd",text,wipe_up);
	  if (ctrl & PAM_ST_DEBUG) {
	       _pam_log(LOG_DEBUG,"pam_sm_acct_mgmt: need a new password");
	  }
	  return PAM_NEW_AUTHTOK_REQD;
     }

     return PAM_SUCCESS;
}

PAM_EXTERN
int pam_sm_open_session(pam_handle_t *pamh, int flags,
			int argc, const char **argv)
{
     char *username,*service;
     int ctrl = _pam_parse(argc,argv);

     D(("called. [post parsing]"));

     _pam_report(ctrl,"pam_sm_open_session", flags, argc, argv);

     if ((pam_get_item(pamh, PAM_USER, (const void **) &username)
	  != PAM_SUCCESS)
	 || (pam_get_item(pamh, PAM_SERVICE, (const void **) &service)
	     != PAM_SUCCESS)) {
	  _pam_log(LOG_WARNING,"pam_sm_open_session: for whom?");
	  return PAM_SESSION_ERR;
     }

     _pam_log(LOG_NOTICE,"pam_stress: opened [%s] session for user [%s]"
	      , service, username);

     if (ctrl & PAM_ST_FAIL_1)
	  return PAM_SESSION_ERR;

     return PAM_SUCCESS;
}

PAM_EXTERN
int pam_sm_close_session(pam_handle_t *pamh, int flags,
			 int argc, const char **argv)
{
     const char *username,*service;
     int ctrl = _pam_parse(argc,argv);

     D(("called. [post parsing]"));

     _pam_report(ctrl,"pam_sm_close_session", flags, argc, argv);

     if ((pam_get_item(pamh, PAM_USER, (const void **)&username)
	  != PAM_SUCCESS)
	 || (pam_get_item(pamh, PAM_SERVICE, (const void **)&service)
	     != PAM_SUCCESS)) {
	  _pam_log(LOG_WARNING,"pam_sm_close_session: for whom?");
	  return PAM_SESSION_ERR;
     }

     _pam_log(LOG_NOTICE,"pam_stress: closed [%s] session for user [%s]"
	      , service, username);

     if (ctrl & PAM_ST_FAIL_2)
	  return PAM_SESSION_ERR;

     return PAM_SUCCESS;
}

PAM_EXTERN
int pam_sm_chauthtok(pam_handle_t *pamh, int flags,
		     int argc, const char **argv)
{
     int retval;
     int ctrl = _pam_parse(argc,argv);

     D(("called. [post parsing]"));

     _pam_report(ctrl,"pam_sm_chauthtok", flags, argc, argv);

     /* this function should be called twice by the Linux-PAM library */

     if (flags & PAM_PRELIM_CHECK) {           /* first call */
	  if (ctrl & PAM_ST_DEBUG) {
	       _pam_log(LOG_DEBUG,"pam_sm_chauthtok: prelim check");
	  }
	  if (ctrl & PAM_ST_PRELIM)
	       return PAM_TRY_AGAIN;

	  return PAM_SUCCESS;
     } else if (flags & PAM_UPDATE_AUTHTOK) {  /* second call */
	  struct pam_message msg[3],*pmsg[3];
	  struct pam_response *resp;
	  const char *text;
	  char *txt=NULL;
	  int i;

	  if (ctrl & PAM_ST_DEBUG) {
	       _pam_log(LOG_DEBUG,"pam_sm_chauthtok: alter password");
	  }

	  if (ctrl & PAM_ST_FAIL_1)
	       return PAM_AUTHTOK_LOCK_BUSY;

	  if ( !(ctrl && PAM_ST_EXPIRED)
	       && (flags & PAM_CHANGE_EXPIRED_AUTHTOK)
	       && (pam_get_data(pamh,"stress_new_pwd",(const void **)&text)
		      != PAM_SUCCESS || strcmp(text,"yes"))) {
	       return PAM_SUCCESS;          /* the token has not expired */
	  }

	  /* the password should be changed */

	  if ((ctrl & PAM_ST_REQUIRE_PWD)
	      && !(getuid() == 0 && (ctrl & PAM_ST_ROOTOK))
	       ) {                       /* first get old one? */
	       char *pass;

	       if (ctrl & PAM_ST_DEBUG) {
		    _pam_log(LOG_DEBUG
			     ,"pam_sm_chauthtok: getting old password");
	       }
	       retval = stress_get_password(pamh,flags,ctrl,&pass);
	       if (retval != PAM_SUCCESS) {
		    _pam_log(LOG_DEBUG
			     ,"pam_sm_chauthtok: no password obtained");
		    return retval;
	       }
	       retval = pam_set_item(pamh, PAM_OLDAUTHTOK, pass);
	       if (retval != PAM_SUCCESS) {
		    _pam_log(LOG_DEBUG
			     ,"pam_sm_chauthtok: could not set OLDAUTHTOK");
		    _pam_overwrite(pass);
		    free(pass);
		    return retval;
	       }
	       _pam_overwrite(pass);
	       free(pass);
	  }

	  /* set up for conversation */

	  if (!(flags & PAM_SILENT)) {
	       char *username;

	       if ( pam_get_item(pamh, PAM_USER, (const void **)&username)
		    || username == NULL ) {
		    _pam_log(LOG_ERR,"no username set");
		    return PAM_USER_UNKNOWN;
	       }
	       pmsg[0] = &msg[0];
	       msg[0].msg_style = PAM_TEXT_INFO;
#define _LOCAL_STRESS_COMMENT "Changing STRESS password for "
	       txt = (char *) malloc(sizeof(_LOCAL_STRESS_COMMENT)
				     +strlen(username)+1);
	       strcpy(txt, _LOCAL_STRESS_COMMENT);
#undef _LOCAL_STRESS_COMMENT
	       strcat(txt, username);
	       msg[0].msg = txt;
	       i = 1;
	  } else {
	       i = 0;
	  }

	  pmsg[i] = &msg[i];
	  msg[i].msg_style = PAM_PROMPT_ECHO_OFF;
	  msg[i++].msg = "Enter new STRESS password: ";
	  pmsg[i] = &msg[i];
	  msg[i].msg_style = PAM_PROMPT_ECHO_OFF;
	  msg[i++].msg = "Retype new STRESS password: ";
	  resp = NULL;

	  retval = converse(pamh,i,pmsg,&resp);
	  if (txt) {
	       free(txt);
	       txt = NULL;               /* clean up */
	  }
	  if (retval != PAM_SUCCESS) {
	       return retval;
	  }

	  if (resp == NULL) {
	       _pam_log(LOG_ERR, "pam_sm_chauthtok: no response from conv");
	       return PAM_CONV_ERR;
	  }

	  /* store the password */

	  if (resp[i-2].resp && resp[i-1].resp) {
	       if (strcmp(resp[i-2].resp,resp[i-1].resp)) {
		    /* passwords are not the same; forget and return error */

		    _pam_drop_reply(resp, i);

		    if (!(flags & PAM_SILENT) && !(ctrl & PAM_ST_NO_WARN)) {
			 pmsg[0] = &msg[0];
			 msg[0].msg_style = PAM_ERROR_MSG;
			 msg[0].msg = "Verification mis-typed; "
			      "password unchaged";
			 resp = NULL;
			 (void) converse(pamh,1,pmsg,&resp);
			 if (resp) {
			     _pam_drop_reply(resp, 1);
			 }
		    }
		    return PAM_AUTHTOK_ERR;
	       }

	       if (pam_get_item(pamh,PAM_AUTHTOK,(const void **)&text)
		   == PAM_SUCCESS) {
		    (void) pam_set_item(pamh,PAM_OLDAUTHTOK,text);
		    text = NULL;
	       }
	       (void) pam_set_item(pamh,PAM_AUTHTOK,resp[0].resp);
	  } else {
	       _pam_log(LOG_DEBUG,"pam_sm_chauthtok: problem with resp");
	       retval = PAM_SYSTEM_ERR;
	  }

	  _pam_drop_reply(resp, i);      /* clean up the passwords */
     } else {
	  _pam_log(LOG_ERR,"pam_sm_chauthtok: this must be a Linux-PAM error");
	  return PAM_SYSTEM_ERR;
     }

     return retval;
}


#ifdef PAM_STATIC

/* static module data */

struct pam_module _pam_stress_modstruct = {
    "pam_stress",
    pam_sm_authenticate,
    pam_sm_setcred,
    pam_sm_acct_mgmt,
    pam_sm_open_session,
    pam_sm_close_session,
    pam_sm_chauthtok
};

#endif
