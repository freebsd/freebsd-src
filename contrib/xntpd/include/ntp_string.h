/*
 * Define string ops: strchr strrchr memcmp memmove memset 
 */

#ifndef  _ntp_string_h
#define  _ntp_string_h

#if defined(NTP_POSIX_SOURCE)

# if defined(HAVE_MEMORY_H)
#  include <memory.h>
# endif

# include <string.h>

#else

# include <strings.h>
# define strchr(s,c) index(s,c)
# define strrchr(s,c) rindex(s,c)
# ifndef NTP_NEED_BOPS
# define NTP_NEED_BOPS
# endif
#endif /* NTP_POSIX_SOURCE */

#ifdef NTP_NEED_BOPS

# define memcmp(a,b,c) bcmp(a,b,c)
# define memmove(t,f,c) bcopy(f,t,c)
# define memset(a,x,c) if (x == 0x00) bzero(a,c); else ntp_memset((char*)a,x,c)
void ntp_memset P((char *, int, int));

#endif /*  NTP_NEED_BOPS */

#endif /* _ntp_string_h */
