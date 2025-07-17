/*
 * ntp_c99_snprintf.h
 *
 * Included from config.h to deal with replacing [v]snprintf() on older
 * systems.  The #undef lines below cannot be directly in config.h as
 * config.status modifies each #undef in config.h.in to either be a
 * commented-out #undef or a functional #define.  Here they are used
 * to avoid redefinition warnings on systems such as macos ca. 2024
 * where system headers define [v]snprintf as preprocessor macros.
 *
 * Do not include this file directly, leave it to config.h.
 */

#if !defined(_KERNEL) && !defined(PARSESTREAM)
/*
 * stdio.h must be included in config.h after _GNU_SOURCE is defined
 * but before #define snprintf rpl_snprintf
 */
# include <stdio.h>
#endif

#ifdef HW_WANT_RPL_SNPRINTF
# undef snprintf
#endif
#ifdef HW_WANT_RPL_VSNPRINTF
# undef vsnprintf
#endif

