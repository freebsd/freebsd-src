
/* Main coding by Elliot Lee <sopwith@redhat.com>, Red Hat Software. 
   Copyright (C) 1996. */

/*
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, and the entire permission notice in its entirety,
 *    including the disclaimer of warranties.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior
 *    written permission.
 * 
 * ALTERNATIVELY, this product may be distributed under the terms of
 * the GNU Public License, in which case the provisions of the GPL are
 * required INSTEAD OF the above restrictions.  (This clause is
 * necessary due to a potential bad interaction between the GPL and
 * the restrictions contained in a BSD-style copyright.)
 * 
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
  How it works:
     Gets in username (has to be done) from the calling program
     Does authentication of user (only if we are not running as root)
     Gets new password/checks for sanity
     Sets it.
 */

#define PAM_SM_PASSWORD

/* #define DEBUG 1 */

#include <stdio.h>
#include <sys/time.h>
#define _BSD_SOURCE
#define _SVID_SOURCE
#include <errno.h>
#define __USE_BSD
#define _BSD_SOURCE
#include <pwd.h>
#include <sys/types.h>

/* why not defined? */
void setpwent(void);
void endpwent(void);
int chmod(const char *path, mode_t mode);
struct passwd *fgetpwent(FILE *stream);
int putpwent(const struct passwd *p, FILE *stream);

#include <unistd.h>
char *crypt(const char *key, const char *salt);
#ifdef USE_CRACKLIB
#include <crack.h>
#endif
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <syslog.h>
#include <string.h>
#include <stdarg.h>
#include <malloc.h>
#include <security/_pam_macros.h>

#ifndef LINUX    /* AGM added this as of 0.2 */
#include <security/pam_appl.h>
#endif           /* ditto */
#include <security/pam_modules.h>
#ifdef HAVE_SHADOW_H
#include <shadow.h>
#endif

#define MAX_PASSWD_TRIES 3
#define OLD_PASSWORD_PROMPT "Password: "
#define NEW_PASSWORD_PROMPT "New password: "
#define AGAIN_PASSWORD_PROMPT "New password (again): "
#define PW_TMPFILE "/etc/npasswd"
#define SH_TMPFILE "/etc/nshadow"
#define CRACKLIB_DICTS "/usr/lib/cracklib_dict"

/* Various flags for the getpass routine to send back in... */
#define PPW_EXPIRED 1
#define PPW_EXPIRING 2
#define PPW_WILLEXPIRE 4
#define PPW_NOSUCHUSER 8
#define PPW_SHADOW 16
#define PPW_TOOEARLY 32
#define PPW_ERROR 64

#ifndef DO_TEST
#define STATIC static
#else
#define STATIC
#endif
/* Sets a password for the specified user to the specified password
   Returns flags PPW_*, or'd. */
STATIC int _do_setpass(char *forwho, char *towhat, int flags);
/* Gets a password for the specified user
   Returns flags PPW_*, or'd. */
STATIC int _do_getpass(char *forwho, char **theirpass);
/* Checks whether the password entered is same as listed in the database
   'entered' should not be crypt()'d or anything (it should be as the
   user entered it...), 'listed' should be as it is listed in the
   password database file */
STATIC int _do_checkpass(const char *entered, char *listed);

/* sends a one-way message to the user, either error or info... */
STATIC int conv_sendmsg(struct pam_conv *aconv, const char *message, int style);
/* sends a message and returns the results of the conversation */
STATIC int conv_getitem(struct pam_conv *aconv, char *message, int style,
			  char **result);

PAM_EXTERN
int pam_sm_chauthtok(	pam_handle_t *pamh, 
			int flags,
			int argc, 
			const char **argv);

static void _pam_log(int err, const char *format, ...)
{
    va_list args;

    va_start(args, format);
    openlog("PAM-unix_passwd", LOG_CONS|LOG_PID, LOG_AUTH);
    vsyslog(err, format, args);
    va_end(args);
    closelog();
}

#ifdef NEED_LCKPWDF
/* This is a hack, but until libc and glibc both include this function
 * by default (libc only includes it if nys is not being used, at the
 * moment, and glibc doesn't appear to have it at all) we need to have
 * it here, too.  :-(
 *
 * This should not become an official part of PAM.
 *
 * BEGIN_HACK
*/

