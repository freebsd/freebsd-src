/* pam_cracklib module */

/*
 * 0.85.  added six new options to use this with long passwords.
 * 0.8. tidied output and improved D(()) usage for debugging.
 * 0.7. added support for more obscure checks for new passwd.
 * 0.6. root can reset user passwd to any values (it's only warned)
 * 0.5. supports retries - 'retry=N' argument
 * 0.4. added argument 'type=XXX' for 'New XXX password' prompt
 * 0.3. Added argument 'debug'
 * 0.2. new password is feeded to cracklib for verify after typed once
 * 0.1. First release
 */

/*
 * Written by Cristian Gafton <gafton@redhat.com> 1996/09/10
 * Long password support by Philip W. Dalrymple <pwd@mdtsoft.com> 1997/07/18
 * See the end of the file for Copyright Information
 *
 * Modification for long password systems (>8 chars).  The original
 * module had problems when used in a md5 password system in that it
 * allowed too short passwords but required that at least half of the
 * bytes in the new password did not appear in the old one.  this
 * action is still the default and the changes should not break any
 * current user. This modification adds 6 new options, one to set the
 * number of bytes in the new password that are not in the old one,
 * the other five to control the length checking, these are all
 * documented (or will be before anyone else sees this code) in the PAM
 * S.A.G. in the section on the cracklib module.
 */

#include <security/_pam_aconf.h>

#include <stdio.h>
#ifdef HAVE_CRYPT_H
# include <crypt.h>
#endif
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>

extern char *FascistCheck(char *pw, const char *dictpath);

#ifndef CRACKLIB_DICTPATH
#define CRACKLIB_DICTPATH "/usr/share/dict/cracklib_dict"
#endif

#define PROMPT1 "New %s password: "
#define PROMPT2 "Retype new %s password: "
#define MISTYPED_PASS "Sorry, passwords do not match"

/*
 * here, we make a definition for the externally accessible function
 * in this file (this definition is required for static a module
 * but strongly encouraged generally) it is used to instruct the
 * modules include file to define the function prototypes.
 */

#define PAM_SM_PASSWORD

#include <security/pam_modules.h>
#include <security/_pam_macros.h>

#ifndef LINUX_PAM 
#include <security/pam_appl.h>
#endif  /* LINUX_PAM */

/* some syslogging */

static void _pam_log(int err, const char *format, ...)
{
    va_list args;

    va_start(args, format);
    openlog("PAM-Cracklib", LOG_CONS|LOG_PID, LOG_AUTH);
    vsyslog(err, format, args);
    va_end(args);
    closelog();
}

/* argument parsing */
#define PAM_DEBUG_ARG       0x0001

struct cracklib_options {
	int retry_times;
	int diff_ok;
	int diff_ignore;
	int min_length;
	int dig_credit;
	int up_credit;
	int low_credit;
	int oth_credit;
	int use_authtok;
	char prompt_type[BUFSIZ];
};

#define CO_RETRY_TIMES  1
#define CO_DIFF_OK      10
#define CO_DIFF_IGNORE  23
#define CO_MIN_LENGTH   9
# define CO_MIN_LENGTH_BASE 5
#define CO_DIG_CREDIT   1
#define CO_UP_CREDIT    1
#define CO_LOW_CREDIT   1
#define CO_OTH_CREDIT   1
#define CO_USE_AUTHTOK  0

