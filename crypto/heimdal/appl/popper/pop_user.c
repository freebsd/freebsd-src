/*
 * Copyright (c) 1989 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 */

#include <popper.h>
RCSID("$Id: pop_user.c,v 1.15 1999/09/16 20:38:50 assar Exp $");

/* 
 *  user:   Prompt for the user name at the start of a POP session
 */

int
pop_user (POP *p)
{
    char ss[256];

    strlcpy(p->user, p->pop_parm[1], sizeof(p->user));

#ifdef OTP
    if (otp_challenge (&p->otp_ctx, p->user, ss, sizeof(ss)) == 0) {
	return pop_msg(p, POP_SUCCESS, "Password %s required for %s.",
		       ss, p->user);
    } else
#endif
    if (p->auth_level != AUTH_NONE) {
	char *s = NULL;
#ifdef OTP
	s = otp_error(&p->otp_ctx);
#endif
	return pop_msg(p, POP_FAILURE, "Permission denied%s%s",
		       s ? ":" : "", s ? s : "");
    } else
	return pop_msg(p, POP_SUCCESS, "Password required for %s.", p->user);
}
