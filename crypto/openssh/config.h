/* config.h.  Generated from config.h.in by configure.  */
/* config.h.in.  Generated from configure.ac by autoheader.  */

/* Define if building universal (internal helper macro) */
/* #undef AC_APPLE_UNIVERSAL_BUILD */

/* Define if you have a getaddrinfo that fails for the all-zeros IPv6 address
   */
/* #undef AIX_GETNAMEINFO_HACK */

/* Define if your AIX loginfailed() function takes 4 arguments (AIX >= 5.2) */
/* #undef AIX_LOGINFAILED_4ARG */

/* System only supports IPv4 audit records */
/* #undef AU_IPv4 */

/* Define if your resolver libs need this for getrrsetbyname */
/* #undef BIND_8_COMPAT */

/* The system has incomplete BSM API */
/* #undef BROKEN_BSM_API */

/* Define if cmsg_type is not passed correctly */
/* #undef BROKEN_CMSG_TYPE */

/* getaddrinfo is broken (if present) */
/* #undef BROKEN_GETADDRINFO */

/* getgroups(0,NULL) will return -1 */
/* #undef BROKEN_GETGROUPS */

/* FreeBSD glob does not do what we need */
#define BROKEN_GLOB 1

/* Define if you system's inet_ntoa is busted (e.g. Irix gcc issue) */
/* #undef BROKEN_INET_NTOA */

/* Define if your struct dirent expects you to allocate extra space for d_name
   */
/* #undef BROKEN_ONE_BYTE_DIRENT_D_NAME */

/* Can't do comparisons on readv */
/* #undef BROKEN_READV_COMPARISON */

/* NetBSD read function is sometimes redirected, breaking atomicio comparisons
   against it */
/* #undef BROKEN_READ_COMPARISON */

/* Needed for NeXT */
/* #undef BROKEN_SAVED_UIDS */

/* Define if your setregid() is broken */
/* #undef BROKEN_SETREGID */

/* Define if your setresgid() is broken */
/* #undef BROKEN_SETRESGID */

/* Define if your setresuid() is broken */
/* #undef BROKEN_SETRESUID */

/* Define if your setreuid() is broken */
/* #undef BROKEN_SETREUID */

/* LynxOS has broken setvbuf() implementation */
/* #undef BROKEN_SETVBUF */

/* QNX shadow support is broken */
/* #undef BROKEN_SHADOW_EXPIRE */

/* Define if your snprintf is busted */
/* #undef BROKEN_SNPRINTF */

/* strndup broken, see APAR IY61211 */
/* #undef BROKEN_STRNDUP */

/* strnlen broken, see APAR IY62551 */
/* #undef BROKEN_STRNLEN */

/* strnvis detected broken */
#define BROKEN_STRNVIS 1

/* tcgetattr with ICANON may hang */
/* #undef BROKEN_TCGETATTR_ICANON */

/* updwtmpx is broken (if present) */
/* #undef BROKEN_UPDWTMPX */

/* Define if you have BSD auth support */
/* #undef BSD_AUTH */

/* Define if you want to specify the path to your lastlog file */
/* #undef CONF_LASTLOG_FILE */

/* Define if you want to specify the path to your utmp file */
/* #undef CONF_UTMP_FILE */

/* Define if you want to specify the path to your wtmpx file */
/* #undef CONF_WTMPX_FILE */

/* Define if you want to specify the path to your wtmp file */
/* #undef CONF_WTMP_FILE */

/* Need to call setpgrp as root */
/* #undef DISABLE_FD_PASSING */

/* Define if you don't want to use lastlog */
#define DISABLE_LASTLOG 1

/* Define if you don't want to use your system's login() call */
/* #undef DISABLE_LOGIN */

/* Define if you don't want to use pututline() etc. to write [uw]tmp */
/* #undef DISABLE_PUTUTLINE */

/* Define if you don't want to use pututxline() etc. to write [uw]tmpx */
/* #undef DISABLE_PUTUTXLINE */

/* Define if you want to disable shadow passwords */
/* #undef DISABLE_SHADOW */

/* Define if you don't want to use utmp */
#define DISABLE_UTMP 1

/* Define if you don't want to use utmpx */
/* #undef DISABLE_UTMPX */

/* Define if you don't want to use wtmp */
#define DISABLE_WTMP 1

/* Define if you don't want to use wtmpx */
#define DISABLE_WTMPX 1

/* Enable for PKCS#11 support */
#define ENABLE_PKCS11 /**/

/* Enable for U2F/FIDO support */
#define ENABLE_SK /**/

/* Enable for built-in U2F/FIDO support */
/* #undef ENABLE_SK_INTERNAL */

/* define if fflush(NULL) does not work */
/* #undef FFLUSH_NULL_BUG */

/* File names may not contain backslash characters */
/* #undef FILESYSTEM_NO_BACKSLASH */

/* fsid_t has member val */
/* #undef FSID_HAS_VAL */

/* fsid_t has member __val */
/* #undef FSID_HAS___VAL */

/* getpgrp takes one arg */
#define GETPGRP_VOID 1

/* Conflicting defs for getspnam */
/* #undef GETSPNAM_CONFLICTING_DEFS */

/* Define if your system glob() function has the GLOB_ALTDIRFUNC extension */
#define GLOB_HAS_ALTDIRFUNC 1

/* Define if your system glob() function has gl_matchc options in glob_t */
#define GLOB_HAS_GL_MATCHC 1

/* Define if your system glob() function has gl_statv options in glob_t */
/* #undef GLOB_HAS_GL_STATV */

/* Define this if you want GSSAPI support in the version 2 protocol */
/* #undef GSSAPI */

/* Define if you want to use shadow password expire field */
/* #undef HAS_SHADOW_EXPIRE */

/* Define if your system uses access rights style file descriptor passing */
/* #undef HAVE_ACCRIGHTS_IN_MSGHDR */

/* Define if you have ut_addr in utmp.h */
/* #undef HAVE_ADDR_IN_UTMP */

/* Define if you have ut_addr in utmpx.h */
/* #undef HAVE_ADDR_IN_UTMPX */

/* Define if you have ut_addr_v6 in utmp.h */
/* #undef HAVE_ADDR_V6_IN_UTMP */

/* Define if you have ut_addr_v6 in utmpx.h */
/* #undef HAVE_ADDR_V6_IN_UTMPX */

/* Define to 1 if you have the `arc4random' function. */
#define HAVE_ARC4RANDOM 1

/* Define to 1 if you have the `arc4random_buf' function. */
#define HAVE_ARC4RANDOM_BUF 1

/* Define to 1 if you have the `arc4random_stir' function. */
/* #undef HAVE_ARC4RANDOM_STIR */

/* Define to 1 if you have the `arc4random_uniform' function. */
#define HAVE_ARC4RANDOM_UNIFORM 1

/* Define to 1 if you have the `asprintf' function. */
#define HAVE_ASPRINTF 1

/* OpenBSD's gcc has bounded */
/* #undef HAVE_ATTRIBUTE__BOUNDED__ */

/* Have attribute nonnull */
#define HAVE_ATTRIBUTE__NONNULL__ 1

/* OpenBSD's gcc has sentinel */
/* #undef HAVE_ATTRIBUTE__SENTINEL__ */

/* Define to 1 if you have the `aug_get_machine' function. */
/* #undef HAVE_AUG_GET_MACHINE */

/* Define to 1 if you have the `auth_hostok' function. */
#define HAVE_AUTH_HOSTOK 1

/* Define to 1 if you have the `auth_timeok' function. */
#define HAVE_AUTH_TIMEOK 1

/* Define to 1 if you have the `b64_ntop' function. */
/* #undef HAVE_B64_NTOP */

/* Define to 1 if you have the `b64_pton' function. */
/* #undef HAVE_B64_PTON */

/* Define if you have the basename function. */
#define HAVE_BASENAME 1

/* Define to 1 if you have the `bcopy' function. */
#define HAVE_BCOPY 1

/* Define to 1 if you have the `bcrypt_pbkdf' function. */
/* #undef HAVE_BCRYPT_PBKDF */

/* Define to 1 if you have the `bindresvport_sa' function. */
#define HAVE_BINDRESVPORT_SA 1

/* Define to 1 if you have the `blf_enc' function. */
/* #undef HAVE_BLF_ENC */

/* Define to 1 if you have the <blf.h> header file. */
/* #undef HAVE_BLF_H */

/* Define to 1 if you have the `Blowfish_expand0state' function. */
/* #undef HAVE_BLOWFISH_EXPAND0STATE */

/* Define to 1 if you have the `Blowfish_expandstate' function. */
/* #undef HAVE_BLOWFISH_EXPANDSTATE */

/* Define to 1 if you have the `Blowfish_initstate' function. */
/* #undef HAVE_BLOWFISH_INITSTATE */

/* Define to 1 if you have the `Blowfish_stream2word' function. */
/* #undef HAVE_BLOWFISH_STREAM2WORD */

/* Define to 1 if you have the `BN_is_prime_ex' function. */
#define HAVE_BN_IS_PRIME_EX 1

/* Define to 1 if you have the <bsd/libutil.h> header file. */
/* #undef HAVE_BSD_LIBUTIL_H */