static int _pam_parse(struct cracklib_options *opt, int argc, const char **argv)
{
     int ctrl=0;

     /* step through arguments */
     for (ctrl=0; argc-- > 0; ++argv) {
	 char *ep = NULL;

	 /* generic options */

	 if (!strcmp(*argv,"debug"))
	     ctrl |= PAM_DEBUG_ARG;
	 else if (!strncmp(*argv,"type=",5))
	     strcpy(opt->prompt_type, *argv+5);
	 else if (!strncmp(*argv,"retry=",6)) {
	     opt->retry_times = strtol(*argv+6,&ep,10);
	     if (!ep || (opt->retry_times < 1))
		 opt->retry_times = CO_RETRY_TIMES;
	 } else if (!strncmp(*argv,"difok=",6)) {
	     opt->diff_ok = strtol(*argv+6,&ep,10);
	     if (!ep || (opt->diff_ok < 0))
		 opt->diff_ok = CO_DIFF_OK;
	 } else if (!strncmp(*argv,"difignore=",10)) {
	     opt->diff_ignore = strtol(*argv+10,&ep,10);
	     if (!ep || (opt->diff_ignore < 0))
		 opt->diff_ignore = CO_DIFF_IGNORE;
	 } else if (!strncmp(*argv,"minlen=",7)) {
	     opt->min_length = strtol(*argv+7,&ep,10);
	     if (!ep || (opt->min_length < CO_MIN_LENGTH_BASE))
		 opt->min_length = CO_MIN_LENGTH_BASE;
	 } else if (!strncmp(*argv,"dcredit=",8)) {
	     opt->dig_credit = strtol(*argv+8,&ep,10);
	     if (!ep || (opt->dig_credit < 0))
		 opt->dig_credit = 0;
	 } else if (!strncmp(*argv,"ucredit=",8)) {
	     opt->up_credit = strtol(*argv+8,&ep,10);
	     if (!ep || (opt->up_credit < 0))
		 opt->up_credit = 0;
	 } else if (!strncmp(*argv,"lcredit=",8)) {
	     opt->low_credit = strtol(*argv+8,&ep,10);
	     if (!ep || (opt->low_credit < 0))
		 opt->low_credit = 0;
	 } else if (!strncmp(*argv,"ocredit=",8)) {
	     opt->oth_credit = strtol(*argv+8,&ep,10);
	     if (!ep || (opt->oth_credit < 0))
		 opt->oth_credit = 0;
	 } else if (!strncmp(*argv,"use_authtok",11)) {
		 opt->use_authtok = 1;
	 } else {
	     _pam_log(LOG_ERR,"pam_parse: unknown option; %s",*argv);
	 }
     }

     return ctrl;
}

/* Helper functions */

/* this is a front-end for module-application conversations */
static int converse(pam_handle_t *pamh, int ctrl, int nargs,
                    struct pam_message **message,
                    struct pam_response **response)
{
    int retval;
    struct pam_conv *conv;

    retval = pam_get_item(pamh, PAM_CONV, (const void **) &conv); 

    if ( retval == PAM_SUCCESS ) {
        retval = conv->conv(nargs, (const struct pam_message **)message,
			                response, conv->appdata_ptr);
        if (retval != PAM_SUCCESS && (ctrl && PAM_DEBUG_ARG)) {
            _pam_log(LOG_DEBUG, "conversation failure [%s]",
                                 pam_strerror(pamh, retval));
        }
    } else {
        _pam_log(LOG_ERR, "couldn't obtain coversation function [%s]",
                pam_strerror(pamh, retval));
    }

    return retval;                  /* propagate error status */
}

static int make_remark(pam_handle_t *pamh, unsigned int ctrl,
                       int type, const char *text)
{
    struct pam_message *pmsg[1], msg[1];
    struct pam_response *resp;
    int retval;

    pmsg[0] = &msg[0];
    msg[0].msg = text;
    msg[0].msg_style = type;
    resp = NULL;

    retval = converse(pamh, ctrl, 1, pmsg, &resp);
    if (retval == PAM_SUCCESS)
	_pam_drop_reply(resp, 1);

    return retval;
}

/* use this to free strings. ESPECIALLY password strings */
static char *_pam_delete(register char *xx)
{
    _pam_overwrite(xx);
    free(xx);
    return NULL;
}

/*
 * can't be a palindrome - like `R A D A R' or `M A D A M'
 */
static int palindrome(const char *old, const char *new)
{
    int	i, j;

	i = strlen (new);

	for (j = 0;j < i;j++)
		if (new[i - j - 1] != new[j])
			return 0;

	return 1;
}

/*
 * This is a reasonably severe check for a different selection of characters
 * in the old and new passwords.
 */

