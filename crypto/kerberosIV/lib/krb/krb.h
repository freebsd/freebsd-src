/*
 * $Id: krb.h,v 1.76 1997/05/26 17:47:31 bg Exp $
 *
 * Copyright 1987, 1988 by the Massachusetts Institute of Technology. 
 *
 * For copying and distribution information, please see the file
 * <mit-copyright.h>. 
 *
 * Include file for the Kerberos library. 
 */

/* Only one time, please */
#ifndef	KRB_DEFS
#define KRB_DEFS

#include <ktypes.h>
#include <sys/cdefs.h>
#include <stdarg.h>
#include <time.h>

__BEGIN_DECLS

#ifndef __P
#define __P(x) x
#endif

/* Need some defs from des.h	 */
#if !defined(NOPROTO) && !defined(__STDC__)
#define NOPROTO
#endif
#include <des.h>

/* Don't use these guys, they are only for compatibility with CNS. */
#ifndef KRB_INT32
#define KRB_INT32 int32_t
#endif
#ifndef KRB_UINT32
#define KRB_UINT32 u_int32_t
#endif

/* Global library variables. */
extern int krb_ignore_ip_address; /* To turn off IP address comparison */
extern int krb_no_long_lifetimes; /* To disable AFS compatible lifetimes */
extern int krbONE;
#define         HOST_BYTE_ORDER (* (char *) &krbONE)

/* Text describing error codes */
#define		MAX_KRB_ERRORS	256
extern const char *krb_err_txt[MAX_KRB_ERRORS];

/* Use this function rather than indexing in krb_err_txt */
const char *krb_get_err_text __P((int code));


/* General definitions */
#define		KSUCCESS	0
#define		KFAILURE	255

/*
 * Kerberos specific definitions 
 *
 * KRBLOG is the log file for the kerberos master server. KRB_CONF is
 * the configuration file where different host machines running master
 * and slave servers can be found. KRB_MASTER is the name of the
 * machine with the master database.  The admin_server runs on this
 * machine, and all changes to the db (as opposed to read-only
 * requests, which can go to slaves) must go to it. KRB_HOST is the
 * default machine * when looking for a kerberos slave server.  Other
 * possibilities are * in the KRB_CONF file. KRB_REALM is the name of
 * the realm. 
 */

/* /etc/kerberosIV is only for backwards compatibility, don't use it! */
#ifndef KRB_CONF
#define KRB_CONF	"/etc/krb.conf"
#endif
#ifndef KRB_RLM_TRANS
#define KRB_RLM_TRANS   "/etc/krb.realms"
#endif
#ifndef KRB_CNF_FILES
#define KRB_CNF_FILES	{ KRB_CONF,   "/etc/kerberosIV/krb.conf", 0}
#endif
#ifndef KRB_RLM_FILES
#define KRB_RLM_FILES	{ KRB_RLM_TRANS, "/etc/kerberosIV/krb.realms", 0}
#endif
#ifndef KRB_EQUIV
#define KRB_EQUIV	"/etc/krb.equiv"
#endif
#define KRB_MASTER	"kerberos"
#ifndef KRB_REALM
#define KRB_REALM	(krb_get_default_realm())
#endif

/* The maximum sizes for aname, realm, sname, and instance +1 */
#define 	ANAME_SZ	40
#define		REALM_SZ	40
#define		SNAME_SZ	40
#define		INST_SZ		40
/* Leave space for quoting */
#define		MAX_K_NAME_SZ	(2*ANAME_SZ + 2*INST_SZ + 2*REALM_SZ - 3)
#define		KKEY_SZ		100
#define		VERSION_SZ	1
#define		MSG_TYPE_SZ	1
#define		DATE_SZ		26	/* RTI date output */

#define MAX_HSTNM 100 /* for compatibility */

typedef struct krb_principal{
    char name[ANAME_SZ];
    char instance[INST_SZ];
    char realm[REALM_SZ];
}krb_principal;

#ifndef DEFAULT_TKT_LIFE	/* allow compile-time override */
/* default lifetime for krb_mk_req & co., 10 hrs */
#define	DEFAULT_TKT_LIFE 120
#endif

#define		KRB_TICKET_GRANTING_TICKET	"krbtgt"

/* Definition of text structure used to pass text around */
#define		MAX_KTXT_LEN	1250

