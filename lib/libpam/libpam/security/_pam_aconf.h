/* _pam_aconf.h.  Generated automatically by configure.  */
/*
 * $Id: _pam_aconf.h.in,v 1.4 2000/12/04 20:56:10 baggins Exp $
 * $FreeBSD$
 *
 * 
 */

#ifndef PAM_ACONF_H
#define PAM_ACONF_H

/* lots of stuff gets written to /tmp/pam-debug.log */
/* #undef DEBUG */

/* build libraries with different names (suffixed with 'd') */
/* #undef WITH_LIBDEBUG */

/* provide a global locking facility within libpam */
/* #undef PAM_LOCKING */

/* GNU systems as a class, all have the feature.h file */
/* #undef HAVE_FEATURES_H */
#ifdef HAVE_FEATURES_H
# define _SVID_SOURCE
# define _BSD_SOURCE
# define __USE_BSD
# define __USE_SVID
# define __USE_MISC
# define _GNU_SOURCE
# include <features.h>
#endif /* HAVE_FEATURES_H */

/* we have libcrack available */
/* #undef HAVE_LIBCRACK */

/* we have libcrypt - its not part of libc (do we need both definitions?) */
/* #undef HAVE_LIBCRYPT */
/* #undef HAVE_CRYPT_H */

/* we have libndbm and/or libdb */
#define HAVE_DB_H 1
#define HAVE_NDBM_H 1

/* have libfl (Flex) */
#define HAVE_LIBFL 1

/* have libnsl - instead of libc support */
/* #undef HAVE_LIBNSL */

/* have libpwdb - don't expect this to be important for much longer */
/* #undef HAVE_LIBPWDB */

/* ugly hack to partially support old pam_strerror syntax */
/* #undef UGLY_HACK_FOR_PRIOR_BEHAVIOR_SUPPORT */

/* read both confs - read /etc/pam.d and /etc/pam.conf in serial */
#define PAM_READ_BOTH_CONFS 1

#define HAVE_PATHS_H 1
#ifdef HAVE_PATHS_H
#include <paths.h>
#endif
/* location of the mail spool directory */
#define PAM_PATH_MAILDIR _PATH_MAILDIR

#endif /* PAM_ACONF_H */