/* Define to 1 if you have the <bsm/audit.h> header file. */
/* #undef HAVE_BSM_AUDIT_H */

/* Define to 1 if you have the <bstring.h> header file. */
/* #undef HAVE_BSTRING_H */

/* Define to 1 if you have the `bzero' function. */
#define HAVE_BZERO 1

/* calloc(0, x) returns NULL */
#define HAVE_CALLOC 1

/* Define to 1 if you have the `cap_rights_limit' function. */
#define HAVE_CAP_RIGHTS_LIMIT 1

/* Define to 1 if you have the `clock' function. */
#define HAVE_CLOCK 1

/* Have clock_gettime */
#define HAVE_CLOCK_GETTIME 1

/* define if you have clock_t data type */
#define HAVE_CLOCK_T 1

/* Define to 1 if you have the `closefrom' function. */
#define HAVE_CLOSEFROM 1

/* Define if gai_strerror() returns const char * */
#define HAVE_CONST_GAI_STRERROR_PROTO 1

/* Define if your system uses ancillary data style file descriptor passing */
#define HAVE_CONTROL_IN_MSGHDR 1

/* Define to 1 if you have the `crypt' function. */
#define HAVE_CRYPT 1

/* Define to 1 if you have the <crypto/sha2.h> header file. */
/* #undef HAVE_CRYPTO_SHA2_H */

/* Define to 1 if you have the <crypt.h> header file. */
/* #undef HAVE_CRYPT_H */

/* Define if you are on Cygwin */
/* #undef HAVE_CYGWIN */

/* Define if your libraries define daemon() */
#define HAVE_DAEMON 1

/* Define to 1 if you have the declaration of `AI_NUMERICSERV', and to 0 if
   you don't. */
#define HAVE_DECL_AI_NUMERICSERV 1

/* Define to 1 if you have the declaration of `authenticate', and to 0 if you
   don't. */
/* #undef HAVE_DECL_AUTHENTICATE */

/* Define to 1 if you have the declaration of `bzero', and to 0 if you don't.
   */
#define HAVE_DECL_BZERO 1

/* Define to 1 if you have the declaration of `getpeereid', and to 0 if you
   don't. */
#define HAVE_DECL_GETPEEREID 1

/* Define to 1 if you have the declaration of `GLOB_NOMATCH', and to 0 if you
   don't. */
#define HAVE_DECL_GLOB_NOMATCH 1

/* Define to 1 if you have the declaration of `GSS_C_NT_HOSTBASED_SERVICE',
   and to 0 if you don't. */
/* #undef HAVE_DECL_GSS_C_NT_HOSTBASED_SERVICE */

/* Define to 1 if you have the declaration of `howmany', and to 0 if you
   don't. */
#define HAVE_DECL_HOWMANY 1

/* Define to 1 if you have the declaration of `h_errno', and to 0 if you
   don't. */
#define HAVE_DECL_H_ERRNO 1

/* Define to 1 if you have the declaration of `loginfailed', and to 0 if you
   don't. */
/* #undef HAVE_DECL_LOGINFAILED */

/* Define to 1 if you have the declaration of `loginrestrictions', and to 0 if
   you don't. */
/* #undef HAVE_DECL_LOGINRESTRICTIONS */

/* Define to 1 if you have the declaration of `loginsuccess', and to 0 if you
   don't. */
/* #undef HAVE_DECL_LOGINSUCCESS */

/* Define to 1 if you have the declaration of `MAXSYMLINKS', and to 0 if you
   don't. */
#define HAVE_DECL_MAXSYMLINKS 1

/* Define to 1 if you have the declaration of `memmem', and to 0 if you don't.
   */
#define HAVE_DECL_MEMMEM 1

/* Define to 1 if you have the declaration of `NFDBITS', and to 0 if you
   don't. */
#define HAVE_DECL_NFDBITS 1

/* Define to 1 if you have the declaration of `offsetof', and to 0 if you
   don't. */
#define HAVE_DECL_OFFSETOF 1

/* Define to 1 if you have the declaration of `O_NONBLOCK', and to 0 if you
   don't. */
#define HAVE_DECL_O_NONBLOCK 1

/* Define to 1 if you have the declaration of `passwdexpired', and to 0 if you
   don't. */
/* #undef HAVE_DECL_PASSWDEXPIRED */

/* Define to 1 if you have the declaration of `readv', and to 0 if you don't.
   */
#define HAVE_DECL_READV 1

/* Define to 1 if you have the declaration of `setauthdb', and to 0 if you
   don't. */
/* #undef HAVE_DECL_SETAUTHDB */

/* Define to 1 if you have the declaration of `SHUT_RD', and to 0 if you
   don't. */
#define HAVE_DECL_SHUT_RD 1

/* Define to 1 if you have the declaration of `UINT32_MAX', and to 0 if you
   don't. */
#define HAVE_DECL_UINT32_MAX 1

/* Define to 1 if you have the declaration of `writev', and to 0 if you don't.
   */
#define HAVE_DECL_WRITEV 1

/* Define to 1 if you have the declaration of `_getlong', and to 0 if you
   don't. */
#define HAVE_DECL__GETLONG 0

/* Define to 1 if you have the declaration of `_getshort', and to 0 if you
   don't. */
#define HAVE_DECL__GETSHORT 0

/* Define to 1 if you have the `DES_crypt' function. */
#define HAVE_DES_CRYPT 1

/* Define if you have /dev/ptmx */
/* #undef HAVE_DEV_PTMX */

/* Define if you have /dev/ptc */
/* #undef HAVE_DEV_PTS_AND_PTC */

/* Define to 1 if you have the `DH_get0_key' function. */
#define HAVE_DH_GET0_KEY 1

/* Define to 1 if you have the `DH_get0_pqg' function. */
#define HAVE_DH_GET0_PQG 1

/* Define to 1 if you have the `DH_set0_key' function. */
#define HAVE_DH_SET0_KEY 1

/* Define to 1 if you have the `DH_set0_pqg' function. */
#define HAVE_DH_SET0_PQG 1

/* Define to 1 if you have the `DH_set_length' function. */
#define HAVE_DH_SET_LENGTH 1

/* Define to 1 if you have the <dirent.h> header file. */
#define HAVE_DIRENT_H 1

/* Define to 1 if you have the `dirfd' function. */
#define HAVE_DIRFD 1

/* Define to 1 if you have the `dirname' function. */
#define HAVE_DIRNAME 1

/* Define to 1 if you have the `dlopen' function. */
#define HAVE_DLOPEN 1

/* Define to 1 if you have the `DSA_generate_parameters_ex' function. */
#define HAVE_DSA_GENERATE_PARAMETERS_EX 1

/* Define to 1 if you have the `DSA_get0_key' function. */
#define HAVE_DSA_GET0_KEY 1

/* Define to 1 if you have the `DSA_get0_pqg' function. */
#define HAVE_DSA_GET0_PQG 1

/* Define to 1 if you have the `DSA_set0_key' function. */
#define HAVE_DSA_SET0_KEY 1

/* Define to 1 if you have the `DSA_set0_pqg' function. */
#define HAVE_DSA_SET0_PQG 1

/* Define to 1 if you have the `DSA_SIG_get0' function. */
#define HAVE_DSA_SIG_GET0 1

/* Define to 1 if you have the `DSA_SIG_set0' function. */
#define HAVE_DSA_SIG_SET0 1

/* Define to 1 if you have the `ECDSA_SIG_get0' function. */
#define HAVE_ECDSA_SIG_GET0 1

/* Define to 1 if you have the `ECDSA_SIG_set0' function. */
#define HAVE_ECDSA_SIG_SET0 1

/* Define to 1 if you have the `EC_KEY_METHOD_new' function. */
#define HAVE_EC_KEY_METHOD_NEW 1

/* Define to 1 if you have the <elf.h> header file. */
#define HAVE_ELF_H 1

/* Define to 1 if you have the `endgrent' function. */
#define HAVE_ENDGRENT 1

/* Define to 1 if you have the <endian.h> header file. */
/* #undef HAVE_ENDIAN_H */

/* Define to 1 if you have the `endutent' function. */
/* #undef HAVE_ENDUTENT */

/* Define to 1 if you have the `endutxent' function. */
#define HAVE_ENDUTXENT 1

/* Define to 1 if you have the `err' function. */
#define HAVE_ERR 1

/* Define to 1 if you have the `errx' function. */
#define HAVE_ERRX 1

/* Define to 1 if you have the <err.h> header file. */
#define HAVE_ERR_H 1

/* Define if your system has /etc/default/login */
/* #undef HAVE_ETC_DEFAULT_LOGIN */

/* Define to 1 if you have the `EVP_chacha20' function. */
#define HAVE_EVP_CHACHA20 1

/* Define to 1 if you have the `EVP_CIPHER_CTX_ctrl' function. */
#define HAVE_EVP_CIPHER_CTX_CTRL 1

/* Define to 1 if you have the `EVP_CIPHER_CTX_get_iv' function. */
/* #undef HAVE_EVP_CIPHER_CTX_GET_IV */

/* Define to 1 if you have the `EVP_CIPHER_CTX_get_updated_iv' function. */
/* #undef HAVE_EVP_CIPHER_CTX_GET_UPDATED_IV */

