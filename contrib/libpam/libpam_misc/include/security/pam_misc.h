/*
 * $Id: pam_misc.h,v 1.3 2001/01/20 22:29:47 agmorgan Exp $
 * $FreeBSD$
 */

#ifndef __PAMMISC_H
#define __PAMMISC_H

#include <security/pam_appl.h>
#include <security/pam_client.h>

/* include some useful macros */

#include <security/_pam_macros.h>

/* functions defined in pam_misc.* libraries */

extern int misc_conv(int _num_msg, const struct pam_message **_msgm,
		     struct pam_response **_response, void *_appdata_ptr);

#include <time.h>

extern time_t pam_misc_conv_warn_time; /* time that we should warn user */
extern time_t pam_misc_conv_die_time;         /* cut-off time for input */
extern const char *pam_misc_conv_warn_line;           /* warning notice */
extern const char *pam_misc_conv_die_line;            /* cut-off remark */
extern int pam_misc_conv_died;      /* 1 = cut-off time reached (0 not) */
extern int (*pam_binary_handler_fn)(void *_appdata, pamc_bp_t *_prompt_p);
extern void (*pam_binary_handler_free)(void *_appdata, pamc_bp_t *_prompt_p);
/*
 * Environment helper functions
 */

/* transcribe given environment (to pam) */
extern int pam_misc_paste_env(pam_handle_t *_pamh
			      , const char * const *_user_env);

/* char **pam_misc_copy_env(pam_handle_t *pamh);

   This is no longer defined as a prototype because the X/Open XSSO
   spec makes it clear that PAM's pam_getenvlist() does exactly
   what this was needed for.

   A wrapper is still provided in the pam_misc library - so that
   legacy applications will still work.  But _BE_WARNED_ it will
   disappear by the release of libpam 1.0 . */

/* delete environment as obtained from (pam_getenvlist) */
extern char **pam_misc_drop_env(char **env);

/* provide something like the POSIX setenv function for the (Linux-)PAM
 * environment. */

extern int pam_misc_setenv(pam_handle_t *pamh, const char *name
			   , const char *value, int readonly);

#endif

 
	
