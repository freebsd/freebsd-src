/* $Id: base64.h,v 1.4 2003/02/24 04:45:43 djm Exp $ */

#ifndef _BSD_BASE64_H
#define _BSD_BASE64_H

#include "config.h"

#ifndef HAVE___B64_NTOP
# ifndef HAVE_B64_NTOP
int b64_ntop(u_char const *src, size_t srclength, char *target, 
    size_t targsize);
# endif /* !HAVE_B64_NTOP */
# define __b64_ntop b64_ntop
#endif /* HAVE___B64_NTOP */

#ifndef HAVE___B64_PTON
# ifndef HAVE_B64_PTON
int b64_pton(char const *src, u_char *target, size_t targsize);
# endif /* !HAVE_B64_PTON */
# define __b64_pton b64_pton
#endif /* HAVE___B64_PTON */

#endif /* _BSD_BASE64_H */