/* Define to 1 if you have the `EVP_CIPHER_CTX_iv' function. */
#define HAVE_EVP_CIPHER_CTX_IV 1

/* Define to 1 if you have the `EVP_CIPHER_CTX_iv_noconst' function. */
#define HAVE_EVP_CIPHER_CTX_IV_NOCONST 1

/* Define to 1 if you have the `EVP_CIPHER_CTX_set_iv' function. */
/* #undef HAVE_EVP_CIPHER_CTX_SET_IV */

/* Define to 1 if you have the `EVP_DigestFinal_ex' function. */
#define HAVE_EVP_DIGESTFINAL_EX 1

/* Define to 1 if you have the `EVP_DigestInit_ex' function. */
#define HAVE_EVP_DIGESTINIT_EX 1

/* Define to 1 if you have the `EVP_MD_CTX_cleanup' function. */
/* #undef HAVE_EVP_MD_CTX_CLEANUP */

/* Define to 1 if you have the `EVP_MD_CTX_copy_ex' function. */
#define HAVE_EVP_MD_CTX_COPY_EX 1

/* Define to 1 if you have the `EVP_MD_CTX_free' function. */
#define HAVE_EVP_MD_CTX_FREE 1

/* Define to 1 if you have the `EVP_MD_CTX_init' function. */
/* #undef HAVE_EVP_MD_CTX_INIT */

/* Define to 1 if you have the `EVP_MD_CTX_new' function. */
#define HAVE_EVP_MD_CTX_NEW 1

/* Define to 1 if you have the `EVP_PKEY_get0_RSA' function. */
#define HAVE_EVP_PKEY_GET0_RSA 1

/* Define to 1 if you have the `EVP_sha256' function. */
#define HAVE_EVP_SHA256 1

/* Define to 1 if you have the `EVP_sha384' function. */
#define HAVE_EVP_SHA384 1

/* Define to 1 if you have the `EVP_sha512' function. */
#define HAVE_EVP_SHA512 1

/* Define if you have ut_exit in utmp.h */
/* #undef HAVE_EXIT_IN_UTMP */

/* Define to 1 if you have the `explicit_bzero' function. */
#define HAVE_EXPLICIT_BZERO 1

/* Define to 1 if you have the `explicit_memset' function. */
/* #undef HAVE_EXPLICIT_MEMSET */

/* Define to 1 if you have the `fchmod' function. */
#define HAVE_FCHMOD 1

/* Define to 1 if you have the `fchmodat' function. */
#define HAVE_FCHMODAT 1

/* Define to 1 if you have the `fchown' function. */
#define HAVE_FCHOWN 1

/* Define to 1 if you have the `fchownat' function. */
#define HAVE_FCHOWNAT 1

/* Use F_CLOSEM fcntl for closefrom */
/* #undef HAVE_FCNTL_CLOSEM */

/* Define to 1 if you have the <fcntl.h> header file. */
#define HAVE_FCNTL_H 1

/* Define to 1 if the system has the type `fd_mask'. */
#define HAVE_FD_MASK 1

/* Define to 1 if you have the <features.h> header file. */
/* #undef HAVE_FEATURES_H */

/* Define to 1 if you have the `fido_cred_prot' function. */
/* #undef HAVE_FIDO_CRED_PROT */

/* Define to 1 if you have the `fido_cred_set_prot' function. */
/* #undef HAVE_FIDO_CRED_SET_PROT */

/* Define to 1 if you have the `fido_dev_get_touch_begin' function. */
/* #undef HAVE_FIDO_DEV_GET_TOUCH_BEGIN */

/* Define to 1 if you have the `fido_dev_get_touch_status' function. */
/* #undef HAVE_FIDO_DEV_GET_TOUCH_STATUS */

/* Define to 1 if you have the `fido_dev_supports_cred_prot' function. */
/* #undef HAVE_FIDO_DEV_SUPPORTS_CRED_PROT */

/* Define to 1 if you have the <floatingpoint.h> header file. */
#define HAVE_FLOATINGPOINT_H 1

/* Define to 1 if you have the `flock' function. */
#define HAVE_FLOCK 1

/* Define to 1 if you have the `fmt_scaled' function. */
/* #undef HAVE_FMT_SCALED */

/* Define to 1 if you have the `fnmatch' function. */
#define HAVE_FNMATCH 1

/* Define to 1 if you have the <fnmatch.h> header file. */
#define HAVE_FNMATCH_H 1

/* Define to 1 if you have the `freeaddrinfo' function. */
#define HAVE_FREEADDRINFO 1

/* Define to 1 if you have the `freezero' function. */
/* #undef HAVE_FREEZERO */

/* Define to 1 if the system has the type `fsblkcnt_t'. */
#define HAVE_FSBLKCNT_T 1

/* Define to 1 if the system has the type `fsfilcnt_t'. */
#define HAVE_FSFILCNT_T 1

/* Define to 1 if you have the `fstatfs' function. */
#define HAVE_FSTATFS 1

/* Define to 1 if you have the `fstatvfs' function. */
#define HAVE_FSTATVFS 1

/* Define to 1 if you have the `futimes' function. */
#define HAVE_FUTIMES 1

/* Define to 1 if you have the `gai_strerror' function. */
#define HAVE_GAI_STRERROR 1

/* Define to 1 if you have the `getaddrinfo' function. */
#define HAVE_GETADDRINFO 1

/* Define to 1 if you have the `getaudit' function. */
/* #undef HAVE_GETAUDIT */

/* Define to 1 if you have the `getaudit_addr' function. */
/* #undef HAVE_GETAUDIT_ADDR */

/* Define to 1 if you have the `getcwd' function. */
#define HAVE_GETCWD 1

/* Define to 1 if you have the `getgrouplist' function. */
#define HAVE_GETGROUPLIST 1

/* Define to 1 if you have the `getgrset' function. */
/* #undef HAVE_GETGRSET */

/* Define to 1 if you have the `getlastlogxbyname' function. */
/* #undef HAVE_GETLASTLOGXBYNAME */

/* Define to 1 if you have the `getline' function. */
#define HAVE_GETLINE 1

/* Define to 1 if you have the `getluid' function. */
/* #undef HAVE_GETLUID */

/* Define to 1 if you have the `getnameinfo' function. */
#define HAVE_GETNAMEINFO 1

/* Define to 1 if you have the `getopt' function. */
#define HAVE_GETOPT 1

/* Define to 1 if you have the <getopt.h> header file. */
#define HAVE_GETOPT_H 1

/* Define if your getopt(3) defines and uses optreset */
#define HAVE_GETOPT_OPTRESET 1

/* Define if your libraries define getpagesize() */
#define HAVE_GETPAGESIZE 1

/* Define to 1 if you have the `getpeereid' function. */
#define HAVE_GETPEEREID 1

/* Define to 1 if you have the `getpeerucred' function. */
/* #undef HAVE_GETPEERUCRED */

/* Define to 1 if you have the `getpgid' function. */
#define HAVE_GETPGID 1

/* Define to 1 if you have the `getpgrp' function. */
#define HAVE_GETPGRP 1

/* Define to 1 if you have the `getpwanam' function. */
/* #undef HAVE_GETPWANAM */

/* Define to 1 if you have the `getrandom' function. */
#define HAVE_GETRANDOM 1

/* Define to 1 if you have the `getrlimit' function. */
#define HAVE_GETRLIMIT 1

/* Define if getrrsetbyname() exists */
/* #undef HAVE_GETRRSETBYNAME */

/* Define to 1 if you have the `getseuserbyname' function. */
/* #undef HAVE_GETSEUSERBYNAME */

/* Define to 1 if you have the `getsid' function. */
#define HAVE_GETSID 1

/* Define to 1 if you have the `gettimeofday' function. */
#define HAVE_GETTIMEOFDAY 1

/* Define to 1 if you have the `getttyent' function. */
#define HAVE_GETTTYENT 1

/* Define to 1 if you have the `getutent' function. */
/* #undef HAVE_GETUTENT */

/* Define to 1 if you have the `getutid' function. */
/* #undef HAVE_GETUTID */

/* Define to 1 if you have the `getutline' function. */
/* #undef HAVE_GETUTLINE */

/* Define to 1 if you have the `getutxent' function. */
#define HAVE_GETUTXENT 1

/* Define to 1 if you have the `getutxid' function. */
#define HAVE_GETUTXID 1

/* Define to 1 if you have the `getutxline' function. */
#define HAVE_GETUTXLINE 1

/* Define to 1 if you have the `getutxuser' function. */
#define HAVE_GETUTXUSER 1

/* Define to 1 if you have the `get_default_context_with_level' function. */
/* #undef HAVE_GET_DEFAULT_CONTEXT_WITH_LEVEL */

/* Define to 1 if you have the `glob' function. */
#define HAVE_GLOB 1

/* Define to 1 if you have the <glob.h> header file. */
#define HAVE_GLOB_H 1

/* Define to 1 if you have the `group_from_gid' function. */
#define HAVE_GROUP_FROM_GID 1

/* Define to 1 if you have the <gssapi_generic.h> header file. */
/* #undef HAVE_GSSAPI_GENERIC_H */