/*
 * lckpwdf.c -- prevent simultaneous updates of password files
 *
 * Before modifying any of the password files, call lckpwdf().  It may block
 * for up to 15 seconds trying to get the lock.  Return value is 0 on success
 * or -1 on failure.  When you are done, call ulckpwdf() to release the lock.
 * The lock is also released automatically when the process exits.  Only one
 * process at a time may hold the lock.
 *
 * These functions are supposed to be conformant with AT&T SVID Issue 3.
 *
 * Written by Marek Michalkiewicz <marekm@i17linuxb.ists.pwr.wroc.pl>,
 * public domain.
 */

#include <fcntl.h>
#include <signal.h>

#define LOCKFILE "/etc/.pwd.lock"
#define TIMEOUT 15

static int lockfd = -1;

static int
set_close_on_exec(int fd)
{
	int flags = fcntl(fd, F_GETFD, 0);
	if (flags == -1)
		return -1;
	flags |= FD_CLOEXEC;
	return fcntl(fd, F_SETFD, flags);
}

static int
do_lock(int fd)
{
	struct flock fl;

	memset(&fl, 0, sizeof fl);
	fl.l_type = F_WRLCK;
	fl.l_whence = SEEK_SET;
	return fcntl(fd, F_SETLKW, &fl);
}

static void
alarm_catch(int sig)
{
/* does nothing, but fcntl F_SETLKW will fail with EINTR */
}

static int lckpwdf(void)
{
	struct sigaction act, oldact;
	sigset_t set, oldset;

	if (lockfd != -1)
		return -1;

	lockfd = open(LOCKFILE, O_CREAT | O_WRONLY, 0600);
	if (lockfd == -1)
		return -1;
	if (set_close_on_exec(lockfd) == -1)
		goto cleanup_fd;

	memset(&act, 0, sizeof act);
	act.sa_handler = alarm_catch;
	act.sa_flags = 0;
	sigfillset(&act.sa_mask);
	if (sigaction(SIGALRM, &act, &oldact) == -1)
		goto cleanup_fd;

	sigemptyset(&set);
	sigaddset(&set, SIGALRM);
	if (sigprocmask(SIG_UNBLOCK, &set, &oldset) == -1)
		goto cleanup_sig;

	alarm(TIMEOUT);
	if (do_lock(lockfd) == -1)
		goto cleanup_alarm;
	alarm(0);
	sigprocmask(SIG_SETMASK, &oldset, NULL);
	sigaction(SIGALRM, &oldact, NULL);
	return 0;

cleanup_alarm:
	alarm(0);
	sigprocmask(SIG_SETMASK, &oldset, NULL);
cleanup_sig:
	sigaction(SIGALRM, &oldact, NULL);
cleanup_fd:
	close(lockfd);
	lockfd = -1;
	return -1;
}

static int
ulckpwdf(void)
{
	unlink(LOCKFILE);
	if (lockfd == -1)
		return -1;

	if (close(lockfd) == -1) {
		lockfd = -1;
		return -1;
	}
	lockfd = -1;
	return 0;
}
/* END_HACK */
#endif

#define PAM_FAIL_CHECK if(retval != PAM_SUCCESS) { return retval; }