struct ktext {
    unsigned int length;		/* Length of the text */
    unsigned char dat[MAX_KTXT_LEN];	/* The data itself */
    u_int32_t mbz;		/* zero to catch runaway strings */
};

typedef struct ktext *KTEXT;
typedef struct ktext KTEXT_ST;


/* Definitions for send_to_kdc */
#define	CLIENT_KRB_TIMEOUT	4	/* time between retries */
#define CLIENT_KRB_RETRY	5	/* retry this many times */
#define	CLIENT_KRB_BUFLEN	512	/* max unfragmented packet */

/* Definitions for ticket file utilities */
#define	R_TKT_FIL	0
#define	W_TKT_FIL	1

/* Parameters for rd_ap_req */
/* Maximum alloable clock skew in seconds */
#define 	CLOCK_SKEW	5*60
/* Filename for readservkey */
#ifndef		KEYFILE
#define		KEYFILE		"/etc/srvtab"
#endif

/* Structure definition for rd_ap_req */

struct auth_dat {
    unsigned char k_flags;	/* Flags from ticket */
    char    pname[ANAME_SZ];	/* Principal's name */
    char    pinst[INST_SZ];	/* His Instance */
    char    prealm[REALM_SZ];	/* His Realm */
    u_int32_t checksum;		/* Data checksum (opt) */
    des_cblock session;		/* Session Key */
    int     life;		/* Life of ticket */
    u_int32_t time_sec;		/* Time ticket issued */
    u_int32_t address;		/* Address in ticket */
    KTEXT_ST reply;		/* Auth reply (opt) */
};

typedef struct auth_dat AUTH_DAT;

/* Structure definition for credentials returned by get_cred */

struct credentials {
    char    service[ANAME_SZ];	/* Service name */
    char    instance[INST_SZ];	/* Instance */
    char    realm[REALM_SZ];	/* Auth domain */
    des_cblock session;		/* Session key */
    int     lifetime;		/* Lifetime */
    int     kvno;		/* Key version number */
    KTEXT_ST ticket_st;		/* The ticket itself */
    int32_t    issue_date;	/* The issue time */
    char    pname[ANAME_SZ];	/* Principal's name */
    char    pinst[INST_SZ];	/* Principal's instance */
};

typedef struct credentials CREDENTIALS;

/* Structure definition for rd_private_msg and rd_safe_msg */

struct msg_dat {
    unsigned char *app_data;	/* pointer to appl data */
    u_int32_t app_length;	/* length of appl data */
    u_int32_t hash;		/* hash to lookup replay */
    int     swap;		/* swap bytes? */
    int32_t    time_sec;		/* msg timestamp seconds */
    unsigned char time_5ms;	/* msg timestamp 5ms units */
};

typedef struct msg_dat MSG_DAT;

struct krb_host {
    char *realm;
    char *host;
    int proto;
    int port;
    int admin;
};

struct krb_host *krb_get_host __P((int, char*, int));


/* Location of ticket file for save_cred and get_cred */
#define TKT_FILE        tkt_string()
#define TKT_ROOT        "/tmp/tkt"

/* Error codes returned from the KDC */
#define		KDC_OK		0	/* Request OK */
#define		KDC_NAME_EXP	1	/* Principal expired */
#define		KDC_SERVICE_EXP	2	/* Service expired */
#define		KDC_AUTH_EXP	3	/* Auth expired */
#define		KDC_PKT_VER	4	/* Protocol version unknown */
#define		KDC_P_MKEY_VER	5	/* Wrong master key version */
#define		KDC_S_MKEY_VER 	6	/* Wrong master key version */
#define		KDC_BYTE_ORDER	7	/* Byte order unknown */
#define		KDC_PR_UNKNOWN	8	/* Principal unknown */
#define		KDC_PR_N_UNIQUE 9	/* Principal not unique */
#define		KDC_NULL_KEY   10	/* Principal has null key */
#define		KDC_GEN_ERR    20	/* Generic error from KDC */


/* Values returned by get_credentials */
#define		GC_OK		0	/* Retrieve OK */
#define		RET_OK		0	/* Retrieve OK */
#define		GC_TKFIL       21	/* Can't read ticket file */
#define		RET_TKFIL      21	/* Can't read ticket file */
#define		GC_NOTKT       22	/* Can't find ticket or TGT */
#define		RET_NOTKT      22	/* Can't find ticket or TGT */