static int similar(struct cracklib_options *opt,
		    const char *old, const char *new)
{
    int i, j;

    for (i = j = 0; old[i]; i++) {
	if (strchr (new, old[i])) {
	    j++;
	}
    }

    if (((i-j) >= opt->diff_ok)
	|| (strlen(new) >= (j * 2))
	|| (strlen(new) >= opt->diff_ignore)) {
	/* passwords are not very similar */
	return 0;
    }

    /* passwords are too similar */
    return 1;
}

/*
 * a nice mix of characters.
 */
static int simple(struct cracklib_options *opt, const char *old, const char *new)
{
	int	digits = 0;
	int	uppers = 0;
	int	lowers = 0;
	int	others = 0;
	int	size;
	int	i;

	for (i = 0;new[i];i++) {
		if (isdigit (new[i]))
			digits++;
		else if (isupper (new[i]))
			uppers++;
		else if (islower (new[i]))
			lowers++;
		else
			others++;
	}

	/*
	 * The scam was this - a password of only one character type
	 * must be 8 letters long.  Two types, 7, and so on.
	 * This is now changed, the base size and the credits or defaults
	 * see the docs on the module for info on these parameters, the
	 * defaults cause the effect to be the same as before the change
	 */

 	if (digits > opt->dig_credit)
	    digits = opt->dig_credit;

 	if (uppers > opt->up_credit)
	    uppers = opt->up_credit;

 	if (lowers > opt->low_credit)
	    lowers = opt->low_credit;

 	if (others > opt->oth_credit)
	    others = opt->oth_credit;

 	size = opt->min_length;
 	size -= digits;
 	size -= uppers;
 	size -= lowers;
 	size -= others;

	if (size <= i)
		return 0;

	return 1;
}

static char * str_lower(char *string)
{
	char *cp;

	for (cp = string; *cp; cp++)
		*cp = tolower(*cp);
	return string;
}

static const char * password_check(struct cracklib_options *opt, const char *old, const char *new)
{
	const char *msg = NULL;
	char *oldmono, *newmono, *wrapped;

	if (strcmp(new, old) == 0) {
        msg = "is the same as the old one";
        return msg;
    }

	newmono = str_lower(x_strdup(new));
	oldmono = str_lower(x_strdup(old));
	wrapped = malloc(strlen(oldmono) * 2 + 1);
	strcpy (wrapped, oldmono);
	strcat (wrapped, oldmono);

	if (palindrome(oldmono, newmono))
		msg = "is a palindrome";

	if (!msg && strcmp(oldmono, newmono) == 0)
		msg = "case changes only";

	if (!msg && similar(opt, oldmono, newmono))
		msg = "is too similar to the old one";

	if (!msg && simple(opt, old, new))
		msg = "is too simple";

	if (!msg && strstr(wrapped, newmono))
		msg = "is rotated";

	memset(newmono, 0, strlen(newmono));
	memset(oldmono, 0, strlen(oldmono));
	memset(wrapped, 0, strlen(wrapped));
	free(newmono);
	free(oldmono);
	free(wrapped);

	return msg;
}


#define OLD_PASSWORDS_FILE	"/etc/security/opasswd"

static const char * check_old_password(const char *forwho, const char *newpass)
{
	static char buf[16384];
	char *s_luser, *s_uid, *s_npas, *s_pas;
	const char *msg = NULL;
	FILE *opwfile;

	opwfile = fopen(OLD_PASSWORDS_FILE, "r");
	if (opwfile == NULL)
		return NULL;

	while (fgets(buf, 16380, opwfile)) {
		if (!strncmp(buf, forwho, strlen(forwho))) {
			buf[strlen(buf)-1] = '\0';
			s_luser = strtok(buf, ":,");
			s_uid   = strtok(NULL, ":,");
			s_npas  = strtok(NULL, ":,");
			s_pas   = strtok(NULL, ":,");
			while (s_pas != NULL) {
				if (!strcmp(crypt(newpass, s_pas), s_pas)) {
					msg = "has been already used";
					break;
				}
				s_pas = strtok(NULL, ":,");
			}
			break;
		}
	}
	fclose(opwfile);

	return msg;
}