PAM_EXTERN
int pam_sm_chauthtok(pam_handle_t *pamh, int flags, int argc, const char **argv)
{
  char *usrname, *curpass, *newpass; /* pointers to the username,
					current password, and new password */

  struct pam_conv *appconv; /* conversation with the app */
  struct pam_message msg, *pmsg; /* Misc for conversations */
  struct pam_response *resp;

  int retval=0; /* Gets the return values for all our function calls */
  unsigned int pflags=0; /* Holds the flags from our getpass & setpass
			 functions */

  const char *cmiscptr; /* Utility variables, used for different purposes at
		    different times */
  char *miscptr; /* Utility variables, used for different purposes at
		    different times */
  unsigned int miscint;
  int fascist = 1; /* Be fascist by default.  If compiled with cracklib,
                      call cracklib.  Otherwise just check length... */

  char argbuf[256],argval[256];
  int i;


  retval = pam_get_item(pamh,PAM_CONV,(const void **) &appconv);
  PAM_FAIL_CHECK;

  retval = pam_get_item(pamh,PAM_USER,(const void **) &usrname);
  PAM_FAIL_CHECK;
  if(flags & PAM_PRELIM_CHECK) {
    pflags = _do_getpass(usrname,&miscptr);
    if(pflags & PPW_NOSUCHUSER)
      return PAM_USER_UNKNOWN;
    else if(pflags & ~(PPW_SHADOW|PPW_EXPIRING|PPW_WILLEXPIRE))
      return PAM_AUTHTOK_ERR;
    else
      return PAM_SUCCESS;
  } /* else... */
#ifdef DEBUG
  fprintf(stderr,"Got username of %s\n",usrname);
#endif
  if((usrname == NULL) || (strlen(usrname) < 1)) {
    /* The app is supposed to get us the username! */
    retval = PAM_USER_UNKNOWN;
    PAM_FAIL_CHECK;
  }

  for(i=0; i < argc; i++) {
     {
	 char *tmp = x_strdup(argv[i]);
	 strncpy(argbuf,strtok(tmp ,"="),255);
	 strncpy(argval,strtok(NULL,"="),255);
	 free(tmp);
     }

     /* For PC functionality use "strict" -- historically "fascist" */
     if(!strcmp(argbuf,"strict") || !strcmp(argbuf, "fascist"))

       if(!strcmp(argval,"true"))
         fascist = 1;
       else if(!strcmp(argval,"false"))
         fascist = 0;
       else
         return PAM_SERVICE_ERR;
     else {
       _pam_log(LOG_ERR,"Unknown option: %s",argbuf);
       return PAM_SERVICE_ERR;
     }
  }


  /* Now we have all the initial information we need from the app to
     set things up (we assume that getting the username succeeded...) */
  retval = pam_get_item(pamh,PAM_OLDAUTHTOK,(const void **) &curpass);
  PAM_FAIL_CHECK;
  if(getuid()) { /* If this is being run by root, we don't need to get their
		    old password.
		    note */
    /* If we haven't been given a password yet, prompt for one... */
    miscint=0;
    while((curpass == NULL) && (miscint++ < MAX_PASSWD_TRIES)) {
      pflags = _do_getpass(usrname,&miscptr);
      if(pflags & PPW_NOSUCHUSER)
	return PAM_USER_UNKNOWN; /* If the user that was passed in doesn't
				    exist, say so and exit (app passes in
				    username) */
    
      /* Get the password from the user... */
      pmsg          = &msg;
    
      msg.msg_style = PAM_PROMPT_ECHO_OFF;
      msg.msg = OLD_PASSWORD_PROMPT;
      resp = NULL;

      retval = appconv->conv(1, (const struct pam_message **) &pmsg,
			     &resp, appconv->appdata_ptr);
      
      PAM_FAIL_CHECK;
      curpass = resp->resp;
      free (resp);
      if(_do_checkpass(curpass?curpass:"",miscptr)) {
        int abortme = 0;

        /* password is incorrect... */
        if (curpass && curpass[0] == '\0') {
          /* ...and it was zero-length; user wishes to abort change */
          abortme = 1;
        }
        if (curpass) { free (curpass); }
        curpass = NULL;
        if (abortme) {
	  conv_sendmsg(appconv,"Password change aborted.",PAM_ERROR_MSG);
	  return PAM_AUTHTOK_ERR;
        }
      }
    }

    if(curpass == NULL)
      return PAM_AUTH_ERR; /* They didn't seem to enter the right password
			      for three tries - error */
    pam_set_item(pamh, PAM_OLDAUTHTOK, (void *)curpass);
  } else {
#ifdef DEBUG
    fprintf(stderr,"I am ROOT!\n");
#endif
    pflags = _do_getpass(usrname,&curpass);
    if(curpass == NULL)
      curpass = x_strdup("");
  }
  if(pflags & PPW_TOOEARLY) {
    conv_sendmsg(appconv,"You must wait longer to change your password",
		 PAM_ERROR_MSG);
    return PAM_AUTHTOK_ERR;
  }
  if(pflags & PPW_WILLEXPIRE)
    conv_sendmsg(appconv,"Your password is about to expire",PAM_TEXT_INFO);
  else if(pflags & PPW_EXPIRED)
    return PAM_ACCT_EXPIRED; /* If their account has expired, we can't auth
				them to change their password */
  if(!(pflags & PPW_EXPIRING) && (flags & PAM_CHANGE_EXPIRED_AUTHTOK))
    return PAM_SUCCESS;
  /* If we haven't been given a password yet, prompt for one... */
  miscint=0;
  pam_get_item(pamh,PAM_AUTHTOK,(const void **)&newpass);
  cmiscptr = NULL;
  while((newpass == NULL) && (miscint++ < MAX_PASSWD_TRIES)) {

    /* Get the password from the user... */
    pmsg          = &msg;
    
    msg.msg_style = PAM_PROMPT_ECHO_OFF;
    msg.msg = NEW_PASSWORD_PROMPT;
    resp = NULL;

    retval = appconv->conv(1, (const struct pam_message **) &pmsg,
			     &resp, appconv->appdata_ptr);
      
    PAM_FAIL_CHECK;
    newpass = resp->resp;
    free (resp);

#ifdef DEBUG
    if(newpass)
      fprintf(stderr,"Got password of %s\n",newpass);
    else
      fprintf(stderr,"No new password...\n");
#endif
    if (newpass[0] == '\0') { free (newpass); newpass = (char *) 0; }
    cmiscptr=NULL;
    if(newpass) {
#ifdef USE_CRACKLIB
      if(fascist && getuid()) 
        cmiscptr = FascistCheck(newpass,CRACKLIB_DICTS);
#else
      if(fascist && getuid() && strlen(newpass) < 6)
	cmiscptr = "You must choose a longer password";
#endif
      if(curpass)
	if(!strcmp(curpass,newpass)) {
	  cmiscptr="You must choose a new password.";
	  newpass=NULL;
	}
    } else {
      /* We want to abort the password change */
	conv_sendmsg(appconv,"Password change aborted",PAM_ERROR_MSG);
      return PAM_AUTHTOK_ERR;
    }
    if(!cmiscptr) {
      /* We ask them to enter their password again... */
      /* Get the password from the user... */
      pmsg          = &msg;
    
      msg.msg_style = PAM_PROMPT_ECHO_OFF;
      msg.msg = AGAIN_PASSWORD_PROMPT;
      resp = NULL;

      retval = appconv->conv(1, (const struct pam_message **) &pmsg,
			     &resp, appconv->appdata_ptr);
      
      PAM_FAIL_CHECK;
      miscptr = resp->resp;
      free (resp);
      if (miscptr[0] == '\0') { free (miscptr); miscptr = (char *) 0; }
      if(!miscptr) { /* Aborting password change... */
	conv_sendmsg(appconv,"Password change aborted",PAM_ERROR_MSG);
	return PAM_AUTHTOK_ERR;
      }
      if(!strcmp(newpass,miscptr)) {
         miscptr=NULL;
	 break;
      }
      conv_sendmsg(appconv,"You must enter the same password twice.",
		     PAM_ERROR_MSG);
      miscptr=NULL;
      newpass=NULL;
    }
    else {
      conv_sendmsg(appconv,cmiscptr,PAM_ERROR_MSG);
      newpass = NULL;
    }
  }
  if(cmiscptr) {
    /* conv_sendmsg(appconv,cmiscptr,PAM_ERROR_MSG); */
    return PAM_AUTHTOK_ERR;
  } else if(newpass == NULL)
    return PAM_AUTHTOK_ERR; /* They didn't seem to enter the right password
			      for three tries - error */
#ifdef DEBUG
	printf("Changing password for sure!\n");
#endif  
  /* From now on, we are bound and determined to get their password
     changed :-) */
  pam_set_item(pamh, PAM_AUTHTOK, (void *)newpass);
  retval = _do_setpass(usrname,newpass,pflags);
#ifdef DEBUG
    fprintf(stderr,"retval was %d\n",retval);
#endif
  if(retval & ~PPW_SHADOW) {
    conv_sendmsg(appconv,"Error: Password NOT changed",PAM_ERROR_MSG);
    return PAM_AUTHTOK_ERR;
  } else {
    conv_sendmsg(appconv,"Password changed",PAM_TEXT_INFO);
    return PAM_SUCCESS;
  }
}

