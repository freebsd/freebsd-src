/* PAM Make Home Dir module

   This module will create a users home directory if it does not exist
   when the session begins. This allows users to be present in central
   database (such as nis, kerb or ldap) without using a distributed
   file system or pre-creating a large number of directories.
   
   Here is a sample /etc/pam.d/login file for Debian GNU/Linux
   2.1:
   
   auth       requisite  pam_securetty.so
   auth       sufficient pam_ldap.so
   auth       required   pam_pwdb.so
   auth       optional   pam_group.so
   auth       optional   pam_mail.so
   account    requisite  pam_time.so
   account    sufficient pam_ldap.so
   account    required   pam_pwdb.so
   session    required   pam_mkhomedir.so skel=/etc/skel/ umask=0022
   session    required   pam_pwdb.so
   session    optional   pam_lastlog.so
   password   required   pam_pwdb.so   
   
   Released under the GNU LGPL version 2 or later
   Originally written by Jason Gunthorpe <jgg@debian.org> Feb 1999
   Structure taken from pam_lastlogin by Andrew Morgan 
     <morgan@parc.power.net> 1996
 */

/* I want snprintf dammit */
#define _GNU_SOURCE 1
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <pwd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>

/*
 * here, we make a definition for the externally accessible function
 * in this file (this definition is required for static a module
 * but strongly encouraged generally) it is used to instruct the
 * modules include file to define the function prototypes.
 */

#define PAM_SM_SESSION

#include <security/pam_modules.h>
#include <security/_pam_macros.h>

/* argument parsing */
#define MKHOMEDIR_DEBUG      020	/* keep quiet about things */
#define MKHOMEDIR_QUIET      040	/* keep quiet about things */

static unsigned int UMask = 0022;
static char SkelDir[BUFSIZ] = "/etc/skel";

/* some syslogging */
static void _log_err(int err, const char *format, ...)
{
    va_list args;

    va_start(args, format);
    openlog("PAM-mkhomedir", LOG_CONS|LOG_PID, LOG_AUTH);
    vsyslog(err, format, args);
    va_end(args);
    closelog();
}

static int _pam_parse(int flags, int argc, const char **argv)
{
   int ctrl = 0;

   /* does the appliction require quiet? */
   if ((flags & PAM_SILENT) == PAM_SILENT)
      ctrl |= MKHOMEDIR_QUIET;

   /* step through arguments */
   for (; argc-- > 0; ++argv)
   {
      if (!strcmp(*argv, "silent"))
      {
	 ctrl |= MKHOMEDIR_QUIET;
      } 
      else if (!strncmp(*argv,"umask=",6))
	 UMask = strtol(*argv+6,0,0);
      else if (!strncmp(*argv,"skel=",5))
	 strcpy(SkelDir,*argv+5);
      else
      {
	 _log_err(LOG_ERR, "unknown option; %s", *argv);
      }
   }

   D(("ctrl = %o", ctrl));
   return ctrl;
}

/* This common function is used to send a message to the applications 
   conversion function. Our only use is to ask the application to print 
   an informative message that we are creating a home directory */
static int converse(pam_handle_t * pamh, int ctrl, int nargs
		    ,struct pam_message **message
		    ,struct pam_response **response)
{
   int retval;
   struct pam_conv *conv;

   D(("begin to converse"));

   retval = pam_get_item(pamh, PAM_CONV, (const void **) &conv);
   if (retval == PAM_SUCCESS)
   {

      retval = conv->conv(nargs, (const struct pam_message **) message
			  ,response, conv->appdata_ptr);

      D(("returned from application's conversation function"));

      if (retval != PAM_SUCCESS && (ctrl & MKHOMEDIR_DEBUG))
      {
	 _log_err(LOG_DEBUG, "conversation failure [%s]"
		  ,pam_strerror(pamh, retval));
      }

   }
   else
   {
      _log_err(LOG_ERR, "couldn't obtain coversation function [%s]"
	       ,pam_strerror(pamh, retval));
   }

   D(("ready to return from module conversation"));

   return retval;		/* propagate error status */
}

/* Ask the application to display a short text string for us. */
static int make_remark(pam_handle_t * pamh, int ctrl, const char *remark)
{
   int retval;

   if ((ctrl & MKHOMEDIR_QUIET) != MKHOMEDIR_QUIET)
   {
      struct pam_message msg[1], *mesg[1];
      struct pam_response *resp = NULL;

      mesg[0] = &msg[0];
      msg[0].msg_style = PAM_TEXT_INFO;
      msg[0].msg = remark;

      retval = converse(pamh, ctrl, 1, mesg, &resp);

      msg[0].msg = NULL;
      if (resp)
      {
	 _pam_drop_reply(resp, 1);
      }
   }
   else
   {
      D(("keeping quiet"));
      retval = PAM_SUCCESS;
   }

   D(("returning %s", pam_strerror(pamh, retval)));
   return retval;
}