static int _pam_unix_approve_pass(pam_handle_t *pamh,
                                  unsigned int ctrl,
				  struct cracklib_options *opt,
                                  const char *pass_old,
                                  const char *pass_new)
{
    const char *msg = NULL;
    const char *user;
    int retval;
    
    if (pass_new == NULL || (pass_old && !strcmp(pass_old,pass_new))) {
        if (ctrl && PAM_DEBUG_ARG)
            _pam_log(LOG_DEBUG, "bad authentication token");
        make_remark(pamh, ctrl, PAM_ERROR_MSG,
                    pass_new == NULL ?
                    "No password supplied":"Password unchanged" );
        return PAM_AUTHTOK_ERR;
    }

    /*
     * if one wanted to hardwire authentication token strength
     * checking this would be the place
     */
    msg = password_check(opt, pass_old,pass_new);
    if (!msg) {
	retval = pam_get_item(pamh, PAM_USER, (const void **)&user);
	if (retval != PAM_SUCCESS) {
	    if (ctrl & PAM_DEBUG_ARG) {
		_pam_log(LOG_ERR,"Can not get username");
        	return PAM_AUTHTOK_ERR;
	    }
	}
	msg = check_old_password(user, pass_new);
    }

    if (msg) {
        char remark[BUFSIZ];
        
        memset(remark,0,sizeof(remark));
        sprintf(remark,"BAD PASSWORD: %s",msg);
        if (ctrl && PAM_DEBUG_ARG)
            _pam_log(LOG_NOTICE, "new passwd fails strength check: %s",
                                  msg);
        make_remark(pamh, ctrl, PAM_ERROR_MSG, remark);
        return PAM_AUTHTOK_ERR;
    };   
    return PAM_SUCCESS;
    
}

/* The Main Thing (by Cristian Gafton, CEO at this module :-) 
 * (stolen from http://home.netscape.com)
 */