/* Define to 1 if you have the <gssapi/gssapi_generic.h> header file. */
/* #undef HAVE_GSSAPI_GSSAPI_GENERIC_H */

/* Define to 1 if you have the <gssapi/gssapi.h> header file. */
/* #undef HAVE_GSSAPI_GSSAPI_H */

/* Define to 1 if you have the <gssapi/gssapi_krb5.h> header file. */
/* #undef HAVE_GSSAPI_GSSAPI_KRB5_H */

/* Define to 1 if you have the <gssapi.h> header file. */
/* #undef HAVE_GSSAPI_H */

/* Define to 1 if you have the <gssapi_krb5.h> header file. */
/* #undef HAVE_GSSAPI_KRB5_H */

/* Define if HEADER.ad exists in arpa/nameser.h */
#define HAVE_HEADER_AD 1

/* Define to 1 if you have the `HMAC_CTX_init' function. */
/* #undef HAVE_HMAC_CTX_INIT */

/* Define if you have ut_host in utmp.h */
/* #undef HAVE_HOST_IN_UTMP */

/* Define if you have ut_host in utmpx.h */
#define HAVE_HOST_IN_UTMPX 1

/* Define to 1 if you have the <iaf.h> header file. */
/* #undef HAVE_IAF_H */

/* Define to 1 if you have the <ia.h> header file. */
/* #undef HAVE_IA_H */

/* Define if you have ut_id in utmp.h */
/* #undef HAVE_ID_IN_UTMP */

/* Define if you have ut_id in utmpx.h */
#define HAVE_ID_IN_UTMPX 1

/* Define to 1 if you have the <ifaddrs.h> header file. */
#define HAVE_IFADDRS_H 1

/* Define to 1 if you have the `inet_aton' function. */
#define HAVE_INET_ATON 1

/* Define to 1 if you have the `inet_ntoa' function. */
#define HAVE_INET_NTOA 1

/* Define to 1 if you have the `inet_ntop' function. */
#define HAVE_INET_NTOP 1

/* Define to 1 if you have the `innetgr' function. */
#define HAVE_INNETGR 1

/* define if you have int64_t data type */
#define HAVE_INT64_T 1

/* Define to 1 if the system has the type `intmax_t'. */
#define HAVE_INTMAX_T 1

/* Define to 1 if you have the <inttypes.h> header file. */
#define HAVE_INTTYPES_H 1

/* define if you have intxx_t data type */
#define HAVE_INTXX_T 1

/* Define to 1 if the system has the type `in_addr_t'. */
#define HAVE_IN_ADDR_T 1

/* Define to 1 if the system has the type `in_port_t'. */
#define HAVE_IN_PORT_T 1

/* Define if you have isblank(3C). */
#define HAVE_ISBLANK 1

/* Define to 1 if you have the `krb5_cc_new_unique' function. */
/* #undef HAVE_KRB5_CC_NEW_UNIQUE */

/* Define to 1 if you have the `krb5_free_error_message' function. */
/* #undef HAVE_KRB5_FREE_ERROR_MESSAGE */

/* Define to 1 if you have the `krb5_get_error_message' function. */
/* #undef HAVE_KRB5_GET_ERROR_MESSAGE */

/* Define to 1 if you have the <langinfo.h> header file. */
#define HAVE_LANGINFO_H 1

/* Define to 1 if you have the <lastlog.h> header file. */
/* #undef HAVE_LASTLOG_H */

/* Define if you want ldns support */
/* #undef HAVE_LDNS */

/* Define to 1 if you have the <libaudit.h> header file. */
/* #undef HAVE_LIBAUDIT_H */

/* Define to 1 if you have the `bsm' library (-lbsm). */
/* #undef HAVE_LIBBSM */

/* Define to 1 if you have the `crypt' library (-lcrypt). */
/* #undef HAVE_LIBCRYPT */

/* Define to 1 if you have the `dl' library (-ldl). */
#define HAVE_LIBDL 1

/* Define to 1 if you have the <libgen.h> header file. */
#define HAVE_LIBGEN_H 1

/* Define if system has libiaf that supports set_id */
/* #undef HAVE_LIBIAF */

/* Define to 1 if you have the `network' library (-lnetwork). */
/* #undef HAVE_LIBNETWORK */

/* Define to 1 if you have the `pam' library (-lpam). */
#define HAVE_LIBPAM 1

/* Define to 1 if you have the <libproc.h> header file. */
/* #undef HAVE_LIBPROC_H */

/* Define to 1 if you have the `socket' library (-lsocket). */
/* #undef HAVE_LIBSOCKET */

/* Define to 1 if you have the <libutil.h> header file. */
#define HAVE_LIBUTIL_H 1

/* Define to 1 if you have the `xnet' library (-lxnet). */
/* #undef HAVE_LIBXNET */

/* Define to 1 if you have the `z' library (-lz). */
#define HAVE_LIBZ 1

/* Define to 1 if you have the <limits.h> header file. */
#define HAVE_LIMITS_H 1

/* Define to 1 if you have the <linux/audit.h> header file. */
/* #undef HAVE_LINUX_AUDIT_H */

/* Define to 1 if you have the <linux/filter.h> header file. */
/* #undef HAVE_LINUX_FILTER_H */

/* Define to 1 if you have the <linux/if_tun.h> header file. */
/* #undef HAVE_LINUX_IF_TUN_H */

/* Define to 1 if you have the <linux/seccomp.h> header file. */
/* #undef HAVE_LINUX_SECCOMP_H */

/* Define to 1 if you have the `llabs' function. */
#define HAVE_LLABS 1

/* Define to 1 if you have the <locale.h> header file. */
#define HAVE_LOCALE_H 1

/* Define to 1 if you have the `localtime_r' function. */
#define HAVE_LOCALTIME_R 1

/* Define to 1 if you have the `login' function. */
/* #undef HAVE_LOGIN */

/* Define to 1 if you have the <login_cap.h> header file. */
#define HAVE_LOGIN_CAP_H 1

/* Define to 1 if you have the `login_getcapbool' function. */
#define HAVE_LOGIN_GETCAPBOOL 1

/* Define to 1 if you have the `login_getpwclass' function. */
#define HAVE_LOGIN_GETPWCLASS 1

/* Define to 1 if you have the <login.h> header file. */
/* #undef HAVE_LOGIN_H */

/* Define to 1 if you have the `logout' function. */
/* #undef HAVE_LOGOUT */

/* Define to 1 if you have the `logwtmp' function. */
/* #undef HAVE_LOGWTMP */

/* Define to 1 if the system has the type `long double'. */
#define HAVE_LONG_DOUBLE 1

/* Define to 1 if the system has the type `long long'. */
#define HAVE_LONG_LONG 1

/* Define to 1 if you have the <maillock.h> header file. */
/* #undef HAVE_MAILLOCK_H */

/* Define to 1 if your system has a GNU libc compatible `malloc' function, and
   to 0 otherwise. */
#define HAVE_MALLOC 1

/* Define to 1 if you have the `mblen' function. */
#define HAVE_MBLEN 1

/* Define to 1 if you have the `mbtowc' function. */
#define HAVE_MBTOWC 1

/* Define to 1 if you have the `md5_crypt' function. */
/* #undef HAVE_MD5_CRYPT */

/* Define if you want to allow MD5 passwords */
/* #undef HAVE_MD5_PASSWORDS */

/* Define to 1 if you have the `memmem' function. */
#define HAVE_MEMMEM 1

/* Define to 1 if you have the `memmove' function. */
#define HAVE_MEMMOVE 1

/* Define to 1 if you have the <memory.h> header file. */
#define HAVE_MEMORY_H 1

/* Define to 1 if you have the `memset_s' function. */
#define HAVE_MEMSET_S 1

/* Define to 1 if you have the `mkdtemp' function. */
#define HAVE_MKDTEMP 1

/* define if you have mode_t data type */
#define HAVE_MODE_T 1

/* Some systems put nanosleep outside of libc */
#define HAVE_NANOSLEEP 1

/* Define to 1 if you have the <ndir.h> header file. */
/* #undef HAVE_NDIR_H */

/* Define to 1 if you have the <netdb.h> header file. */
#define HAVE_NETDB_H 1

/* Define to 1 if you have the <netgroup.h> header file. */
/* #undef HAVE_NETGROUP_H */

/* Define to 1 if you have the <net/if_tun.h> header file. */
#define HAVE_NET_IF_TUN_H 1

/* Define to 1 if you have the <net/route.h> header file. */
#define HAVE_NET_ROUTE_H 1

/* Define if you are on NeXT */
/* #undef HAVE_NEXT */

/* Define to 1 if you have the `ngetaddrinfo' function. */
/* #undef HAVE_NGETADDRINFO */

/* Define to 1 if you have the `nl_langinfo' function. */
#define HAVE_NL_LANGINFO 1

/* Define to 1 if you have the `nsleep' function. */
/* #undef HAVE_NSLEEP */

/* Define to 1 if you have the `ogetaddrinfo' function. */
/* #undef HAVE_OGETADDRINFO */

/* Define if you have an old version of PAM which takes only one argument to
   pam_strerror */
/* #undef HAVE_OLD_PAM */