/* Values returned by mk_ap_req	 */
#define		MK_AP_OK	0	/* Success */
#define		MK_AP_TGTEXP   26	/* TGT Expired */

/* Values returned by rd_ap_req */
#define		RD_AP_OK	0	/* Request authentic */
#define		RD_AP_UNDEC    31	/* Can't decode authenticator */
#define		RD_AP_EXP      32	/* Ticket expired */
#define		RD_AP_NYV      33	/* Ticket not yet valid */
#define		RD_AP_REPEAT   34	/* Repeated request */
#define		RD_AP_NOT_US   35	/* The ticket isn't for us */
#define		RD_AP_INCON    36	/* Request is inconsistent */
#define		RD_AP_TIME     37	/* delta_t too big */
#define		RD_AP_BADD     38	/* Incorrect net address */
#define		RD_AP_VERSION  39	/* protocol version mismatch */
#define		RD_AP_MSG_TYPE 40	/* invalid msg type */
#define		RD_AP_MODIFIED 41	/* message stream modified */
#define		RD_AP_ORDER    42	/* message out of order */
#define		RD_AP_UNAUTHOR 43	/* unauthorized request */

/* Values returned by get_pw_tkt */
#define		GT_PW_OK	0	/* Got password changing tkt */
#define		GT_PW_NULL     51	/* Current PW is null */
#define		GT_PW_BADPW    52	/* Incorrect current password */
#define		GT_PW_PROT     53	/* Protocol Error */
#define		GT_PW_KDCERR   54	/* Error returned by KDC */
#define		GT_PW_NULLTKT  55	/* Null tkt returned by KDC */


/* Values returned by send_to_kdc */
#define		SKDC_OK		0	/* Response received */
#define		SKDC_RETRY     56	/* Retry count exceeded */
#define		SKDC_CANT      57	/* Can't send request */

/*
 * Values returned by get_intkt
 * (can also return SKDC_* and KDC errors)
 */

#define		INTK_OK		0	/* Ticket obtained */
#define		INTK_W_NOTALL  61	/* Not ALL tickets returned */
#define		INTK_BADPW     62	/* Incorrect password */
#define		INTK_PROT      63	/* Protocol Error */
#define		INTK_ERR       70	/* Other error */

/* Values returned by get_adtkt */
#define         AD_OK           0	/* Ticket Obtained */
#define         AD_NOTGT       71	/* Don't have tgt */
#define         AD_INTR_RLM_NOTGT 72	/* Can't get inter-realm tgt */

/* Error codes returned by ticket file utilities */
#define		NO_TKT_FIL	76	/* No ticket file found */
#define		TKT_FIL_ACC	77	/* Couldn't access tkt file */
#define		TKT_FIL_LCK	78	/* Couldn't lock ticket file */
#define		TKT_FIL_FMT	79	/* Bad ticket file format */
#define		TKT_FIL_INI	80	/* tf_init not called first */

/* Error code returned by kparse_name */
#define		KNAME_FMT	81	/* Bad Kerberos name format */

/* Error code returned by krb_mk_safe */
#define		SAFE_PRIV_ERROR	-1	/* syscall error */

/*
 * macros for byte swapping; also scratch space
 * u_quad  0-->7, 1-->6, 2-->5, 3-->4, 4-->3, 5-->2, 6-->1, 7-->0
 * u_int32_t  0-->3, 1-->2, 2-->1, 3-->0
 * u_int16_t 0-->1, 1-->0
 */

#define     swap_u_16(x) {\
 u_int32_t   _krb_swap_tmp[4];\
 swab(((char *) x) +0, ((char *)  _krb_swap_tmp) +14 ,2); \
 swab(((char *) x) +2, ((char *)  _krb_swap_tmp) +12 ,2); \
 swab(((char *) x) +4, ((char *)  _krb_swap_tmp) +10 ,2); \
 swab(((char *) x) +6, ((char *)  _krb_swap_tmp) +8  ,2); \
 swab(((char *) x) +8, ((char *)  _krb_swap_tmp) +6 ,2); \
 swab(((char *) x) +10,((char *)  _krb_swap_tmp) +4 ,2); \
 swab(((char *) x) +12,((char *)  _krb_swap_tmp) +2 ,2); \
 swab(((char *) x) +14,((char *)  _krb_swap_tmp) +0 ,2); \
 memcpy(x, _krb_swap_tmp, 16);\
                            }

