/*
 * Copyright 1987, 1988 by the Massachusetts Institute of Technology.
 * For copying and distribution information, please see the file
 * <Copyright.MIT>.
 *
 * Include file for the Kerberos library.
 *
 *	from: krb.h,v 4.26 89/08/08 17:55:25 jtkohl Exp $
 *	$FreeBSD$
 */

/* Only one time, please */
#ifndef	KRB_DEFS
#define KRB_DEFS

/* Need some defs from des.h	 */
#include <stdio.h>
#include <des.h>
#include <netinet/in.h>

/* Text describing error codes */
#define		MAX_KRB_ERRORS	256
extern char *krb_err_txt[MAX_KRB_ERRORS];

/* These are not defined for at least SunOS 3.3 and Ultrix 2.2 */
#if defined(ULTRIX022) || (defined(SunOS) && SunOS < 40)
#define FD_ZERO(p)  ((p)->fds_bits[0] = 0)
#define FD_SET(n, p)   ((p)->fds_bits[0] |= (1 << (n)))
#define FD_ISSET(n, p)   ((p)->fds_bits[0] & (1 << (n)))
#endif /* ULTRIX022 || SunOS */

/* General definitions */
#define		KSUCCESS	0
#define		KFAILURE	255

#ifdef NO_UIDGID_T
typedef unsigned short uid_t;
typedef unsigned short gid_t;
#endif /* NO_UIDGID_T */

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

#ifdef notdef
this is server - only, does not belong here;
#define 	KRBLOG 		"/etc/kerberosIV/kerberos.log"
are these used anyplace '?';
#define		VX_KRB_HSTFILE	"/etc/krbhst"
#define		PC_KRB_HSTFILE	"\\kerberos\\krbhst"
#endif

#define		KRB_CONF	"/etc/kerberosIV/krb.conf"
#define		KRB_RLM_TRANS	"/etc/kerberosIV/krb.realms"
#define		KRB_MASTER	"kerberos"
#define		KRB_HOST	KRB_MASTER
#define		KRB_REALM	"ATHENA.MIT.EDU"

/* The maximum sizes for aname, realm, sname, and instance +1 */
#define 	ANAME_SZ	40
#define		REALM_SZ	40
#define		SNAME_SZ	40
#define		INST_SZ		40
/* include space for '.' and '@' */
#define		MAX_K_NAME_SZ	(ANAME_SZ + INST_SZ + REALM_SZ + 2)
#define		KKEY_SZ		100
#define		VERSION_SZ	1
#define		MSG_TYPE_SZ	1
#define		DATE_SZ		26	/* RTI date output */

#define		MAX_HSTNM	100

#ifndef DEFAULT_TKT_LIFE		/* allow compile-time override */
#define		DEFAULT_TKT_LIFE	96 /* default lifetime for krb_mk_req
					      & co., 8 hrs */
#endif

/* Definition of text structure used to pass text around */
#define		MAX_KTXT_LEN	1250