/* Define to 1 if you have the `openlog_r' function. */
/* #undef HAVE_OPENLOG_R */

/* Define to 1 if you have the `openpty' function. */
#define HAVE_OPENPTY 1

/* as a macro */
#define HAVE_OPENSSL_ADD_ALL_ALGORITHMS 1

/* Define to 1 if you have the `OPENSSL_init_crypto' function. */
#define HAVE_OPENSSL_INIT_CRYPTO 1

/* Define to 1 if you have the `OpenSSL_version' function. */
#define HAVE_OPENSSL_VERSION 1

/* Define to 1 if you have the `OpenSSL_version_num' function. */
#define HAVE_OPENSSL_VERSION_NUM 1

/* Define if you have Digital Unix Security Integration Architecture */
/* #undef HAVE_OSF_SIA */

/* Define to 1 if you have the `pam_getenvlist' function. */
#define HAVE_PAM_GETENVLIST 1

/* Define to 1 if you have the <pam/pam_appl.h> header file. */
/* #undef HAVE_PAM_PAM_APPL_H */

/* Define to 1 if you have the `pam_putenv' function. */
#define HAVE_PAM_PUTENV 1

/* Define to 1 if you have the <paths.h> header file. */
#define HAVE_PATHS_H 1

/* Define if you have ut_pid in utmp.h */
/* #undef HAVE_PID_IN_UTMP */

/* define if you have pid_t data type */
#define HAVE_PID_T 1

/* Define to 1 if you have the `pledge' function. */
/* #undef HAVE_PLEDGE */

/* Define to 1 if you have the `poll' function. */
#define HAVE_POLL 1

/* Define to 1 if you have the <poll.h> header file. */
#define HAVE_POLL_H 1

/* Define to 1 if you have the `prctl' function. */
/* #undef HAVE_PRCTL */

/* Define to 1 if you have the `priv_basicset' function. */
/* #undef HAVE_PRIV_BASICSET */

/* Define to 1 if you have the <priv.h> header file. */
/* #undef HAVE_PRIV_H */

/* Define to 1 if you have the `procctl' function. */
#define HAVE_PROCCTL 1

/* Define if you have /proc/$pid/fd */
/* #undef HAVE_PROC_PID */

/* Define to 1 if you have the `proc_pidinfo' function. */
/* #undef HAVE_PROC_PIDINFO */

/* Define to 1 if you have the `pselect' function. */
#define HAVE_PSELECT 1

/* Define to 1 if you have the `pstat' function. */
/* #undef HAVE_PSTAT */

/* Define to 1 if you have the <pty.h> header file. */
/* #undef HAVE_PTY_H */

/* Define to 1 if you have the `pututline' function. */
/* #undef HAVE_PUTUTLINE */

/* Define to 1 if you have the `pututxline' function. */
#define HAVE_PUTUTXLINE 1

/* Define to 1 if you have the `raise' function. */
#define HAVE_RAISE 1

/* Define to 1 if you have the `readpassphrase' function. */
#define HAVE_READPASSPHRASE 1

/* Define to 1 if you have the <readpassphrase.h> header file. */
#define HAVE_READPASSPHRASE_H 1

/* Define to 1 if your system has a GNU libc compatible `realloc' function,
   and to 0 otherwise. */
#define HAVE_REALLOC 1

/* Define to 1 if you have the `reallocarray' function. */
#define HAVE_REALLOCARRAY 1

/* Define to 1 if you have the `realpath' function. */
#define HAVE_REALPATH 1

/* Define to 1 if you have the `recallocarray' function. */
/* #undef HAVE_RECALLOCARRAY */

/* Define to 1 if you have the `recvmsg' function. */
#define HAVE_RECVMSG 1

/* sys/resource.h has RLIMIT_NPROC */
#define HAVE_RLIMIT_NPROC /**/

/* Define to 1 if you have the <rpc/types.h> header file. */
#define HAVE_RPC_TYPES_H 1

/* Define to 1 if you have the `rresvport_af' function. */
#define HAVE_RRESVPORT_AF 1

/* Define to 1 if you have the `RSA_generate_key_ex' function. */
#define HAVE_RSA_GENERATE_KEY_EX 1

/* Define to 1 if you have the `RSA_get0_crt_params' function. */
#define HAVE_RSA_GET0_CRT_PARAMS 1

/* Define to 1 if you have the `RSA_get0_factors' function. */
#define HAVE_RSA_GET0_FACTORS 1

/* Define to 1 if you have the `RSA_get0_key' function. */
#define HAVE_RSA_GET0_KEY 1

/* Define to 1 if you have the `RSA_get_default_method' function. */
#define HAVE_RSA_GET_DEFAULT_METHOD 1

/* Define to 1 if you have the `RSA_meth_dup' function. */
#define HAVE_RSA_METH_DUP 1

/* Define to 1 if you have the `RSA_meth_free' function. */
#define HAVE_RSA_METH_FREE 1

/* Define to 1 if you have the `RSA_meth_get_finish' function. */
#define HAVE_RSA_METH_GET_FINISH 1

/* Define to 1 if you have the `RSA_meth_set1_name' function. */
#define HAVE_RSA_METH_SET1_NAME 1

/* Define to 1 if you have the `RSA_meth_set_finish' function. */
#define HAVE_RSA_METH_SET_FINISH 1

/* Define to 1 if you have the `RSA_meth_set_priv_dec' function. */
#define HAVE_RSA_METH_SET_PRIV_DEC 1

/* Define to 1 if you have the `RSA_meth_set_priv_enc' function. */
#define HAVE_RSA_METH_SET_PRIV_ENC 1

/* Define to 1 if you have the `RSA_set0_crt_params' function. */
#define HAVE_RSA_SET0_CRT_PARAMS 1

/* Define to 1 if you have the `RSA_set0_factors' function. */
#define HAVE_RSA_SET0_FACTORS 1

/* Define to 1 if you have the `RSA_set0_key' function. */
#define HAVE_RSA_SET0_KEY 1

/* Define to 1 if you have the <sandbox.h> header file. */
/* #undef HAVE_SANDBOX_H */

/* Define to 1 if you have the `sandbox_init' function. */
/* #undef HAVE_SANDBOX_INIT */

/* define if you have sa_family_t data type */
#define HAVE_SA_FAMILY_T 1

/* Define to 1 if you have the `scan_scaled' function. */
/* #undef HAVE_SCAN_SCALED */

/* Define if you have SecureWare-based protected password database */
/* #undef HAVE_SECUREWARE */

/* Define to 1 if you have the <security/pam_appl.h> header file. */
#define HAVE_SECURITY_PAM_APPL_H 1

/* Define to 1 if you have the `sendmsg' function. */
#define HAVE_SENDMSG 1

/* Define to 1 if you have the `setauthdb' function. */
/* #undef HAVE_SETAUTHDB */

/* Define to 1 if you have the `setdtablesize' function. */
/* #undef HAVE_SETDTABLESIZE */

/* Define to 1 if you have the `setegid' function. */
#define HAVE_SETEGID 1

/* Define to 1 if you have the `setenv' function. */
#define HAVE_SETENV 1

/* Define to 1 if you have the `seteuid' function. */
#define HAVE_SETEUID 1

/* Define to 1 if you have the `setgroupent' function. */
#define HAVE_SETGROUPENT 1

/* Define to 1 if you have the `setgroups' function. */
#define HAVE_SETGROUPS 1

/* Define to 1 if you have the `setlinebuf' function. */
#define HAVE_SETLINEBUF 1

/* Define to 1 if you have the `setlogin' function. */
#define HAVE_SETLOGIN 1

/* Define to 1 if you have the `setluid' function. */
/* #undef HAVE_SETLUID */

/* Define to 1 if you have the `setpassent' function. */
#define HAVE_SETPASSENT 1

/* Define to 1 if you have the `setpcred' function. */
/* #undef HAVE_SETPCRED */

/* Define to 1 if you have the `setpflags' function. */
/* #undef HAVE_SETPFLAGS */

/* Define to 1 if you have the `setppriv' function. */
/* #undef HAVE_SETPPRIV */

/* Define to 1 if you have the `setproctitle' function. */
#define HAVE_SETPROCTITLE 1

/* Define to 1 if you have the `setregid' function. */
#define HAVE_SETREGID 1

/* Define to 1 if you have the `setresgid' function. */
#define HAVE_SETRESGID 1

/* Define to 1 if you have the `setresuid' function. */
#define HAVE_SETRESUID 1

/* Define to 1 if you have the `setreuid' function. */
#define HAVE_SETREUID 1

/* Define to 1 if you have the `setrlimit' function. */
#define HAVE_SETRLIMIT 1

/* Define to 1 if you have the `setsid' function. */
#define HAVE_SETSID 1

/* Define to 1 if you have the `setutent' function. */
/* #undef HAVE_SETUTENT */

/* Define to 1 if you have the `setutxdb' function. */
#define HAVE_SETUTXDB 1

/* Define to 1 if you have the `setutxent' function. */
#define HAVE_SETUTXENT 1

/* Define to 1 if you have the `setvbuf' function. */
#define HAVE_SETVBUF 1