#define     swap_u_12(x) {\
 u_int32_t   _krb_swap_tmp[4];\
 swab(( char *) x,     ((char *)  _krb_swap_tmp) +10 ,2); \
 swab(((char *) x) +2, ((char *)  _krb_swap_tmp) +8 ,2); \
 swab(((char *) x) +4, ((char *)  _krb_swap_tmp) +6 ,2); \
 swab(((char *) x) +6, ((char *)  _krb_swap_tmp) +4 ,2); \
 swab(((char *) x) +8, ((char *)  _krb_swap_tmp) +2 ,2); \
 swab(((char *) x) +10,((char *)  _krb_swap_tmp) +0 ,2); \
 memcpy(x, _krb_swap_tmp, 12);\
                            }

#define     swap_C_Block(x) {\
 u_int32_t   _krb_swap_tmp[4];\
 swab(( char *) x,    ((char *)  _krb_swap_tmp) +6 ,2); \
 swab(((char *) x) +2,((char *)  _krb_swap_tmp) +4 ,2); \
 swab(((char *) x) +4,((char *)  _krb_swap_tmp) +2 ,2); \
 swab(((char *) x) +6,((char *)  _krb_swap_tmp)    ,2); \
 memcpy(x, _krb_swap_tmp, 8);\
                            }
#define     swap_u_quad(x) {\
 u_int32_t   _krb_swap_tmp[4];\
 swab(( char *) &x,    ((char *)  _krb_swap_tmp) +6 ,2); \
 swab(((char *) &x) +2,((char *)  _krb_swap_tmp) +4 ,2); \
 swab(((char *) &x) +4,((char *)  _krb_swap_tmp) +2 ,2); \
 swab(((char *) &x) +6,((char *)  _krb_swap_tmp)    ,2); \
 memcpy(x, _krb_swap_tmp, 8);\
                            }

#define     swap_u_long(x) {\
 u_int32_t   _krb_swap_tmp[4];\
 swab((char *)  &x,    ((char *)  _krb_swap_tmp) +2 ,2); \
 swab(((char *) &x) +2,((char *)  _krb_swap_tmp),2); \
 x = _krb_swap_tmp[0];   \
                           }

#define     swap_u_short(x) {\
 u_int16_t	_krb_swap_sh_tmp; \
 swab((char *)  &x,    ( &_krb_swap_sh_tmp) ,2); \
 x = (u_int16_t) _krb_swap_sh_tmp; \
                            }
/* Kerberos ticket flag field bit definitions */
#define K_FLAG_ORDER    0       /* bit 0 --> lsb */
#define K_FLAG_1                /* reserved */
#define K_FLAG_2                /* reserved */
#define K_FLAG_3                /* reserved */
#define K_FLAG_4                /* reserved */
#define K_FLAG_5                /* reserved */
#define K_FLAG_6                /* reserved */
#define K_FLAG_7                /* reserved, bit 7 --> msb */

/* Defines for krb_sendauth and krb_recvauth */

#define	KOPT_DONT_MK_REQ 0x00000001 /* don't call krb_mk_req */
#define	KOPT_DO_MUTUAL   0x00000002 /* do mutual auth */

#define	KOPT_DONT_CANON  0x00000004 /*
				     * don't canonicalize inst as
				     * a hostname
				     */

#define	KRB_SENDAUTH_VLEN 8	    /* length for version strings */


/* File locking */
#define   K_LOCK_SH   1		/* Shared lock */
#define   K_LOCK_EX   2		/* Exclusive lock */
#define   K_LOCK_NB   4		/* Don't block when locking */
#define   K_LOCK_UN   8		/* Unlock */
int k_flock __P((int fd, int operation));
struct tm *k_localtime __P((u_int32_t *));
int k_getsockinst __P((int fd, char *inst, size_t));
int k_getportbyname __P((const char *service, const char *proto, int default_port));

extern char *krb4_version;

struct in_addr;

int k_get_all_addrs __P((struct in_addr **l));

/* Host address comparison */
int krb_equiv __P((u_int32_t, u_int32_t));

