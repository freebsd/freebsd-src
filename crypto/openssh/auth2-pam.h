/* $Id: auth2-pam.h,v 1.2 2001/02/09 01:55:36 djm Exp $ */

#include "includes.h"
#ifdef USE_PAM

int	auth2_pam(Authctxt *authctxt);

#endif /* USE_PAM */