/* Define to 1 if you have the `set_id' function. */
/* #undef HAVE_SET_ID */

/* Define to 1 if you have the `SHA256Update' function. */
/* #undef HAVE_SHA256UPDATE */

/* Define to 1 if you have the <sha2.h> header file. */
/* #undef HAVE_SHA2_H */

/* Define to 1 if you have the `SHA384Update' function. */
/* #undef HAVE_SHA384UPDATE */

/* Define to 1 if you have the `SHA512Update' function. */
/* #undef HAVE_SHA512UPDATE */

/* Define to 1 if you have the <shadow.h> header file. */
/* #undef HAVE_SHADOW_H */

/* Define to 1 if you have the `sigaction' function. */
#define HAVE_SIGACTION 1

/* Define to 1 if the system has the type `sighandler_t'. */
/* #undef HAVE_SIGHANDLER_T */

/* Define to 1 if you have the `sigvec' function. */
#define HAVE_SIGVEC 1

/* Define to 1 if the system has the type `sig_atomic_t'. */
#define HAVE_SIG_ATOMIC_T 1

/* define if you have size_t data type */
#define HAVE_SIZE_T 1

/* Define to 1 if you have the `snprintf' function. */
#define HAVE_SNPRINTF 1

/* Define to 1 if you have the `socketpair' function. */
#define HAVE_SOCKETPAIR 1

/* Have PEERCRED socket option */
/* #undef HAVE_SO_PEERCRED */

/* define if you have ssize_t data type */
#define HAVE_SSIZE_T 1

/* Fields in struct sockaddr_storage */
#define HAVE_SS_FAMILY_IN_SS 1

/* Define if you have ut_ss in utmpx.h */
/* #undef HAVE_SS_IN_UTMPX */

/* Define to 1 if you have the `statfs' function. */
#define HAVE_STATFS 1

/* Define to 1 if you have the `statvfs' function. */
#define HAVE_STATVFS 1

/* Define to 1 if you have the <stddef.h> header file. */
#define HAVE_STDDEF_H 1

/* Define to 1 if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

/* Define to 1 if you have the `strcasestr' function. */
#define HAVE_STRCASESTR 1

/* Define to 1 if you have the `strdup' function. */
#define HAVE_STRDUP 1

/* Define to 1 if you have the `strerror' function. */
#define HAVE_STRERROR 1

/* Define to 1 if you have the `strftime' function. */
#define HAVE_STRFTIME 1

/* Define to 1 if you have the <strings.h> header file. */
#define HAVE_STRINGS_H 1

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define to 1 if you have the `strlcat' function. */
#define HAVE_STRLCAT 1

/* Define to 1 if you have the `strlcpy' function. */
#define HAVE_STRLCPY 1

/* Define to 1 if you have the `strmode' function. */
#define HAVE_STRMODE 1

/* Define to 1 if you have the `strndup' function. */
#define HAVE_STRNDUP 1

/* Define to 1 if you have the `strnlen' function. */
#define HAVE_STRNLEN 1

/* Define to 1 if you have the `strnvis' function. */
#define HAVE_STRNVIS 1

/* Define to 1 if you have the `strptime' function. */
#define HAVE_STRPTIME 1

/* Define to 1 if you have the `strsep' function. */
#define HAVE_STRSEP 1

/* Define to 1 if you have the `strsignal' function. */
#define HAVE_STRSIGNAL 1

/* Define to 1 if you have the `strtoll' function. */
#define HAVE_STRTOLL 1

/* Define to 1 if you have the `strtonum' function. */
#define HAVE_STRTONUM 1

/* Define to 1 if you have the `strtoul' function. */
#define HAVE_STRTOUL 1

/* Define to 1 if you have the `strtoull' function. */
#define HAVE_STRTOULL 1

/* define if you have struct addrinfo data type */
#define HAVE_STRUCT_ADDRINFO 1

/* define if you have struct in6_addr data type */
#define HAVE_STRUCT_IN6_ADDR 1

/* Define to 1 if `pw_change' is a member of `struct passwd'. */
#define HAVE_STRUCT_PASSWD_PW_CHANGE 1

/* Define to 1 if `pw_class' is a member of `struct passwd'. */
#define HAVE_STRUCT_PASSWD_PW_CLASS 1

/* Define to 1 if `pw_expire' is a member of `struct passwd'. */
#define HAVE_STRUCT_PASSWD_PW_EXPIRE 1

/* Define to 1 if `pw_gecos' is a member of `struct passwd'. */
#define HAVE_STRUCT_PASSWD_PW_GECOS 1

/* define if you have struct sockaddr_in6 data type */
#define HAVE_STRUCT_SOCKADDR_IN6 1

/* Define to 1 if `sin6_scope_id' is a member of `struct sockaddr_in6'. */
#define HAVE_STRUCT_SOCKADDR_IN6_SIN6_SCOPE_ID 1

/* define if you have struct sockaddr_storage data type */
#define HAVE_STRUCT_SOCKADDR_STORAGE 1

/* Define to 1 if `f_files' is a member of `struct statfs'. */
#define HAVE_STRUCT_STATFS_F_FILES 1

/* Define to 1 if `f_flags' is a member of `struct statfs'. */
#define HAVE_STRUCT_STATFS_F_FLAGS 1

/* Define to 1 if `st_blksize' is a member of `struct stat'. */
#define HAVE_STRUCT_STAT_ST_BLKSIZE 1

/* Define to 1 if `st_mtim' is a member of `struct stat'. */
#define HAVE_STRUCT_STAT_ST_MTIM 1

/* Define to 1 if `st_mtime' is a member of `struct stat'. */
#define HAVE_STRUCT_STAT_ST_MTIME 1

/* define if you have struct timespec */
#define HAVE_STRUCT_TIMESPEC 1

/* define if you have struct timeval */
#define HAVE_STRUCT_TIMEVAL 1

/* Define to 1 if you have the `swap32' function. */
/* #undef HAVE_SWAP32 */

/* Define to 1 if you have the `sysconf' function. */
#define HAVE_SYSCONF 1

/* Define if you have syslen in utmpx.h */
/* #undef HAVE_SYSLEN_IN_UTMPX */

/* Define to 1 if you have the <sys/audit.h> header file. */
/* #undef HAVE_SYS_AUDIT_H */

/* Define to 1 if you have the <sys/bitypes.h> header file. */
/* #undef HAVE_SYS_BITYPES_H */

/* Define to 1 if you have the <sys/bsdtty.h> header file. */
/* #undef HAVE_SYS_BSDTTY_H */

/* Define to 1 if you have the <sys/byteorder.h> header file. */
/* #undef HAVE_SYS_BYTEORDER_H */

/* Define to 1 if you have the <sys/capsicum.h> header file. */
#define HAVE_SYS_CAPSICUM_H 1

/* Define to 1 if you have the <sys/cdefs.h> header file. */
#define HAVE_SYS_CDEFS_H 1

/* Define to 1 if you have the <sys/dir.h> header file. */
#define HAVE_SYS_DIR_H 1

/* Define if your system defines sys_errlist[] */
#define HAVE_SYS_ERRLIST 1

/* Define to 1 if you have the <sys/file.h> header file. */
#define HAVE_SYS_FILE_H 1

/* Define to 1 if you have the <sys/label.h> header file. */
/* #undef HAVE_SYS_LABEL_H */

/* Define to 1 if you have the <sys/mman.h> header file. */
#define HAVE_SYS_MMAN_H 1

/* Define to 1 if you have the <sys/mount.h> header file. */
#define HAVE_SYS_MOUNT_H 1

/* Define to 1 if you have the <sys/ndir.h> header file. */
/* #undef HAVE_SYS_NDIR_H */

/* Define if your system defines sys_nerr */
/* #undef HAVE_SYS_NERR */

/* Define to 1 if you have the <sys/poll.h> header file. */
#define HAVE_SYS_POLL_H 1

/* Define to 1 if you have the <sys/prctl.h> header file. */
/* #undef HAVE_SYS_PRCTL_H */

/* Define to 1 if you have the <sys/procctl.h> header file. */
#define HAVE_SYS_PROCCTL_H 1

/* Define to 1 if you have the <sys/pstat.h> header file. */
/* #undef HAVE_SYS_PSTAT_H */

/* Define to 1 if you have the <sys/ptms.h> header file. */
/* #undef HAVE_SYS_PTMS_H */

/* Define to 1 if you have the <sys/ptrace.h> header file. */
#define HAVE_SYS_PTRACE_H 1

/* Define to 1 if you have the <sys/random.h> header file. */
#define HAVE_SYS_RANDOM_H 1

/* Define to 1 if you have the <sys/select.h> header file. */
#define HAVE_SYS_SELECT_H 1

/* Define to 1 if you have the <sys/statvfs.h> header file. */
#define HAVE_SYS_STATVFS_H 1

/* Define to 1 if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 1

/* Define to 1 if you have the <sys/stream.h> header file. */
/* #undef HAVE_SYS_STREAM_H */

/* Define to 1 if you have the <sys/stropts.h> header file. */
/* #undef HAVE_SYS_STROPTS_H */

/* Define to 1 if you have the <sys/strtio.h> header file. */
/* #undef HAVE_SYS_STRTIO_H */