/* Password conversion */
void mit_string_to_key __P((char *str, char *cell, des_cblock *key));
void afs_string_to_key __P((char *str, char *cell, des_cblock *key));

/* Lifetime conversion */
u_int32_t krb_life_to_time __P((u_int32_t start, int life));
int krb_time_to_life __P((u_int32_t start, u_int32_t end));
char *krb_life_to_atime __P((int life));
int krb_atime_to_life __P((char *atime));

/* Ticket manipulation */
int tf_get_cred __P((CREDENTIALS *));
int tf_get_pinst __P((char *));
int tf_get_pname __P((char *));
int tf_put_pinst __P((char *));
int tf_put_pname __P((char *));
int tf_init __P((char *, int));
int tf_create __P((char *));
int tf_save_cred __P((char *, char *, char *, unsigned char *, int , int , KTEXT ticket, u_int32_t));
void tf_close __P((void));
int tf_setup __P((CREDENTIALS *cred, char *pname, char *pinst));

/* Private communication */

struct sockaddr_in;

int32_t krb_mk_priv __P((void *, void *, u_int32_t, struct des_ks_struct *, des_cblock *, struct sockaddr_in *, struct sockaddr_in *));
int32_t krb_rd_priv __P((void *, u_int32_t, struct des_ks_struct *, des_cblock *, struct sockaddr_in *, struct sockaddr_in *, MSG_DAT *));

/* Misc */
KTEXT create_auth_reply __P((char *, char *, char *, int32_t, int, u_int32_t, int, KTEXT));

char *krb_get_phost __P((const char *));
char *krb_realmofhost __P((const char *));
char *tkt_string __P((void));

int create_ciph __P((KTEXT, unsigned char *, char *, char *, char *, u_int32_t, int, KTEXT, u_int32_t, des_cblock *));
int decomp_ticket __P((KTEXT, unsigned char *, char *, char *, char *, u_int32_t *, unsigned char *, int *, u_int32_t *, char *, char *, des_cblock *, struct des_ks_struct *));
int dest_tkt __P((void));
int get_ad_tkt __P((char *, char *, char *, int));
int get_pw_tkt __P((char *, char *, char *, char *));
int get_request __P((KTEXT, int, char **, char **));
int in_tkt __P((char *, char *));
int k_gethostname __P((char *, int ));
int k_isinst __P((char *));
int k_isname __P((char *));
int k_isrealm __P((char *));
int kname_parse __P((char *, char *, char *, char *));
int krb_parse_name __P((const char*, krb_principal*));
char *krb_unparse_name  __P((krb_principal*));
char *krb_unparse_name_r  __P((krb_principal*, char*));
char *krb_unparse_name_long  __P((char*, char*, char*));
char *krb_unparse_name_long_r __P((char *name, char *instance, char *realm, char *fullname));
int krb_create_ticket __P((KTEXT, unsigned char, char *, char *, char *, int32_t, void *, int16_t, int32_t, char *, char *, des_cblock *));
int krb_get_admhst __P((char *, char *, int));
int krb_get_cred __P((char *, char *, char *, CREDENTIALS *));

typedef int (*key_proc_t) __P((char*, char*, char*, void*, des_cblock*));

typedef int (*decrypt_proc_t) __P((char*, char*, char*, void*, 
			      key_proc_t, KTEXT*));

int krb_get_in_tkt __P((char*, char*, char*, char*, char*, int, key_proc_t, 
			decrypt_proc_t, void*));

int srvtab_to_key	__P((char *, char *, char *, void *, des_cblock *));
int passwd_to_key	__P((char *, char *, char *, void *, des_cblock *));
int passwd_to_afskey	__P((char *, char *, char *, void *, des_cblock *));

