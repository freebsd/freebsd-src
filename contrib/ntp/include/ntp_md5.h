/*
 * ntp_md5.h: deal with md5.h headers
 */

#ifdef HAVE_MD5_H
# include <md5.h>
#else
# include "rsa_md5.h"
#endif