/* _do_checkpass() returns 0 on success, non-0 on failure */
STATIC int _do_checkpass(const char *entered, char *listed)
{
  char salt[3];
  if ((strlen(listed) == 0) &&(strlen(entered) == 0)) {
    /* no password in database; no password entered */
    return (0);
  }
  salt[0]=listed[0]; salt[1]=listed[1]; salt[2]='\0';
  return strcmp(crypt(entered,salt),listed);
}

STATIC char mksalt(int seed) {
  int num = seed % 64;

  if (num < 26)
    return 'a' + num;
  else if (num < 52)
    return 'A' + (num - 26);
  else if (num < 62)
    return '0' + (num - 52);
  else if (num == 63)
    return '.';
  else
    return '/';
}

STATIC int _do_setpass(char *forwho, char *towhat,int flags)
{
  struct passwd *pwd=NULL, *tmpent=NULL;
  FILE *pwfile,*opwfile;
  char thesalt[3];
  int retval=0;
  struct timeval time1;
  int err = 0;
#ifdef HAVE_SHADOW_H
  struct spwd *spwdent=NULL, *stmpent=NULL;
#endif
  if(flags & PPW_SHADOW) { retval |= PPW_SHADOW; }
  gettimeofday(&time1, NULL);
  srand(time1.tv_usec);
  thesalt[0]=mksalt(rand());
  thesalt[1]=mksalt(rand());
  thesalt[2]='\0';
  
  /* lock the entire password subsystem */
#ifdef USE_LCKPWDF
  lckpwdf();
#endif
  setpwent();
  pwd = getpwnam(forwho);
#ifdef DEBUG
  printf("Got %p, for %s (salt %s)\n",pwd,
	 forwho,thesalt);
#endif
  if(pwd == NULL)
    return PPW_NOSUCHUSER;
  endpwent();

#ifdef HAVE_SHADOW_H
  if(flags & PPW_SHADOW) {
    spwdent = getspnam(forwho);
    if(spwdent == NULL)
      return PPW_NOSUCHUSER;
    spwdent->sp_pwdp = towhat;
    spwdent->sp_lstchg = time(NULL)/(60*60*24);
    pwfile = fopen(SH_TMPFILE,"w");
    opwfile = fopen("/etc/shadow","r");
    if(pwfile == NULL || opwfile == NULL)
      return PPW_ERROR;
    chown(SH_TMPFILE,0,0);
    chmod(SH_TMPFILE,0600);
    stmpent=fgetspent(opwfile);
    while(stmpent) {
      if(!strcmp(stmpent->sp_namp,forwho)) {
	stmpent->sp_pwdp = crypt(towhat,thesalt);
	stmpent->sp_lstchg = time(NULL)/(60*60*24);
#ifdef DEBUG
	fprintf(stderr,"Set password %s for %s\n",stmpent->sp_pwdp,
		forwho);
#endif
      }
      if (putspent(stmpent,pwfile)) {
	fprintf(stderr, "error writing entry to shadow file: %s\n",
		strerror(errno));
	err = 1;
        retval = PPW_ERROR;
	break;
      }
      stmpent=fgetspent(opwfile);
    }
    fclose(opwfile);

    if (fclose(pwfile)) {
	fprintf(stderr, "error writing entries to shadow file: %s\n",
		strerror(errno));
	retval = PPW_ERROR;
	err = 1;
    }

    if (!err)
      rename(SH_TMPFILE,"/etc/shadow");
    else
      unlink(SH_TMPFILE);
  } else {
    pwd->pw_passwd = towhat;
    pwfile = fopen(PW_TMPFILE,"w");
    opwfile = fopen("/etc/passwd","r");
    if(pwfile == NULL || opwfile == NULL)
      return PPW_ERROR;
    chown(PW_TMPFILE,0,0);
    chmod(PW_TMPFILE,0644);
    tmpent=fgetpwent(opwfile);
    while(tmpent) {
      if(!strcmp(tmpent->pw_name,forwho)) {
	tmpent->pw_passwd = crypt(towhat,thesalt);
      }
      if (putpwent(tmpent,pwfile)) {
	fprintf(stderr, "error writing entry to password file: %s\n",
		strerror(errno));
	err = 1;
        retval = PPW_ERROR;
	break;
      }
      tmpent=fgetpwent(opwfile);
    }
    fclose(opwfile);

    if (fclose(pwfile)) {
	fprintf(stderr, "error writing entries to password file: %s\n",
		strerror(errno));
	retval = PPW_ERROR;
	err = 1;
    }

    if (!err)
	rename(PW_TMPFILE,"/etc/passwd");
    else
	unlink(PW_TMPFILE);
  }
#else
  pwd->pw_passwd = towhat;
  pwfile = fopen(PW_TMPFILE,"w");
  opwfile = fopen("/etc/passwd","r");
  if(pwfile == NULL || opwfile == NULL)
    return PPW_ERROR;
  chown(PW_TMPFILE,0,0);
  chmod(PW_TMPFILE,0644);
  tmpent=fgetpwent(opwfile);
  while(tmpent) {
    if(!strcmp(tmpent->pw_name,forwho)) {
      tmpent->pw_passwd = crypt(towhat,thesalt);
    }
    if (putpwent(tmpent,pwfile)) {
	fprintf(stderr, "error writing entry to shadow file: %s\n",
		strerror(errno));
	err = 1;
        retval = PPW_ERROR;
	break;
    }
    tmpent=fgetpwent(opwfile);
  }
  fclose(opwfile);

  if (fclose(pwfile)) {
      fprintf(stderr, "error writing entries to password file: %s\n",
	      strerror(errno));
      retval = PPW_ERROR;
      err = 1;
  }

  if (!err)
    rename(PW_TMPFILE,"/etc/passwd");
  else
    unlink(PW_TMPFILE);
#endif
  /* unlock the entire password subsystem */
#ifdef USE_LCKPWDF
  ulckpwdf();
#endif
  return retval;
}