/* Define to 1 if you have the <sys/sysctl.h> header file. */
#define HAVE_SYS_SYSCTL_H 1

/* Force use of sys/syslog.h on Ultrix */
/* #undef HAVE_SYS_SYSLOG_H */

/* Define to 1 if you have the <sys/sysmacros.h> header file. */
/* #undef HAVE_SYS_SYSMACROS_H */

/* Define to 1 if you have the <sys/timers.h> header file. */
#define HAVE_SYS_TIMERS_H 1

/* Define to 1 if you have the <sys/time.h> header file. */
#define HAVE_SYS_TIME_H 1

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Define to 1 if you have the <sys/un.h> header file. */
#define HAVE_SYS_UN_H 1

/* Define to 1 if you have the <sys/vfs.h> header file. */
/* #undef HAVE_SYS_VFS_H */

/* Define to 1 if you have the `tcgetpgrp' function. */
#define HAVE_TCGETPGRP 1

/* Define to 1 if you have the `tcsendbreak' function. */
#define HAVE_TCSENDBREAK 1

/* Define to 1 if you have the `time' function. */
#define HAVE_TIME 1

/* Define to 1 if you have the <time.h> header file. */
#define HAVE_TIME_H 1

/* Define if you have ut_time in utmp.h */
/* #undef HAVE_TIME_IN_UTMP */

/* Define if you have ut_time in utmpx.h */
/* #undef HAVE_TIME_IN_UTMPX */

/* Define to 1 if you have the `timingsafe_bcmp' function. */
#define HAVE_TIMINGSAFE_BCMP 1

/* Define to 1 if you have the <tmpdir.h> header file. */
/* #undef HAVE_TMPDIR_H */

/* Define to 1 if you have the `truncate' function. */
#define HAVE_TRUNCATE 1

/* Define to 1 if you have the <ttyent.h> header file. */
#define HAVE_TTYENT_H 1

/* Define if you have ut_tv in utmp.h */
/* #undef HAVE_TV_IN_UTMP */

/* Define if you have ut_tv in utmpx.h */
#define HAVE_TV_IN_UTMPX 1

/* Define if you have ut_type in utmp.h */
/* #undef HAVE_TYPE_IN_UTMP */

/* Define if you have ut_type in utmpx.h */
#define HAVE_TYPE_IN_UTMPX 1

/* Define to 1 if you have the <ucred.h> header file. */
/* #undef HAVE_UCRED_H */

/* Define to 1 if the system has the type `uintmax_t'. */
#define HAVE_UINTMAX_T 1

/* define if you have uintxx_t data type */
#define HAVE_UINTXX_T 1

/* Define to 1 if you have the <unistd.h> header file. */
#define HAVE_UNISTD_H 1

/* Define to 1 if you have the `unsetenv' function. */
#define HAVE_UNSETENV 1

/* Define to 1 if the system has the type `unsigned long long'. */
#define HAVE_UNSIGNED_LONG_LONG 1

/* Define to 1 if you have the `updwtmp' function. */
/* #undef HAVE_UPDWTMP */

/* Define to 1 if you have the `updwtmpx' function. */
/* #undef HAVE_UPDWTMPX */

/* Define to 1 if you have the <usersec.h> header file. */
/* #undef HAVE_USERSEC_H */

/* Define to 1 if you have the `user_from_uid' function. */
#define HAVE_USER_FROM_UID 1

/* Define to 1 if you have the `usleep' function. */
#define HAVE_USLEEP 1

/* Define to 1 if you have the <util.h> header file. */
/* #undef HAVE_UTIL_H */

/* Define to 1 if you have the `utimensat' function. */
#define HAVE_UTIMENSAT 1

/* Define to 1 if you have the `utimes' function. */
#define HAVE_UTIMES 1

/* Define to 1 if you have the <utime.h> header file. */
#define HAVE_UTIME_H 1

/* Define to 1 if you have the `utmpname' function. */
/* #undef HAVE_UTMPNAME */

/* Define to 1 if you have the `utmpxname' function. */
/* #undef HAVE_UTMPXNAME */

/* Define to 1 if you have the <utmpx.h> header file. */
#define HAVE_UTMPX_H 1

/* Define to 1 if you have the <utmp.h> header file. */
/* #undef HAVE_UTMP_H */

/* define if you have u_char data type */
#define HAVE_U_CHAR 1

/* define if you have u_int data type */
#define HAVE_U_INT 1

/* define if you have u_int64_t data type */
#define HAVE_U_INT64_T 1

/* define if you have u_intxx_t data type */
#define HAVE_U_INTXX_T 1

/* Define to 1 if you have the `vasprintf' function. */
#define HAVE_VASPRINTF 1

/* Define if va_copy exists */
#define HAVE_VA_COPY 1

/* Define to 1 if you have the <vis.h> header file. */
#define HAVE_VIS_H 1

/* Define to 1 if you have the `vsnprintf' function. */
#define HAVE_VSNPRINTF 1

/* Define to 1 if you have the `waitpid' function. */
#define HAVE_WAITPID 1

/* Define to 1 if you have the `warn' function. */
#define HAVE_WARN 1

/* Define to 1 if you have the <wchar.h> header file. */
#define HAVE_WCHAR_H 1

/* Define to 1 if you have the `wcwidth' function. */
#define HAVE_WCWIDTH 1

/* Define to 1 if you have the `_getlong' function. */
#define HAVE__GETLONG 1

/* Define to 1 if you have the `_getpty' function. */
/* #undef HAVE__GETPTY */

/* Define to 1 if you have the `_getshort' function. */
#define HAVE__GETSHORT 1

/* Define if you have struct __res_state _res as an extern */
#define HAVE__RES_EXTERN 1

/* Define to 1 if you have the `__b64_ntop' function. */
#define HAVE___B64_NTOP 1

/* Define to 1 if you have the `__b64_pton' function. */
#define HAVE___B64_PTON 1

/* Define if compiler implements __FUNCTION__ */
#define HAVE___FUNCTION__ 1

/* Define if libc defines __progname */
#define HAVE___PROGNAME 1

/* Fields in struct sockaddr_storage */
/* #undef HAVE___SS_FAMILY_IN_SS */

/* Define if __va_copy exists */
#define HAVE___VA_COPY 1

/* Define if compiler implements __func__ */
#define HAVE___func__ 1

/* Define this if you are using the Heimdal version of Kerberos V5 */
/* #undef HEIMDAL */

/* Define if you need to use IP address instead of hostname in $DISPLAY */
/* #undef IPADDR_IN_DISPLAY */

/* Detect IPv4 in IPv6 mapped addresses and treat as IPv4 */
/* #undef IPV4_IN_IPV6 */

/* Define if your system choked on IP TOS setting */
/* #undef IP_TOS_IS_BROKEN */

/* Define if you want Kerberos 5 support */
/* #undef KRB5 */

/* Define if pututxline updates lastlog too */
/* #undef LASTLOG_WRITE_PUTUTXLINE */

/* Define if you want TCP Wrappers support */
/* #undef LIBWRAP */

/* Define to whatever link() returns for "not supported" if it doesn't return
   EOPNOTSUPP. */
/* #undef LINK_OPNOTSUPP_ERRNO */

/* Adjust Linux out-of-memory killer */
/* #undef LINUX_OOM_ADJUST */

/* max value of long long calculated by configure */
/* #undef LLONG_MAX */

/* min value of long long calculated by configure */
/* #undef LLONG_MIN */

/* Account locked with pw(1) */
#define LOCKED_PASSWD_PREFIX "*LOCKED*"

/* String used in /etc/passwd to denote locked account */
/* #undef LOCKED_PASSWD_STRING */

/* String used in /etc/passwd to denote locked account */
/* #undef LOCKED_PASSWD_SUBSTR */

/* Some systems need a utmpx entry for /bin/login to work */
/* #undef LOGIN_NEEDS_UTMPX */

/* Set this to your mail directory if you do not have _PATH_MAILDIR */
/* #undef MAIL_DIRECTORY */

/* Need setpgrp to for controlling tty */
/* #undef NEED_SETPGRP */

/* compiler does not accept __attribute__ on prototype args */
/* #undef NO_ATTRIBUTE_ON_PROTOTYPE_ARGS */

/* compiler does not accept __attribute__ on return types */
/* #undef NO_ATTRIBUTE_ON_RETURN_TYPE */

/* SA_RESTARTed signals do no interrupt select */
/* #undef NO_SA_RESTART */

/* Define to disable UID restoration test */
/* #undef NO_UID_RESTORATION_TEST */

/* Define if X11 doesn't support AF_UNIX sockets on that system */
/* #undef NO_X11_UNIX_SOCKETS */

/* Define if EVP_DigestUpdate returns void */
/* #undef OPENSSL_EVP_DIGESTUPDATE_VOID */

/* OpenSSL has ECC */
#define OPENSSL_HAS_ECC 1

/* libcrypto has NID_X9_62_prime256v1 */
#define OPENSSL_HAS_NISTP256 1

/* libcrypto has NID_secp384r1 */
#define OPENSSL_HAS_NISTP384 1

/* libcrypto has NID_secp521r1 */
#define OPENSSL_HAS_NISTP521 1