struct ktext {
    int     length;		/* Length of the text */
    unsigned char dat[MAX_KTXT_LEN];	/* The data itself */
    unsigned long mbz;		/* zero to catch runaway strings */
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

/* Definitions for cl_get_tgt */
#ifdef PC
#define CL_GTGT_INIT_FILE		"\\kerberos\\k_in_tkts"
#else
#define CL_GTGT_INIT_FILE		"/etc/k_in_tkts"
#endif PC

/* Parameters for rd_ap_req */
/* Maximum alloable clock skew in seconds */
#define 	CLOCK_SKEW	5*60
/* Filename for readservkey */
#define		KEYFILE		"/etc/kerberosIV/srvtab"

/* Structure definition for rd_ap_req */

struct auth_dat {
    unsigned char k_flags;	/* Flags from ticket */
    char    pname[ANAME_SZ];	/* Principal's name */
    char    pinst[INST_SZ];	/* His Instance */
    char    prealm[REALM_SZ];	/* His Realm */
    unsigned long checksum;	/* Data checksum (opt) */
    C_Block session;		/* Session Key */
    int     life;		/* Life of ticket */
    unsigned long time_sec;	/* Time ticket issued */
    unsigned long address;	/* Address in ticket */
    KTEXT_ST reply;		/* Auth reply (opt) */
};

typedef struct auth_dat AUTH_DAT;

/* Structure definition for credentials returned by get_cred */

struct credentials {
    char    service[ANAME_SZ];	/* Service name */
    char    instance[INST_SZ];	/* Instance */
    char    realm[REALM_SZ];	/* Auth domain */
    C_Block session;		/* Session key */
    int     lifetime;		/* Lifetime */
    int     kvno;		/* Key version number */
    KTEXT_ST ticket_st;		/* The ticket itself */
    long    issue_date;		/* The issue time */
    char    pname[ANAME_SZ];	/* Principal's name */
    char    pinst[INST_SZ];	/* Principal's instance */
};

typedef struct credentials CREDENTIALS;

/* Structure definition for rd_private_msg and rd_safe_msg */

struct msg_dat {
    unsigned char *app_data;	/* pointer to appl data */
    unsigned long app_length;	/* length of appl data */
    unsigned long hash;		/* hash to lookup replay */
    int     swap;		/* swap bytes? */
    long    time_sec;		/* msg timestamp seconds */
    unsigned char time_5ms;	/* msg timestamp 5ms units */
};

typedef struct msg_dat MSG_DAT;


/* Location of ticket file for save_cred and get_cred */
#ifdef PC
#define TKT_FILE        "\\kerberos\\ticket.ses"
#else
#define TKT_FILE        tkt_string()
#define TKT_ROOT        "/tmp/tkt"
#endif PC

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

/* Error codes returned by ticket file utilities */
#define		NO_TKT_FIL	76	/* No ticket file found */
#define		TKT_FIL_ACC	77	/* Couldn't access tkt file */
#define		TKT_FIL_LCK	78	/* Couldn't lock ticket file */
#define		TKT_FIL_FMT	79	/* Bad ticket file format */
#define		TKT_FIL_INI	80	/* tf_init not called first */

/* Error code returned by kparse_name */
#define		KNAME_FMT	81	/* Bad Kerberos name format */

/* Error codes returned by get_local_addr and bind_local_addr */
#define		GT_LADDR_NOSOCK  82	/* Can't open socket */
#define		GT_LADDR_IFLIST	 83	/*
					 * Can't retrieve local interface 
					 * configuration list
					 */
#define		GT_LADDR_NVI	 84	/* No valid local interface found */
#define		BND_LADDR_BIND	 85	/* Can't bind local address */

/* Error code returned by krb_mk_safe */
#define		SAFE_PRIV_ERROR	-1	/* syscall error */

/*
 * macros for byte swapping; also scratch space
 * u_quad  0-->7, 1-->6, 2-->5, 3-->4, 4-->3, 5-->2, 6-->1, 7-->0
 * u_long  0-->3, 1-->2, 2-->1, 3-->0
 * u_short 0-->1, 1-->0
 */

#define     swap_u_16(x) {\
 unsigned long   _krb_swap_tmp[4];\
 swab(((char *) x) +0, ((char *)  _krb_swap_tmp) +14 ,2); \
 swab(((char *) x) +2, ((char *)  _krb_swap_tmp) +12 ,2); \
 swab(((char *) x) +4, ((char *)  _krb_swap_tmp) +10 ,2); \
 swab(((char *) x) +6, ((char *)  _krb_swap_tmp) +8  ,2); \
 swab(((char *) x) +8, ((char *)  _krb_swap_tmp) +6 ,2); \
 swab(((char *) x) +10,((char *)  _krb_swap_tmp) +4 ,2); \
 swab(((char *) x) +12,((char *)  _krb_swap_tmp) +2 ,2); \
 swab(((char *) x) +14,((char *)  _krb_swap_tmp) +0 ,2); \
 bcopy((char *)_krb_swap_tmp,(char *)x,16);\
                            }

#define     swap_u_12(x) {\
 unsigned long   _krb_swap_tmp[4];\
 swab(( char *) x,     ((char *)  _krb_swap_tmp) +10 ,2); \
 swab(((char *) x) +2, ((char *)  _krb_swap_tmp) +8 ,2); \
 swab(((char *) x) +4, ((char *)  _krb_swap_tmp) +6 ,2); \
 swab(((char *) x) +6, ((char *)  _krb_swap_tmp) +4 ,2); \
 swab(((char *) x) +8, ((char *)  _krb_swap_tmp) +2 ,2); \
 swab(((char *) x) +10,((char *)  _krb_swap_tmp) +0 ,2); \
 bcopy((char *)_krb_swap_tmp,(char *)x,12);\
                            }

#define     swap_C_Block(x) {\
 unsigned long   _krb_swap_tmp[4];\
 swab(( char *) x,    ((char *)  _krb_swap_tmp) +6 ,2); \
 swab(((char *) x) +2,((char *)  _krb_swap_tmp) +4 ,2); \
 swab(((char *) x) +4,((char *)  _krb_swap_tmp) +2 ,2); \
 swab(((char *) x) +6,((char *)  _krb_swap_tmp)    ,2); \
 bcopy((char *)_krb_swap_tmp,(char *)x,8);\
                            }
