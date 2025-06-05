/*
 * leasherr.h
 * This file is the #include file for leasherr.et.
 * Please do not edit it as it is automatically generated.
 */

#define LSH_ONLYONEME                            (40591872L)
#define LSH_INVPRINCIPAL                         (40591873L)
#define LSH_FAILEDREALM                          (40591874L)
#define LSH_INVINSTANCE                          (40591875L)
#define LSH_INVREALM                             (40591876L)
#define LSH_EOF                                  (40591877L)
#define LSH_EXPIRESOON                           (40591878L)
#define LSH_NOMATCH                              (40591879L)
#define LSH_BADCHARS                             (40591880L)
#define LSH_FATAL_ERROR                          (40591881L)
#define LSH_BADWINSOCK                           (40591882L)
#define LSH_BADTIMESERV                          (40591883L)
#define LSH_NOSOCKET                             (40591884L)
#define LSH_NOCONNECT                            (40591885L)
#define LSH_TIMEFAILED                           (40591886L)
#define LSH_GETTIMEOFDAY                         (40591887L)
#define LSH_SETTIMEOFDAY                         (40591888L)
#define LSH_RECVTIME                             (40591889L)
#define LSH_RECVBYTES                            (40591890L)
#define LSH_ALREADY_SETTIME                      (40591891L)
extern void initialize_lsh_error_table(struct et_list **);
#define ERROR_TABLE_BASE_lsh (40591872L)

/* for compatibility with older versions... */
#define init_lsh_err_tbl() initialize_lsh_error_table(&_et_list)
#define lsh_err_base ERROR_TABLE_BASE_lsh