/* libcrypto has EVP AES CTR */
#define OPENSSL_HAVE_EVPCTR 1

/* libcrypto has EVP AES GCM */
#define OPENSSL_HAVE_EVPGCM 1

/* libcrypto is missing AES 192 and 256 bit functions */
/* #undef OPENSSL_LOBOTOMISED_AES */

/* Define if you want the OpenSSL internally seeded PRNG only */
#define OPENSSL_PRNG_ONLY 1

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT "openssh-unix-dev@mindrot.org"

/* Define to the full name of this package. */
#define PACKAGE_NAME "OpenSSH"

/* Define to the full name and version of this package. */
#define PACKAGE_STRING "OpenSSH Portable"

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME "openssh"

/* Define to the home page for this package. */
#define PACKAGE_URL ""

/* Define to the version of this package. */
#define PACKAGE_VERSION "Portable"

/* Define if you are using Solaris-derived PAM which passes pam_messages to
   the conversation function with an extra level of indirection */
/* #undef PAM_SUN_CODEBASE */

/* Work around problematic Linux PAM modules handling of PAM_TTY */
/* #undef PAM_TTY_KLUDGE */

/* must supply username to passwd */
/* #undef PASSWD_NEEDS_USERNAME */

/* System dirs owned by bin (uid 2) */
/* #undef PLATFORM_SYS_DIR_UID */

/* Port number of PRNGD/EGD random number socket */
/* #undef PRNGD_PORT */

/* Location of PRNGD/EGD random number socket */
/* #undef PRNGD_SOCKET */

/* read(1) can return 0 for a non-closed fd */
/* #undef PTY_ZEROREAD */

/* Sandbox using capsicum */
#define SANDBOX_CAPSICUM 1

/* Sandbox using Darwin sandbox_init(3) */
/* #undef SANDBOX_DARWIN */

/* no privsep sandboxing */
/* #undef SANDBOX_NULL */

/* Sandbox using pledge(2) */
/* #undef SANDBOX_PLEDGE */

/* Sandbox using setrlimit(2) */
/* #undef SANDBOX_RLIMIT */

/* Sandbox using seccomp filter */
/* #undef SANDBOX_SECCOMP_FILTER */

/* setrlimit RLIMIT_FSIZE works */
/* #undef SANDBOX_SKIP_RLIMIT_FSIZE */

/* define if setrlimit RLIMIT_NOFILE breaks things */
#define SANDBOX_SKIP_RLIMIT_NOFILE 1

/* Sandbox using Solaris/Illumos privileges */
/* #undef SANDBOX_SOLARIS */

/* Sandbox using systrace(4) */
/* #undef SANDBOX_SYSTRACE */

/* Specify the system call convention in use */
/* #undef SECCOMP_AUDIT_ARCH */

/* Define if your platform breaks doing a seteuid before a setuid */
/* #undef SETEUID_BREAKS_SETUID */

/* The size of `int', as computed by sizeof. */
#define SIZEOF_INT 4

/* The size of `long int', as computed by sizeof. */
#define SIZEOF_LONG_INT 8

/* The size of `long long int', as computed by sizeof. */
#define SIZEOF_LONG_LONG_INT 8

/* The size of `short int', as computed by sizeof. */
#define SIZEOF_SHORT_INT 2

/* The size of `time_t', as computed by sizeof. */
#define SIZEOF_TIME_T 8

/* Define as const if snprintf() can declare const char *fmt */
#define SNPRINTF_CONST const

/* Define to a Set Process Title type if your system is supported by
   bsd-setproctitle.c */
/* #undef SPT_TYPE */

/* Define if sshd somehow reacquires a controlling TTY after setsid() */
/* #undef SSHD_ACQUIRES_CTTY */

/* sshd PAM service name */
/* #undef SSHD_PAM_SERVICE */

/* Define if pam_chauthtok wants real uid set to the unpriv'ed user */
/* #undef SSHPAM_CHAUTHTOK_NEEDS_RUID */

/* Use audit debugging module */
/* #undef SSH_AUDIT_EVENTS */

/* Windows is sensitive to read buffer size */
/* #undef SSH_IOBUFSZ */

/* non-privileged user for privilege separation */
#define SSH_PRIVSEP_USER "sshd"

/* Use tunnel device compatibility to OpenBSD */
/* #undef SSH_TUN_COMPAT_AF */

/* Open tunnel devices the FreeBSD way */
#define SSH_TUN_FREEBSD 1

/* Open tunnel devices the Linux tun/tap way */
/* #undef SSH_TUN_LINUX */

/* No layer 2 tunnel support */
/* #undef SSH_TUN_NO_L2 */

/* Open tunnel devices the OpenBSD way */
/* #undef SSH_TUN_OPENBSD */

/* Prepend the address family to IP tunnel traffic */
/* #undef SSH_TUN_PREPEND_AF */

/* Define to 1 if you have the ANSI C header files. */
#define STDC_HEADERS 1

/* Define if you want a different $PATH for the superuser */
/* #undef SUPERUSER_PATH */

/* syslog_r function is safe to use in in a signal handler */
/* #undef SYSLOG_R_SAFE_IN_SIGHAND */

/* Support routing domains using Linux VRF */
/* #undef SYS_RDOMAIN_LINUX */

/* Support passwords > 8 chars */
/* #undef UNIXWARE_LONG_PASSWORDS */

/* Specify default $PATH */
/* #undef USER_PATH */

/* Define this if you want to use libkafs' AFS support */
/* #undef USE_AFS */

/* Use BSM audit module */
/* #undef USE_BSM_AUDIT */

/* Use btmp to log bad logins */
/* #undef USE_BTMP */

/* Use libedit for sftp */
#define USE_LIBEDIT 1

/* Use Linux audit module */
/* #undef USE_LINUX_AUDIT */

/* Enable OpenSSL engine support */
#define USE_OPENSSL_ENGINE 1

/* Define if you want to enable PAM support */
#define USE_PAM 1

/* Use PIPES instead of a socketpair() */
/* #undef USE_PIPES */

/* Define if you have Solaris privileges */
/* #undef USE_SOLARIS_PRIVS */

/* Define if you have Solaris process contracts */
/* #undef USE_SOLARIS_PROCESS_CONTRACTS */

/* Define if you have Solaris projects */
/* #undef USE_SOLARIS_PROJECTS */

/* compiler variable declarations after code */
#define VARIABLE_DECLARATION_AFTER_CODE 1

/* compiler supports variable length arrays */
#define VARIABLE_LENGTH_ARRAYS 1

/* Define if you shouldn't strip 'tty' from your ttyname in [uw]tmp */
/* #undef WITH_ABBREV_NO_TTY */

/* Define if you want to enable AIX4's authenticate function */
/* #undef WITH_AIXAUTHENTICATE */

/* Define if you have/want arrays (cluster-wide session management, not C
   arrays) */
/* #undef WITH_IRIX_ARRAY */

/* Define if you want IRIX audit trails */
/* #undef WITH_IRIX_AUDIT */

/* Define if you want IRIX kernel jobs */
/* #undef WITH_IRIX_JOBS */

/* Define if you want IRIX project management */
/* #undef WITH_IRIX_PROJECT */

/* use libcrypto for cryptography */
#define WITH_OPENSSL 1

/* Define if you want SELinux support. */
/* #undef WITH_SELINUX */

/* Enable zlib */
#define WITH_ZLIB 1

/* Define WORDS_BIGENDIAN to 1 if your processor stores words with the most
   significant byte first (like Motorola and SPARC, unlike Intel). */
#if defined AC_APPLE_UNIVERSAL_BUILD
# if defined __BIG_ENDIAN__
#  define WORDS_BIGENDIAN 1
# endif
#else
# ifndef WORDS_BIGENDIAN
/* #  undef WORDS_BIGENDIAN */
# endif
#endif

/* Define if xauth is found in your path */
#define XAUTH_PATH "/usr/local/bin/xauth"

/* Enable large inode numbers on Mac OS X 10.5.  */
#ifndef _DARWIN_USE_64_BIT_INODE
# define _DARWIN_USE_64_BIT_INODE 1
#endif

/* Number of bits in a file offset, on hosts where this is settable. */
/* #undef _FILE_OFFSET_BITS */

/* Define for large files, on AIX-style hosts. */
/* #undef _LARGE_FILES */

/* log for bad login attempts */
/* #undef _PATH_BTMP */

/* Full path of your "passwd" program */
#define _PATH_PASSWD_PROG "/usr/bin/passwd"

/* Specify location of ssh.pid */
#define _PATH_SSH_PIDDIR "/var/run"

/* Define if we don't have struct __res_state in resolv.h */
/* #undef __res_state */

/* Define to rpl_calloc if the replacement function should be used. */
/* #undef calloc */

/* Define to `__inline__' or `__inline' if that's what the C compiler
   calls it, or to nothing if 'inline' is not supported under any name.  */
#ifndef __cplusplus
/* #undef inline */
#endif

/* Define to rpl_malloc if the replacement function should be used. */
/* #undef malloc */

/* Define to rpl_realloc if the replacement function should be used. */
/* #undef realloc */

/* type to use in place of socklen_t if not defined */
/* #undef socklen_t */