/* Do the actual work of creating a home dir */
static int create_homedir(pam_handle_t * pamh, int ctrl,
			 const struct passwd *pwd)
{
   char *remark;
   DIR *D;
   struct dirent *Dir;
   
   /* Some scratch space */
   remark = malloc(BUFSIZ);
   if (remark == NULL)
   {
      D(("no memory for last login remark"));
      return PAM_BUF_ERR;
   }

   /* Mention what is happening, if the notification fails that is OK */
   if (snprintf(remark,BUFSIZ,"Creating home directory '%s'.",
	    pwd->pw_dir) == -1)
      return PAM_PERM_DENIED;
   
   make_remark(pamh, ctrl, remark);

   /* Crete the home directory */
   if (mkdir(pwd->pw_dir,0700) != 0)
   {
      free(remark);
      _log_err(LOG_DEBUG, "unable to create home directory %s",pwd->pw_dir);
      return PAM_PERM_DENIED;
   }   
   if (chmod(pwd->pw_dir,0777 & (~UMask)) != 0 ||
       chown(pwd->pw_dir,pwd->pw_uid,pwd->pw_gid) != 0)
   {
      free(remark);
      _log_err(LOG_DEBUG, "unable to chance perms on home directory %s",pwd->pw_dir);
      return PAM_PERM_DENIED;
   }   
   
   /* See if we need to copy the skel dir over. */
   if (SkelDir[0] == 0)
   {
      free(remark);
      return PAM_SUCCESS;
   }

   /* Scan the directory */
   D = opendir(SkelDir);
   if (D == 0)
   {
      free(remark);
      _log_err(LOG_DEBUG, "unable to read directory %s",SkelDir);
      return PAM_PERM_DENIED;
   }
   
   for (Dir = readdir(D); Dir != 0; Dir = readdir(D))
   {  
      int SrcFd;
      int DestFd;
      int Res;
      struct stat St;
      
      /* Skip some files.. */
      if (strcmp(Dir->d_name,".") == 0 ||
	  strcmp(Dir->d_name,"..") == 0)
	 continue;
      
      /* Check if it is a directory */
      snprintf(remark,BUFSIZ,"%s/%s",SkelDir,Dir->d_name);
      if (stat(remark,&St) != 0)
        continue;
      if (S_ISDIR(St.st_mode))
      {
	snprintf(remark,BUFSIZ,"%s/%s",pwd->pw_dir,Dir->d_name);
	if (mkdir(remark,(St.st_mode | 0222) & (~UMask)) != 0 ||
	    chmod(remark,(St.st_mode | 0222) & (~UMask)) != 0 ||
	    chown(remark,pwd->pw_uid,pwd->pw_gid) != 0)
	{
	   free(remark);
	   _log_err(LOG_DEBUG, "unable to change perms on copy %s",remark);
	   return PAM_PERM_DENIED;
	}
	continue;
      }

      /* Open the source file */
      if ((SrcFd = open(remark,O_RDONLY)) < 0 || fstat(SrcFd,&St) != 0)
      {
	 free(remark);
	 _log_err(LOG_DEBUG, "unable to open src file %s",remark);
	 return PAM_PERM_DENIED;
      }
      stat(remark,&St);
      
      /* Open the dest file */
      snprintf(remark,BUFSIZ,"%s/%s",pwd->pw_dir,Dir->d_name);
      if ((DestFd = open(remark,O_WRONLY | O_TRUNC | O_CREAT,0600)) < 0)
      {
	 close(SrcFd);
	 free(remark);
	 _log_err(LOG_DEBUG, "unable to open dest file %s",remark);
	 return PAM_PERM_DENIED;
      }

      /* Set the proper ownership and permissions for the module. We make
       	 the file a+w and then mask it with the set mask. This preseves
       	 execute bits */
      if (fchmod(DestFd,(St.st_mode | 0222) & (~UMask)) != 0 ||
	  fchown(DestFd,pwd->pw_uid,pwd->pw_gid) != 0)
      {
	 free(remark);
	 _log_err(LOG_DEBUG, "unable to chang perms on copy %s",remark);
	 return PAM_PERM_DENIED;
      }   
      
      /* Copy the file */
      do
      {
	 Res = read(SrcFd,remark,BUFSIZ);
	 if (Res < 0 || write(DestFd,remark,Res) != Res)
	 {
	    close(SrcFd);
	    close(DestFd);
	    free(remark);
	    _log_err(LOG_DEBUG, "unable to perform IO");
	    return PAM_PERM_DENIED;
	 }
      }
      while (Res != 0);
      close(SrcFd);
      close(DestFd);
   }
   
   free(remark);
   return PAM_SUCCESS;
}

/* --- authentication management functions (only) --- */

PAM_EXTERN
int pam_sm_open_session(pam_handle_t * pamh, int flags, int argc
			,const char **argv)
{
   int retval, ctrl;
   const char *user;
   const struct passwd *pwd;
   struct stat St;
      
   /* Parse the flag values */
   ctrl = _pam_parse(flags, argc, argv);

   /* Determine the user name so we can get the home directory */
   retval = pam_get_item(pamh, PAM_USER, (const void **) &user);
   if (retval != PAM_SUCCESS || user == NULL || *user == '\0')
   {
      _log_err(LOG_NOTICE, "user unknown");
      return PAM_USER_UNKNOWN;
   }

   /* Get the password entry */
   pwd = getpwnam(user);
   if (pwd == NULL)
   {
      D(("couldn't identify user %s", user));
      return PAM_CRED_INSUFFICIENT;
   }

   /* Stat the home directory, if something exists then we assume it is
      correct and return a success*/
   if (stat(pwd->pw_dir,&St) == 0)
      return PAM_SUCCESS;

   return create_homedir(pamh,ctrl,pwd);
}

/* Ignore */
PAM_EXTERN 
int pam_sm_close_session(pam_handle_t * pamh, int flags, int argc
			 ,const char **argv)
{
   return PAM_SUCCESS;
}

#ifdef PAM_STATIC

/* static module data */
struct pam_module _pam_mkhomedir_modstruct =
{
   "pam_mkhomedir",
   NULL,
   NULL,
   NULL,
   pam_sm_open_session,
   pam_sm_close_session,
   NULL,
};

#endif
