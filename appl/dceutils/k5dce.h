/* dummy K5 routines which are needed to get this to
 * compile without having access ti the DCE versions
 * of the header files.
 * Thiis is very crude, and OSF needs to expose the K5
 * API.
 */

#ifdef sun
/* Transarc obfascates these routines */
#ifdef DCE_1_1

#define krb5_init_ets                   _dce_PkjKqOaklP
#define krb5_copy_creds                 _dce_LuFxPiITzD
#define krb5_unparse_name               _dce_LWHtAuNgRV
#define krb5_get_default_realm          _dce_vDruhprWGh
#define krb5_build_principal            _dce_qwAalSzTtF
#define krb5_build_principal_ext        _dce_vhafIQlejW
#define krb5_build_principal_va         _dce_alsqToMmuJ
#define krb5_cc_default                 _dce_KZRshhTXhE
#define krb5_cc_default_name            _dce_bzJVAjHXVQ
#define sec_login_krb5_add_cred			_dce_ePDtOJTZvU

#else /* DCE 1.0.3a */

#define krb5_init_ets                   _dce_BmLRpOVsBo
#define krb5_copy_creds                 _dce_VGwSEBNwaf
#define krb5_unparse_name               _dce_PgAOkJoMXA
#define krb5_get_default_realm          _dce_plVOzStKyK
#define krb5_build_principal            _dce_uAKSsluIFy
#define krb5_build_principal_ext        _dce_tRMpPiRada
#define krb5_build_principal_va         _dce_SxnLejZemH
#define krb5_cc_default                 _dce_SeKosWFnsv
#define krb5_cc_default_name            _dce_qJeaphJWVc
#define sec_login_krb5_add_cred         _dce_uHwRasumsN

#endif
#endif

/* Define the bare minimum k5 structures which are needed
 * by this program. Since the krb5 includes are not supplied
 * with DCE, these were based on the MIT Kerberos 5 beta 3
 * which should match the DCE as of 1.0.3 at least.
 * The tricky one is the krb5_creds, since one is allocated
 * by this program, and it needs access to the client principal
 * in it.
 * Note that there are no function prototypes, so there is no
 * compile time checking.
 * DEE 07/11/95
 */
#define     NPROTOTYPE(x) ()
typedef int krb5_int32;  /* assuming all DCE systems are 32 bit */
typedef short krb5short; /* assuming short is 16 bit */
typedef krb5_int32      krb5_error_code;
typedef unsigned char   krb5_octet;
typedef krb5_octet      krb5_boolean;
typedef krb5short       krb5_keytype; /* in k5.2 it's a short */
typedef krb5_int32      krb5_flags;
typedef krb5_int32  krb5_timestamp; /* is a time_t in krb5.h */

typedef char * krb5_pointer;  /* pointer to unexposed data */

typedef struct _krb5_ccache {
    struct _krb5_cc_ops *ops;
    krb5_pointer data;
} *krb5_ccache;

typedef struct _krb5_cc_ops {
    char *prefix;
    char *(*get_name) NPROTOTYPE((krb5_ccache));
    krb5_error_code (*resolve) NPROTOTYPE((krb5_ccache *, char *));
    krb5_error_code (*gen_new) NPROTOTYPE((krb5_ccache *));
    krb5_error_code (*init) NPROTOTYPE((krb5_ccache, krb5_principal));
    krb5_error_code (*destroy) NPROTOTYPE((krb5_ccache));
    krb5_error_code (*close) NPROTOTYPE((krb5_ccache));
    krb5_error_code (*store) NPROTOTYPE((krb5_ccache, krb5_creds *));
    krb5_error_code (*retrieve) NPROTOTYPE((krb5_ccache, krb5_flags,
                   krb5_creds *, krb5_creds *));
    krb5_error_code (*get_princ) NPROTOTYPE((krb5_ccache,
                        krb5_principal *));
    krb5_error_code (*get_first) NPROTOTYPE((krb5_ccache,
                        krb5_cc_cursor *));
    krb5_error_code (*get_next) NPROTOTYPE((krb5_ccache, krb5_cc_cursor *,
                   krb5_creds *));
    krb5_error_code (*end_get) NPROTOTYPE((krb5_ccache, krb5_cc_cursor *));
    krb5_error_code (*remove_cred) NPROTOTYPE((krb5_ccache, krb5_flags,
                      krb5_creds *));
    krb5_error_code (*set_flags) NPROTOTYPE((krb5_ccache, krb5_flags));
} krb5_cc_ops;

typedef struct _krb5_keyblock {
	krb5_keytype keytype;
	int length;
	krb5_octet *contents;
} krb5_keyblock;

typedef struct _krb5_ticket_times {
	krb5_timestamp authtime;
	krb5_timestamp starttime;
	krb5_timestamp endtime;
	krb5_timestamp renew_till;
} krb5_ticket_times;

typedef krb5_pointer krb5_cc_cursor;

typedef struct _krb5_data {
   int length;
   char *data;
} krb5_data;

typedef struct _krb5_authdata {
   int ad_type;
   int length;
   krb5_octet *contents;
} krb5_authdata;

typedef struct _krb5_creds {
    krb5_pointer client;
    krb5_pointer server;
    krb5_keyblock keyblock;
    krb5_ticket_times times;
    krb5_boolean is_skey;
    krb5_flags ticket_flags;
    krb5_pointer **addresses;
    krb5_data ticket;
    krb5_data second_ticket;
    krb5_pointer **authdata;
} krb5_creds;

typedef krb5_pointer krb5_principal;

#define KRB5_CC_END                              336760974
#define KRB5_TC_OPENCLOSE              0x00000001

/* Ticket flags */
/* flags are 32 bits; each host is responsible to put the 4 bytes
   representing these bits into net order before transmission */
/* #define  TKT_FLG_RESERVED    0x80000000 */
#define TKT_FLG_FORWARDABLE     0x40000000
#define TKT_FLG_FORWARDED       0x20000000
#define TKT_FLG_PROXIABLE       0x10000000
#define TKT_FLG_PROXY           0x08000000
#define TKT_FLG_MAY_POSTDATE    0x04000000
#define TKT_FLG_POSTDATED       0x02000000
#define TKT_FLG_INVALID         0x01000000
#define TKT_FLG_RENEWABLE       0x00800000
#define TKT_FLG_INITIAL         0x00400000
#define TKT_FLG_PRE_AUTH        0x00200000
#define TKT_FLG_HW_AUTH         0x00100000
#ifdef PK_INIT
#define TKT_FLG_PUBKEY_PREAUTH          0x00080000
#define TKT_FLG_DIGSIGN_PREAUTH         0x00040000
#define TKT_FLG_PRIVKEY_PREAUTH         0x00020000
#endif


#define krb5_cc_get_principal(cache, principal) (*(cache)->ops->get_princ)(cache, principal)
#define krb5_cc_set_flags(cache, flags) (*(cache)->ops->set_flags)(cache, flags)
#define krb5_cc_get_name(cache) (*(cache)->ops->get_name)(cache)
#define krb5_cc_start_seq_get(cache, cursor) (*(cache)->ops->get_first)(cache, cursor)
#define krb5_cc_next_cred(cache, cursor, creds) (*(cache)->ops->get_next)(cache, cursor, creds)
#define krb5_cc_destroy(cache) (*(cache)->ops->destroy)(cache)
#define krb5_cc_end_seq_get(cache, cursor) (*(cache)->ops->end_get)(cache, cursor)

/* end of k5 dummy typedefs */

