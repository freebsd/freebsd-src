/* pam_nologin module */

/*
 * $Id: pam_nologin.c,v 1.2 2000/12/04 19:02:34 baggins Exp $
 *
 * Written by Michael K. Johnson <johnsonm@redhat.com> 1996/10/24
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pwd.h>

#include <security/_pam_macros.h>
/*
 * here, we make a definition for the externally accessible function
 * in this file (this definition is required for static a module
 * but strongly encouraged generally) it is used to instruct the
 * modules include file to define the function prototypes.
 */

#define PAM_SM_AUTH

#include <security/pam_modules.h>

/* --- authentication management functions (only) --- */

PAM_EXTERN
int pam_sm_authenticate(pam_handle_t *pamh, int flags, int argc,
                        const char **argv)
{
     int retval = PAM_SUCCESS;
     int fd;
     const char *username;
     char *mtmp=NULL;
     struct passwd *user_pwd;
     struct pam_conv *conversation;
     struct pam_message message;
     struct pam_message *pmessage = &message;
     struct pam_response *resp = NULL;
     struct stat st;

     if ((fd = open("/etc/nologin", O_RDONLY, 0)) >= 0) {
       /* root can still log in; lusers cannot */
       if ((pam_get_user(pamh, &username, NULL) != PAM_SUCCESS)
           || !username) {
         return PAM_SERVICE_ERR;
       }
       user_pwd = getpwnam(username);
       if (user_pwd && user_pwd->pw_uid == 0) {
         message.msg_style = PAM_TEXT_INFO;
       } else {
	   if (!user_pwd) {
	       retval = PAM_USER_UNKNOWN;
	   } else {
	       retval = PAM_AUTH_ERR;
	   }
	   message.msg_style = PAM_ERROR_MSG;
       }

       /* fill in message buffer with contents of /etc/nologin */
       if (fstat(fd, &st) < 0) /* give up trying to display message */
         return retval;
       message.msg = mtmp = malloc(st.st_size+1);
       /* if malloc failed... */
       if (!message.msg) return retval;
       read(fd, mtmp, st.st_size);
       mtmp[st.st_size] = '\000';

       /* Use conversation function to give user contents of /etc/nologin */
       pam_get_item(pamh, PAM_CONV, (const void **)&conversation);
       conversation->conv(1, (const struct pam_message **)&pmessage,
			  &resp, conversation->appdata_ptr);
       free(mtmp);
       if (resp)
	   _pam_drop_reply(resp, 1);
     }

     return retval;
}

PAM_EXTERN
int pam_sm_setcred(pam_handle_t *pamh, int flags, int argc,
                   const char **argv)
{
     return PAM_SUCCESS;
}


#ifdef PAM_STATIC

/* static module data */

struct pam_module _pam_nologin_modstruct = {
     "pam_nologin",
     pam_sm_authenticate,
     pam_sm_setcred,
     NULL,
     NULL,
     NULL,
     NULL,
};

#endif

/* end of module definition */
