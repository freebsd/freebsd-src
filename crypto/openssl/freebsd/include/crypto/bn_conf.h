
/**
 * OpenSSL's Configure script generates these values automatically for the host
 * architecture, but FreeBSD provides values which are universal for all
 * supported target architectures.
 */

#ifndef	__FREEBSD_BN_CONF_H__
#define	__FREEBSD_BN_CONF_H__

# undef	SIXTY_FOUR_BIT_LONG
# undef SIXTY_FOUR_BIT
# undef	THIRTY_TWO_BIT

# if __SIZEOF_LONG__ == 8
#  define SIXTY_FOUR_BIT_LONG
#  undef SIXTY_FOUR_BIT
#  undef THIRTY_TWO_BIT
# elif __SIZEOF_LONG__ == 4
#  undef SIXTY_FOUR_BIT_LONG
#  undef SIXTY_FOUR_BIT
#  define THIRTY_TWO_BIT
# else
#  error Unsupported size of long
# endif

#endif  /* __FREEBSD_BN_CONF_H__ */