PAM_EXTERN int pam_sm_chauthtok(pam_handle_t *pamh, int flags,
				int argc, const char **argv)
{
    unsigned int ctrl;
    struct cracklib_options options;

    options.retry_times = CO_RETRY_TIMES;
    options.diff_ok = CO_DIFF_OK;
    options.diff_ignore = CO_DIFF_IGNORE;
    options.min_length = CO_MIN_LENGTH;
    options.dig_credit = CO_DIG_CREDIT;
    options.up_credit = CO_UP_CREDIT;
    options.low_credit = CO_LOW_CREDIT;
    options.oth_credit = CO_OTH_CREDIT;
    options.use_authtok = CO_USE_AUTHTOK;
    memset(options.prompt_type, 0, BUFSIZ);
    
    ctrl = _pam_parse(&options, argc, argv);

    D(("called."));
    if (!options.prompt_type[0])
        strcpy(options.prompt_type,"UNIX");

    if (flags & PAM_PRELIM_CHECK) {
        /* Check for passwd dictionary */       
        struct stat st;
        char buf[sizeof(CRACKLIB_DICTPATH)+10];

	D(("prelim check"));

        memset(buf,0,sizeof(buf)); /* zero the buffer */
        sprintf(buf,"%s.pwd",CRACKLIB_DICTPATH);

        if (!stat(buf,&st) && st.st_size)
            return PAM_SUCCESS;
        else {
            if (ctrl & PAM_DEBUG_ARG)
                _pam_log(LOG_NOTICE,"dict path '%s'[.pwd] is invalid",
                                     CRACKLIB_DICTPATH);
            return PAM_ABORT;
        }
        
        /* Not reached */
        return PAM_SERVICE_ERR;

    } else if (flags & PAM_UPDATE_AUTHTOK) {
        int retval;
        char *token1, *token2, *oldtoken;
        struct pam_message msg[1],*pmsg[1];
        struct pam_response *resp;
        const char *cracklib_dictpath = CRACKLIB_DICTPATH;
        char prompt[BUFSIZ];

	D(("do update"));
        retval = pam_get_item(pamh, PAM_OLDAUTHTOK,
                              (const void **)&oldtoken);
        if (retval != PAM_SUCCESS) {
            if (ctrl & PAM_DEBUG_ARG)
                _pam_log(LOG_ERR,"Can not get old passwd");
            oldtoken=NULL;
            retval = PAM_SUCCESS;
        }

        do {        
        /*
         * make sure nothing inappropriate gets returned
         */
        token1 = token2 = NULL;
        
        if (!options.retry_times) {
	    D(("returning %s because maxtries reached",
	       pam_strerror(pamh, retval)));
            return retval;
	}

        /* Planned modus operandi:
         * Get a passwd.
         * Verify it against cracklib.
         * If okay get it a second time. 
         * Check to be the same with the first one.
         * set PAM_AUTHTOK and return
         */

	if (options.use_authtok == 1) {
	    const char *item = NULL;

	    retval = pam_get_item(pamh, PAM_AUTHTOK, (const void **) &item);
	    if (retval != PAM_SUCCESS) {
		/* very strange. */
		_pam_log(LOG_ALERT
			,"pam_get_item returned error to pam_cracklib"
			);
	    } else if (item != NULL) {      /* we have a password! */
		token1 = x_strdup(item);
		item = NULL;
	    } else {
		retval = PAM_AUTHTOK_RECOVER_ERR;         /* didn't work */
	    }

	} else {
            /* Prepare to ask the user for the first time */
            memset(prompt,0,sizeof(prompt));
            sprintf(prompt,PROMPT1,options.prompt_type);
            pmsg[0] = &msg[0];
            msg[0].msg_style = PAM_PROMPT_ECHO_OFF;
            msg[0].msg = prompt;

            resp = NULL;
            retval = converse(pamh, ctrl, 1, pmsg, &resp);
            if (resp != NULL) {
                /* interpret the response */
                if (retval == PAM_SUCCESS) {     /* a good conversation */
                    token1 = x_strdup(resp[0].resp);
                    if (token1 == NULL) {
                        _pam_log(LOG_NOTICE,
                                 "could not recover authentication token 1");
                        retval = PAM_AUTHTOK_RECOVER_ERR;
                    }
                }
                /*
                 * tidy up the conversation (resp_retcode) is ignored
                 */
                _pam_drop_reply(resp, 1);
            } else {
                retval = (retval == PAM_SUCCESS) ?
                         PAM_AUTHTOK_RECOVER_ERR:retval ;
            }
	}

        if (retval != PAM_SUCCESS) {
            if (ctrl && PAM_DEBUG_ARG)
                _pam_log(LOG_DEBUG,"unable to obtain a password");
            continue;
        }

	D(("testing password, retval = %s", pam_strerror(pamh, retval)));
        /* now test this passwd against cracklib */
        {
            char *crack_msg;
            char remark[BUFSIZ];
            
            bzero(remark,sizeof(remark));
	    D(("against cracklib"));
            if ((crack_msg = FascistCheck(token1, cracklib_dictpath))) {
                if (ctrl && PAM_DEBUG_ARG)
                    _pam_log(LOG_DEBUG,"bad password: %s",crack_msg);
                sprintf(remark,"BAD PASSWORD: %s", crack_msg);
                make_remark(pamh, ctrl, PAM_ERROR_MSG, remark);
                if (getuid() || (flags & PAM_CHANGE_EXPIRED_AUTHTOK))
                    retval = PAM_AUTHTOK_ERR;
                else
                    retval = PAM_SUCCESS;
            } else {
                /* check it for strength too... */
		D(("for strength"));
                if (oldtoken) {
                    retval = _pam_unix_approve_pass(pamh,ctrl,&options,
                                               oldtoken,token1);
                    if (retval != PAM_SUCCESS) {
                        if (getuid() || (flags & PAM_CHANGE_EXPIRED_AUTHTOK))
			    retval = PAM_AUTHTOK_ERR;
			else
			    retval = PAM_SUCCESS;
		    }
                }
            }
        }

	D(("after testing: retval = %s", pam_strerror(pamh, retval)));
        /* if cracklib/strength check said it is a bad passwd... */
        if ((retval != PAM_SUCCESS) && (retval != PAM_IGNORE)) {
	    int temp_unused;

	    temp_unused = pam_set_item(pamh, PAM_AUTHTOK, NULL);
            token1 = _pam_delete(token1);
            continue;
        }

        /* Now we have a good passwd. Ask for it once again */

        if (options.use_authtok == 0) {
            bzero(prompt,sizeof(prompt));
            sprintf(prompt,PROMPT2,options.prompt_type);
            pmsg[0] = &msg[0];
            msg[0].msg_style = PAM_PROMPT_ECHO_OFF;
            msg[0].msg = prompt;

            resp = NULL;
            retval = converse(pamh, ctrl, 1, pmsg, &resp);
            if (resp != NULL) {
                /* interpret the response */
                if (retval == PAM_SUCCESS) {     /* a good conversation */
                    token2 = x_strdup(resp[0].resp);
                    if (token2 == NULL) {
                        _pam_log(LOG_NOTICE,
                                 "could not recover authentication token 2");
                        retval = PAM_AUTHTOK_RECOVER_ERR;
                    }
                }
                /*
                 * tidy up the conversation (resp_retcode) is ignored
                 */
	        _pam_drop_reply(resp, 1);
            } else {
                retval = (retval == PAM_SUCCESS) ?
                         PAM_AUTHTOK_RECOVER_ERR:retval ;
            }

            if (retval != PAM_SUCCESS) {
                if (ctrl && PAM_DEBUG_ARG)
                    _pam_log(LOG_DEBUG
			     ,"unable to obtain the password a second time");
                continue;
            }

            /* Hopefully now token1 and token2 the same password ... */
            if (strcmp(token1,token2) != 0) {
                /* tell the user */
                make_remark(pamh, ctrl, PAM_ERROR_MSG, MISTYPED_PASS);
                token1 = _pam_delete(token1);
                token2 = _pam_delete(token2);
                pam_set_item(pamh, PAM_AUTHTOK, NULL);
                if (ctrl & PAM_DEBUG_ARG)
                    _pam_log(LOG_NOTICE,"Password mistyped");
                retval = PAM_AUTHTOK_RECOVER_ERR;
                continue;
            }
        
            /* Yes, the password was typed correct twice
             * we store this password as an item
             */

	    {
		const char *item = NULL;

		retval = pam_set_item(pamh, PAM_AUTHTOK, token1);

		/* clean up */
		token1 = _pam_delete(token1);
		token2 = _pam_delete(token2);

		if ( (retval != PAM_SUCCESS) ||
		     ((retval = pam_get_item(pamh, PAM_AUTHTOK,
					     (const void **)&item)
			 ) != PAM_SUCCESS) ) {
                    _pam_log(LOG_CRIT, "error manipulating password");
                    continue;
		}
		item = NULL;                 /* break link to password */
		return PAM_SUCCESS;
	    }
        }
        
        } while (options.retry_times--);

    } else {
        if (ctrl & PAM_DEBUG_ARG)
            _pam_log(LOG_NOTICE, "UNKNOWN flags setting %02X",flags);
        return PAM_SERVICE_ERR;
    }

    /* Not reached */
    return PAM_SERVICE_ERR;                            
}



#ifdef PAM_STATIC
/* static module data */
struct pam_module _pam_cracklib_modstruct = {
     "pam_cracklib",
     NULL,
     NULL,
     NULL,
     NULL,
     NULL,
     pam_sm_chauthtok
};
#endif

/*
 * Copyright (c) Cristian Gafton <gafton@redhat.com>, 1996.
 *                                              All rights reserved
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
 * THIS SOFTWARE IS PROVIDED `AS IS'' AND ANY EXPRESS OR IMPLIED
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
 *
 * The following copyright was appended for the long password support
 * added with the libpam 0.58 release:
 *
 * Modificaton Copyright (c) Philip W. Dalrymple III <pwd@mdtsoft.com>
 *       1997. All rights reserved
 *
 * THE MODIFICATION THAT PROVIDES SUPPORT FOR LONG PASSWORD TYPE CHECKING TO
 * THIS SOFTWARE IS PROVIDED `AS IS'' AND ANY EXPRESS OR IMPLIED
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
