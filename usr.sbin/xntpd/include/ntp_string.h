/*
 * Define bcopy, bzero, and bcmp and string op's
 */

#ifndef  _ntp_string_h
#define  _ntp_string_h

#ifdef NTP_POSIX_SOURCE

#if defined(HAVE_MEMORY_H)
#include <memory.h>
#endif

#include <string.h>

#define bcopy(s1,s2,n) memcpy(s2, s1, n)
#define bzero(s,n)     memset(s, 0, n)
#define bcmp(s1,s2,n)  memcmp(s1, s2, n)

#else /* NTP_POSIX_SOURCE */

#include <strings.h>

#define  strrchr    rindex
#define  strchr     index

#endif /*  NTP_POSIX_SOURCE */

#endif /* _ntp_string_h */