int krb_get_krbhst __P((char *, char *, int));
int krb_get_lrealm __P((char *, int));
char *krb_get_default_realm __P((void));
int krb_get_pw_in_tkt __P((char *, char *, char *, char *, char *, int, char *));
int krb_get_svc_in_tkt __P((char *, char *, char *, char *, char *, int, char *));
int krb_get_tf_fullname __P((char *, char *, char *, char *));
int krb_get_tf_realm __P((char *, char *));
int krb_kntoln __P((AUTH_DAT *, char *));
int krb_mk_req __P((KTEXT , char *, char *, char *, int32_t));
int krb_net_read __P((int , void *, size_t));
int krb_net_write __P((int , const void *, size_t));
int krb_rd_err __P((u_char *, u_int32_t, int32_t *, MSG_DAT *));
int krb_rd_req __P((KTEXT , char *, char *, int32_t, AUTH_DAT *, char *));
int krb_recvauth __P((int32_t, int, KTEXT, char *, char *, struct sockaddr_in *, struct sockaddr_in *, AUTH_DAT *, char *, struct des_ks_struct *, char *));
int krb_sendauth __P((int32_t, int, KTEXT, char *,char *, char *, u_int32_t, MSG_DAT *, CREDENTIALS *, struct des_ks_struct *, struct sockaddr_in *, struct sockaddr_in *, char *));
int krb_mk_auth __P((int32_t, KTEXT, char *, char *, char *, u_int32_t, char *, KTEXT));
int krb_check_auth __P((KTEXT, u_int32_t, MSG_DAT *, des_cblock *, struct des_ks_struct *, struct sockaddr_in *, struct sockaddr_in *));
int krb_set_key __P((void *, int));
int krb_set_lifetime __P((int));
int krb_kuserok __P((char *name, char *inst, char *realm, char *luser));
int kuserok __P((AUTH_DAT *, char *));
int read_service_key __P((char *, char *, char *, int , char *, char *));
int save_credentials __P((char *, char *, char *, unsigned char *, int , int , KTEXT , int32_t));
int send_to_kdc __P((KTEXT , KTEXT , char *));

int32_t krb_mk_err __P((u_char *, int32_t, char *));
int32_t krb_mk_safe __P((void *, void *, u_int32_t, des_cblock *, struct sockaddr_in *, struct sockaddr_in *));
int32_t krb_rd_safe __P((void *, u_int32_t, des_cblock *, struct sockaddr_in *, struct sockaddr_in *, MSG_DAT *));

void ad_print __P((AUTH_DAT *));
void cr_err_reply __P((KTEXT, char *, char *, char *, u_int32_t, u_int32_t, char *));
void extract_ticket __P((KTEXT, int, char *, int *, int *, char *, KTEXT));
void krb_set_tkt_string __P((char *));

int krb_get_default_principal __P((char *, char *, char *));
int krb_realm_parse __P((char *, int));
int krb_verify_user __P((char*, char*, char*, char*, int, char *));

/* logging.c */

typedef int (*krb_log_func_t)(FILE *, const char *, va_list);

typedef krb_log_func_t krb_warnfn_t;

struct krb_log_facility;

int krb_vlogger __P((struct krb_log_facility*, const char *, va_list))
#ifdef __GNUC__
__attribute__ ((format (printf, 2, 0)))
#endif
;
int krb_logger __P((struct krb_log_facility*, const char *, ...))
#ifdef __GNUC__
__attribute__ ((format (printf, 2, 3)))
#endif
;
int krb_openlog __P((struct krb_log_facility*, char*, FILE*, krb_log_func_t));

void krb_set_warnfn  __P((krb_warnfn_t));
krb_warnfn_t krb_get_warnfn  __P((void));
void krb_warning  __P((const char*, ...))
#ifdef __GNUC__
__attribute__ ((format (printf, 1, 2)))
#endif
;

void kset_logfile __P((char*));
void krb_log __P((const char*, ...))
#ifdef __GNUC__
__attribute__ ((format (printf, 1, 2)))
#endif
;
char *klog __P((int, const char*, ...))
#ifdef __GNUC__
__attribute__ ((format (printf, 2, 3)))
#endif
;

int getst __P((int, char *, int));
const char *month_sname __P((int));
const char *krb_stime __P((time_t *));
int krb_check_tm __P((struct tm));

int krb_get_int __P((void *from, u_int32_t *to, int size, int lsb));
int krb_put_int __P((u_int32_t from, void *to, int size));
int krb_get_address __P((void *from, u_int32_t *to));
int krb_put_address __P((u_int32_t addr, void *to));
int krb_put_string __P((char *from, void *to));
int krb_get_string __P((void *from, char *to));
int krb_get_nir __P((void *from, char *name, char *instance, char *realm));
int krb_put_nir __P((char *name, char *instance, char *realm, void *to));

__END_DECLS

#endif /* KRB_DEFS */