#define     swap_u_quad(x) {\
 unsigned long   _krb_swap_tmp[4];\
 swab(( char *) &x,    ((char *)  _krb_swap_tmp) +6 ,2); \
 swab(((char *) &x) +2,((char *)  _krb_swap_tmp) +4 ,2); \
 swab(((char *) &x) +4,((char *)  _krb_swap_tmp) +2 ,2); \
 swab(((char *) &x) +6,((char *)  _krb_swap_tmp)    ,2); \
 bcopy((char *)_krb_swap_tmp,(char *)&x,8);\
                            }

#define     swap_u_long(x) {\
 unsigned long   _krb_swap_tmp[4];\
 swab((char *)  &x,    ((char *)  _krb_swap_tmp) +2 ,2); \
 swab(((char *) &x) +2,((char *)  _krb_swap_tmp),2); \
 x = _krb_swap_tmp[0];   \
                           }

#define     swap_u_short(x) {\
 unsigned short	_krb_swap_sh_tmp; \
 swab((char *)  &x,    ( &_krb_swap_sh_tmp) ,2); \
 x = (unsigned short) _krb_swap_sh_tmp; \
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

#ifndef PC
char *tkt_string();
#endif	PC

#ifdef	OLDNAMES
#define krb_mk_req	mk_ap_req
#define krb_rd_req	rd_ap_req
#define krb_kntoln	an_to_ln
#define krb_set_key	set_serv_key
#define krb_get_cred	get_credentials
#define krb_mk_priv	mk_private_msg
#define krb_rd_priv	rd_private_msg
#define krb_mk_safe	mk_safe_msg
#define krb_rd_safe	rd_safe_msg
#define krb_mk_err	mk_appl_err_msg
#define krb_rd_err	rd_appl_err_msg
#define krb_ck_repl	check_replay
#define	krb_get_pw_in_tkt	get_in_tkt
#define krb_get_svc_in_tkt	get_svc_in_tkt
#define krb_get_pw_tkt		get_pw_tkt
#define krb_realmofhost		krb_getrealm
#define krb_get_phost		get_phost
#define krb_get_krbhst		get_krbhst
#define krb_get_lrealm		get_krbrlm
#endif	OLDNAMES

/* Defines for krb_sendauth and krb_recvauth */

#define	KOPT_DONT_MK_REQ 0x00000001 /* don't call krb_mk_req */
#define	KOPT_DO_MUTUAL   0x00000002 /* do mutual auth */

#define	KOPT_DONT_CANON  0x00000004 /*
				     * don't canonicalize inst as
				     * a hostname
				     */

#define	KRB_SENDAUTH_VLEN 8	    /* length for version strings */

#ifdef ATHENA_COMPAT
#define	KOPT_DO_OLDSTYLE 0x00000008 /* use the old-style protocol */
#endif ATHENA_COMPAT

/* libacl */
void acl_canonicalize_principal __P((char *principal, char *buf));
int acl_check __P((char *acl, char *principal));
int acl_exact_match __P((char *acl, char *principal));
int acl_add __P((char *acl, char *principal));
int acl_delete __P((char *acl, char *principal));
int acl_initialize __P((char *acl_file, int mode));

