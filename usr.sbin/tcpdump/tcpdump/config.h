/* $FreeBSD: src/usr.sbin/tcpdump/tcpdump/config.h,v 1.1 2000/01/30 01:05:24 fenner Exp $ */

/* config.h.  Generated automatically by configure.  */
/* config.h.in.  Generated automatically from configure.in by autoheader.  */
/* "generated automatically" means DO NOT MAKE CHANGES TO config.h.in --
 * make them to acconfig.h and rerun autoheader */

/* Define to empty if the keyword does not work.  */
/* #undef const */

/* Define as __inline if that's what the C compiler calls it.  */
/* #undef inline */

/* Define as the return type of signal handlers (int or void).  */
#define RETSIGTYPE void

/* Define if you can safely include both <sys/time.h> and <time.h>.  */
#define TIME_WITH_SYS_TIME 1

/* Define if your processor stores words with the most significant
   byte first (like Motorola and SPARC, unlike Intel and VAX).  */
/* #undef WORDS_BIGENDIAN */

/* Define if you have SSLeay XXX why isn't this HAVE_LIBCRYPTO? */
/* #undef CRYPTO */

/* Define if you have SSLeay 0.9.0b with the buggy cast128. */
/* #undef HAVE_BUGGY_CAST128 */

/* Define both to enable IPv6 support XXX why 2? ENABLE_IPV6 is not used. */
#define ENABLE_IPV6 1
#define INET6 1

/* Define if you enable support for the libsmi. */
/* #undef LIBSMI */

/* Is T_AAAA predefined? */
#define HAVE_AAAA 1

/* Fallback definition if not in headers */
/* XXX why is this not #ifndef HAVE_AAA #define T_AAAA ... ? */
/* XXX or even #ifndef T_AAAA ... */
/* #undef T_AAAA */

/* define if you have struct __res_state_ext */
#define HAVE_RES_STATE_EXT 1

/* define if your struct __res_state has the nsort member */
#define HAVE_NEW_RES_STATE 1

/*
 * define if struct ether_header.ether_dhost is a struct with ether_addr_octet
 */
/* #undef ETHER_HEADER_HAS_EA */

/*
 * define if struct ether_arp.arp_sha is a struct with ether_addr_octet
 */
/* #undef ETHER_ARP_HAS_EA */

/* define if struct ether_arp contains arp_xsha */
/* #undef ETHER_ARP_HAS_X */

/* define if you have the addrinfo function. */
#define HAVE_ADDRINFO 1

/* define if you need to include missing/addrinfoh.h. */
/* #undef NEED_ADDRINFO_H */

/* define ifyou have the h_errno variable. */
#define HAVE_H_ERRNO 1

/* define if IN6ADDRSZ is defined (XXX not used!) */
#define HAVE_IN6ADDRSZ 1

/* define if INADDRSZ is defined (XXX not used!) */
#define HAVE_INADDRSZ 1

/* define if you have <net/slip.h> */
#define HAVE_NET_SLIP_H 1

/* define if this is a development version, to use additional prototypes. */
/* #undef HAVE_OS_PROTO_H */

/* define if <unistd.h> defines __P() */
#define HAVE_PORTABLE_PROTOTYPE 1

/* define if RES_USE_INET6 is defined */
#define HAVE_RES_USE_INET6 1

/* define if struct sockaddr has the sa_len member */
#define HAVE_SOCKADDR_SA_LEN 1

/* define if you have struct sockaddr_storage */
#define HAVE_SOCKADDR_STORAGE 1

/* define if unaligned memory accesses fail */
/* #undef LBL_ALIGN */

/* The successful return value from signal (?)XXX */
#define RETSIGVAL 

/* Define this on IRIX */
/* #undef _BSD_SIGNALS */

/* For HP/UX ANSI compiler? */
/* #undef _HPUX_SOURCE */

/* AIX hack. */
/* #undef _SUN */

/* OSF hack: "Workaround around ip_hl vs. ip_vhl problem in netinet/ip.h" */
/* #undef __STDC__ */

/* Workaround for missing sized types */
/* XXX this should move to the more standard uint*_t */
/* #undef int16_t */
/* #undef int32_t */
/* #undef u_int16_t */
/* #undef u_int32_t */
/* #undef u_int8_t */

/* The number of bytes in a char.  */
#define SIZEOF_CHAR 1

/* The number of bytes in a int.  */
#define SIZEOF_INT 4

/* The number of bytes in a long.  */
#define SIZEOF_LONG 4

/* The number of bytes in a short.  */
#define SIZEOF_SHORT 2

/* Define if you have the ether_ntohost function.  */
#define HAVE_ETHER_NTOHOST 1

/* Define if you have the getaddrinfo function.  */
#define HAVE_GETADDRINFO 1

/* Define if you have the gethostbyname2 function.  */
#define HAVE_GETHOSTBYNAME2 1

/* Define if you have the getnameinfo function.  */
#define HAVE_GETNAMEINFO 1

/* Define if you have the inet_aton function.  */
#define HAVE_INET_ATON 1

/* Define if you have the inet_ntop function.  */
#define HAVE_INET_NTOP 1

/* Define if you have the inet_pton function.  */
#define HAVE_INET_PTON 1

/* Define if you have the pfopen function.  */
/* #undef HAVE_PFOPEN */

/* Define if you have the setlinebuf function.  */
#define HAVE_SETLINEBUF 1

/* Define if you have the sigaction function.  */
#define HAVE_SIGACTION 1

/* Define if you have the sigset function.  */
/* #undef HAVE_SIGSET */

/* Define if you have the strcasecmp function.  */
#define HAVE_STRCASECMP 1

/* Define if you have the vfprintf function.  */
#define HAVE_VFPRINTF 1

/* Define if you have the <cast.h> header file.  */
/* #undef HAVE_CAST_H */

/* Define if you have the <fcntl.h> header file.  */
#define HAVE_FCNTL_H 1

/* Define if you have the <malloc.h> header file.  */
/* #undef HAVE_MALLOC_H */

/* Define if you have the <memory.h> header file.  */
#define HAVE_MEMORY_H 1

/* Define if you have the <net/slip.h> header file.  */
#define HAVE_NET_SLIP_H 1

/* Define if you have the <rc5.h> header file.  */
/* #undef HAVE_RC5_H */

/* Define if you have the <rpc/rpcent.h> header file.  */
/* #undef HAVE_RPC_RPCENT_H */

/* Define if you have the <smi.h> header file.  */
/* #undef HAVE_SMI_H */

/* Define if you have the <zlib.h> header file.  */
#define HAVE_ZLIB_H 1

/* Define if you have the crypto library (-lcrypto).  */
/* #undef HAVE_LIBCRYPTO */

/* Define if you have the dnet library (-ldnet).  */
/* #undef HAVE_LIBDNET */

/* Define if you have the resolv library (-lresolv).  */
/* #undef HAVE_LIBRESOLV */

/* Define if you have the rpc library (-lrpc).  */
/* #undef HAVE_LIBRPC */

/* Define if you have the smi library (-lsmi).  */
/* #undef HAVE_LIBSMI */

/* Define if you have the socket library (-lsocket).  */
/* #undef HAVE_LIBSOCKET */

/* Define if you have the str library (-lstr).  */
/* #undef HAVE_LIBSTR */

/* Define if you have the z library (-lz).  */
#define HAVE_LIBZ 1
