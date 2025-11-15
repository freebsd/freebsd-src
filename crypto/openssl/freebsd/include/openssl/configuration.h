
/**
 * OpenSSL's Configure script generates these values automatically for the host
 * architecture, but FreeBSD provides values which are universal for all
 * supported target architectures.
 */

#ifndef	__FREEBSD_CONFIGURATION_H__
#define	__FREEBSD_CONFIGURATION_H__

# undef OPENSSL_NO_EC_NISTP_64_GCC_128
# if __SIZEOF_LONG__ == 4 || __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#  ifndef OPENSSL_NO_EC_NISTP_64_GCC_128
#   define OPENSSL_NO_EC_NISTP_64_GCC_128
#  endif
# endif

# undef BN_LLONG
# undef	SIXTY_FOUR_BIT_LONG
# undef SIXTY_FOUR_BIT
# undef	THIRTY_TWO_BIT
# if !defined(OPENSSL_SYS_UEFI)
#  if __SIZEOF_LONG__ == 8
#   undef BN_LLONG
#   define SIXTY_FOUR_BIT_LONG
#   undef SIXTY_FOUR_BIT
#   undef THIRTY_TWO_BIT
#  elif __SIZEOF_LONG__ == 4
#   define BN_LLONG
#   undef SIXTY_FOUR_BIT_LONG
#   undef SIXTY_FOUR_BIT
#   define THIRTY_TWO_BIT
#  else
#   error Unsupported size of long
#  endif
# endif

#endif  /* __FREEBSD_CONFIGURATION_H__ */