/* libkrb - krb.3 */
int krb_mk_req __P((KTEXT authent, char *service, char *instance, char *realm,
    long checksum);
int krb_rd_req __P((KTEXT authent, char *service, char *instance,
    long from_addr, AUTH_DAT *ad, char *fn));
int krb_kntoln __P((AUTH_DAT *ad, char *lname));
int krb_set_key __P((char *key, int cvt));
int krb_get_cred __P((char *service, char *instance, char *realm,
    CREDENTIALS *c));
long krb_mk_priv __P((u_char *in, u_char *out, u_long in_length,
    des_key_schedule schedule, des_cblock key, struct sockaddr_in *sender,
    struct sockaddr_in *receiver));
long krb_rd_priv __P((u_char *in, u_long in_length, Key_schedule schedule,
    des_cblock key, struct sockaddr_in *sender, struct sockaddr_in *receiver,
    MSG_DAT *msg_data));
long krb_mk_safe __P((u_char *in, u_char *out, u_long in_length,
    des_cblock *key, struct sockaddr_in *sender, struct sockaddr_in *receiver));
long krb_rd_safe __P((u_char *in, u_long length, des_cblock *key,
    struct sockaddr_in *sender, struct sockaddr_in *receiver,
    MSG_DAT *msg_data));
long krb_mk_err __P((u_char *out, long code, char *string));
int krb_rd_err __P((u_char *in, u_long in_length, long *code, MSG_DAT *m_data));

/* libkrb - krb_sendauth.3 */
int krb_sendauth __P((long options, int fd, KTEXT ticket, char *service,
    char *inst, char *realm, u_long checksum, MSG_DAT *msg_data,
    CREDENTIALS *cred, Key_schedule schedule, struct sockaddr_in *laddr,
    struct sockaddr_in *faddr, char *version));
int krb_recvauth __P((long options, int fd, KTEXT ticket, char *service,
    char *instance, struct sockaddr_in *faddr, struct sockaddr_in *laddr,
    AUTH_DAT *kdata, char *filename, Key_schedule schedule, char *version));
int krb_net_write __P((int fd, char *buf, int len));
int krb_net_read __P((int fd, char *buf, int len));

/* libkrb - krb_realmofhost.3 */
char *krb_realmofhost __P((char *host));
char *krb_get_phost __P((char *alias));
int krb_get_krbhst __P((char *h, char *r, int n));
int krb_get_admhst __P((char *h, char *r, int n));
int krb_get_lrealm __P((char *r, int n));

/* libkrb - krb_set_tkt_string.3 */
void krb_set_tkt_string(char *val);

/* libkrb - kuserok.3 */
int kuserok __P((AUTH_DAT *authdata, char *localuser));

/* libkrb - tf_util.3 */
int tf_init __P((char *tf_name, int rw));
int tf_get_pname __P((char *p));
int tf_get_pinst __P((char *inst));
int tf_get_cred __P((CREDENTIALS *c));
void tf_close __P((void));

/* Internal routines */
int des_set_key_krb __P((des_cblock *inkey, des_key_schedule insched));
void des_clear_key_krb __P((void));
int des_read __P((int fd, char *buf, int len));
int des_write __P((int fd, char *buf, int len));
int krb_get_tf_realm __P((char *ticket_file, char *realm));
int krb_get_in_tkt __P((char *user, char *instance, char *realm, char *service,
    char *sinstance, int life, int (*key_proc)(), int (*decrypt_proc)(),
    char *arg));
int krb_get_pw_in_tkt __P((char *user, char *instance, char *realm,
    char *service, char *sinstance, int life, char *password));
int krb_get_svc_in_tkt __P((char *user, char *instance, char *realm,
    char *service, char *sinstance, int life, char *srvtab));
int krb_get_tf_fullname __P((char *ticket_file, char *name, char *instance,
    char *realm));
int save_credentials __P((char *service, char *instance, char *realm,
    des_cblock session, int lifetime, int kvno, KTEXT ticket, long issue_date));
int read_service_key __P((char *service, char *instance, char *realm, int kvno,
    char *file, char *key));
int get_ad_tkt __P((char *service, char *sinstance, char *realm, int lifetime));
int send_to_kdc __P((KTEXT pkt, KTEXT rpkt, char *realm));
int krb_bind_local_addr __P((int s));
int krb_get_local_addr __P((struct sockaddr_in *returned_addr));
int krb_create_ticket __P((KTEXT tkt, unsigned char flags, char *pname,
    char *pinstance, char *prealm, long paddress, char *session, short life,
    long time_sec, char *sname, char *sinstance, C_Block key));
int decomp_ticket __P((KTEXT tkt, unsigned char *flags, char *pname,
    char *pinstance, char *prealm, unsigned long *paddress, des_cblock session,
    int *life, unsigned long *time_sec, char *sname, char *sinstance,
    des_cblock key, des_key_schedule key_s));
int create_ciph __P((KTEXT c, C_Block session, char *service, char *instance,
    char *realm, unsigned long life, int kvno, KTEXT tkt,
    unsigned long kdc_time, C_Block key));
int kname_parse __P((char *np, char *ip, char *rp, char *fullname));
int tf_save_cred __P((char *service, char *instance, char *realm,
    des_cblock session, int lifetime, int kvno, KTEXT ticket, long issue_date));
int getst(int fd, char *s, int n));
int pkt_clen __P((KTEXT pkt));
int in_tkt __P((char *pname, char *pinst));
int dest_tkt __P((void));
char *month_sname __P((int n));
void log __P(()); /* Actually VARARGS - markm */
void kset_logfile __P((char *filename));
void set_logfile __P((char *filename));
int k_isinst __P((char *s));
int k_isrealm __P((char *s));
int k_isname __P((char *s));
int k_gethostname __P((char *name, int namelen));
int kerb_init __P((void));
void kerb_fini __P((void));
int kerb_db_set_name __P((char *name));
int kerb_db_set_lockmode __P((int mode));
int kerb_db_create __P((char *db_name));
int kerb_db_iterate __P((int (*func)(), char *arg));
int kerb_db_rename __P((char *from, char *to));
long kerb_get_db_age __P((void));
char * stime __P((long *t));

long kdb_get_master_key __P((int prompt, C_Block master_key,
    Key_schedule master_key_sched));
long kdb_verify_master_key __P((C_Block master_key,
    Key_schedule master_key_sched, FILE *out));
void kdb_encrypt_key  __P((C_Block in, C_Block out, C_Block master_key,
    Key_schedule master_key_sched, int e_d_flag));

extern int krb_ap_req_debug;
extern int krb_debug;

#endif	KRB_DEFS
