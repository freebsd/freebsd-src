/* pam_map.c - PAM mapping interface
 *
 * $Id$
 * $FreeBSD$
 *
 * This is based on the X/Open XSSO specification of March 1997.
 * It is not implemented as it is going to change... after 1997/9/25.
 *
 * $Log$
 */

#include <stdio.h>

#include "pam_private.h"

/* p 54 */

int pam_get_mapped_authtok(pam_handle_t *pamh,
			   const char *target_module_username,
			   const char *target_module_type,
			   const char *target_authn_domain,
			   size_t *target_authtok_len
			   unsigned char **target_module_authtok);
{
    D(("called"));

    IF_NO_PAMH("pam_get_mapped_authtok",pamh,PAM_SYSTEM_ERR);

    return PAM_SYSTEM_ERROR;
}

/* p 68 */

int pam_set_mapped_authtok(pam_handle_t *pamh,
			   char *target_module_username,
			   size_t *target_authtok_len,
			   unsigned char *target_module_authtok,
			   char *target_module_type,
			   char *target_authn_domain)
{
    D(("called"));

    IF_NO_PAMH("pam_set_mapped_authtok",pamh,PAM_SYSTEM_ERR);

    return PAM_SYSTEM_ERROR;
}

/* p 56 */

int pam_get_mapped_username(pam_handle_t *pamh,
			    const char *src_username,
			    const char *src_module_type,
			    const char *src_authn_domain,
			    const char *target_module_type,
			    const char *target_authn_domain,
			    char **target_module_username)
{
    D(("called"));

    IF_NO_PAMH("pam_get_mapped_username",pamh,PAM_SYSTEM_ERR);

    return PAM_SYSTEM_ERROR;
}

/* p 70 */

int pam_set_mapped_username(pam_handle_t *pamh,
			    char *src_username,
			    char *src_module_type,
			    char *src_authn_domain,
			    char *target_module_username,
			    char *target_module_type,
			    char *target_authn_domain)
{
    D(("called"));

    IF_NO_PAMH("pam_set_mapped_username",pamh,PAM_SYSTEM_ERR);

    return PAM_SYSTEM_ERROR;
}
