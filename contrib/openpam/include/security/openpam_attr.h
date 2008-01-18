/*
 * $Id: openpam_attr.h 405 2007-12-19 11:38:27Z des $
 */

#ifndef SECURITY_PAM_ATTRIBUTES_H_INCLUDED
#define SECURITY_PAM_ATTRIBUTES_H_INCLUDED

/* GCC attributes */
#if defined(__GNUC__) && defined(__GNUC_MINOR__) && !defined(__STRICT_ANSI__)
# define OPENPAM_GNUC_PREREQ(maj, min) \
        ((__GNUC__ << 16) + __GNUC_MINOR__ >= ((maj) << 16) + (min))
#else
# define OPENPAM_GNUC_PREREQ(maj, min) 0
#endif

#if OPENPAM_GNUC_PREREQ(2,5)
# define OPENPAM_FORMAT(params) __attribute__((__format__ params))
#else
# define OPENPAM_FORMAT(params)
#endif

#if OPENPAM_GNUC_PREREQ(3,3)
# define OPENPAM_NONNULL(params) __attribute__((__nonnull__ params))
#else
# define OPENPAM_NONNULL(params)
#endif

#endif /* !SECURITY_PAM_ATTRIBUTES_H_INCLUDED */