STATIC int _do_getpass(char *forwho, char **theirpass)
{
  struct passwd *pwd=NULL;   /* Password and shadow password */
#ifdef HAVE_SHADOW_H
  struct spwd *spwdent=NULL; /* file entries for the user */
  time_t curdays;
#endif
  int retval=0;
  /* UNIX passwords area */
  setpwent();
  pwd = getpwnam(forwho); /* Get password file entry... */
  endpwent();
  if(pwd == NULL)
    return PPW_NOSUCHUSER; /* We don't need to do the rest... */
#ifdef HAVE_SHADOW_H
  if(!strcmp(pwd->pw_passwd,"x")) {
    /* ...and shadow password file entry for this user, if shadowing
       is enabled */
    retval |= PPW_SHADOW;
    setspent();
    spwdent = getspnam(forwho);
    endspent();
    if(spwdent == NULL) 
      return PPW_NOSUCHUSER;
    *theirpass = x_strdup(spwdent->sp_pwdp);

    /* We have the user's information, now let's check if their account
     has expired (60 * 60 * 24 = number of seconds in a day) */

    /* Get the current number of days since 1970 */
    curdays = time(NULL)/(60*60*24);
    if((curdays < (spwdent->sp_lstchg + spwdent->sp_min))
	&& (spwdent->sp_min != -1))
      retval |= PPW_TOOEARLY;
    else if((curdays
	    > (spwdent->sp_lstchg + spwdent->sp_max + spwdent->sp_inact))
	&& (spwdent->sp_max != -1) && (spwdent->sp_inact != -1))
      /* Their password change has been put off too long,
	 OR their account has just plain expired */
      retval |= PPW_EXPIRED;
    else if((curdays > (spwdent->sp_lstchg + spwdent->sp_max))
	&& (spwdent->sp_max != -1))
      /* Their passwd needs to be changed */
      retval |= PPW_EXPIRING;
    else if((curdays > (spwdent->sp_lstchg
                        + spwdent->sp_max - spwdent->sp_warn))
            && (spwdent->sp_max != -1) && (spwdent->sp_warn != -1))
      retval |= PPW_WILLEXPIRE;
/*    if(spwdent->sp_lstchg < 0)
      retval &= ~(PPW_WILLEXPIRE | PPW_EXPIRING | PPW_EXPIRED);
    if(spwdent->sp_max < 0)
      retval &= ~(PPW_EXPIRING | PPW_EXPIRED); */
  } else {
    *theirpass = (char *)x_strdup(pwd->pw_passwd);
  }

#else
  *theirpass = (char *) x_strdup(pwd->pw_passwd);
#endif

  return retval;
}

STATIC int conv_sendmsg(struct pam_conv *aconv, const char *message, int style)
{
  struct pam_message msg,*pmsg;
  struct pam_response *resp;
  int retval;

  /* Get the password from the user... */
  pmsg          = &msg;

  msg.msg_style = style;
  msg.msg = message;
  resp = NULL;
  
  retval = aconv->conv(1, (const struct pam_message **) &pmsg,
		       &resp, aconv->appdata_ptr);
  if (resp) {
      _pam_drop_reply(resp, 1);
  }
  return retval;
}


STATIC int conv_getitem(struct pam_conv *aconv, char *message, int style,
			  char **result)
{
  struct pam_message msg,*pmsg;
  struct pam_response *resp;
  int retval;

  D(("called."));

  /* Get the password from the user... */
  pmsg          = &msg;
  msg.msg_style = style;
  msg.msg = message;
  resp = NULL;
  
  retval = aconv->conv(1, (const struct pam_message **) &pmsg,
		       &resp, aconv->appdata_ptr);
  if(retval != PAM_SUCCESS)
    return retval;
  if(resp != NULL) {
    *result = resp->resp; free(resp);
    return PAM_SUCCESS;
  }
  else
    return PAM_SERVICE_ERR;
}
