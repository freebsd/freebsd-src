/* pam_session.c - PAM Session Management */

/*
 * $Id: pam_session.c,v 1.3 1996/12/01 03:14:13 morgan Exp $
 * $FreeBSD$
 *
 * $Log: pam_session.c,v $
 * Revision 1.3  1996/12/01 03:14:13  morgan
 * use _pam_macros.h
 *
 * Revision 1.2  1996/03/10 02:19:12  morgan
 * some oversight meant that this wasn't being compiled. It needed a
 * couple of changes.
 *
 *
 */

#include <stdio.h>

#include "pam_private.h"

int pam_open_session(pam_handle_t *pamh, int flags)
{
    D(("called"));

    IF_NO_PAMH("pam_open_session",pamh,PAM_SYSTEM_ERR);
    return _pam_dispatch(pamh, flags, PAM_OPEN_SESSION);
}

int pam_close_session(pam_handle_t *pamh, int flags)
{
    D(("called"));

    IF_NO_PAMH("pam_close_session",pamh,PAM_SYSTEM_ERR);
    return _pam_dispatch(pamh, flags, PAM_CLOSE_SESSION);
}
