#define YPMAXRECORD 1024
#define YPMAXDOMAIN 64
#define YPMAXMAP 64
#define YPMAXPEER 64

enum ypstat {
	YP_TRUE = 1,
	YP_NOMORE = 2,
	YP_FALSE = 0,
	YP_NOMAP = -1,
	YP_NODOM = -2,
	YP_NOKEY = -3,
	YP_BADOP = -4,
	YP_BADDB = -5,
	YP_YPERR = -6,
	YP_BADARGS = -7,
	YP_VERS = -8,
};
typedef enum ypstat ypstat;
bool_t xdr_ypstat();


enum ypxfrstat {
	YPXFR_SUCC = 1,
	YPXFR_AGE = 2,
	YPXFR_NOMAP = -1,
	YPXFR_NODOM = -2,
	YPXFR_RSRC = -3,
	YPXFR_RPC = -4,
	YPXFR_MADDR = -5,
	YPXFR_YPERR = -6,
	YPXFR_BADARGS = -7,
	YPXFR_DBM = -8,
	YPXFR_FILE = -9,
	YPXFR_SKEW = -10,
	YPXFR_CLEAR = -11,
	YPXFR_FORCE = -12,
	YPXFR_XFRERR = -13,
	YPXFR_REFUSED = -14,
};
typedef enum ypxfrstat ypxfrstat;
bool_t xdr_ypxfrstat();


typedef char *domainname;
bool_t xdr_domainname();


typedef char *mapname;
bool_t xdr_mapname();


typedef char *peername;
bool_t xdr_peername();


typedef struct {
	u_int keydat_len;
	char *keydat_val;
} keydat;
bool_t xdr_keydat();


typedef struct {
	u_int valdat_len;
	char *valdat_val;
} valdat;
bool_t xdr_valdat();


struct ypmap_parms {
	domainname domain;
	mapname map;
	u_int ordernum;
	peername peer;
};
typedef struct ypmap_parms ypmap_parms;
bool_t xdr_ypmap_parms();


struct ypreq_key {
	domainname domain;
	mapname map;
	keydat key;
};
typedef struct ypreq_key ypreq_key;
bool_t xdr_ypreq_key();


struct ypreq_nokey {
	domainname domain;
	mapname map;
};
typedef struct ypreq_nokey ypreq_nokey;
bool_t xdr_ypreq_nokey();


struct ypreq_xfr {
	ypmap_parms map_parms;
	u_int transid;
	u_int prog;
	u_int port;
};
typedef struct ypreq_xfr ypreq_xfr;
bool_t xdr_ypreq_xfr();


struct ypresp_val {
	ypstat stat;
	valdat val;
};
typedef struct ypresp_val ypresp_val;
bool_t xdr_ypresp_val();


struct ypresp_key_val {
	ypstat stat;
	keydat key;
	valdat val;
};
typedef struct ypresp_key_val ypresp_key_val;
bool_t xdr_ypresp_key_val();


struct ypresp_master {
	ypstat stat;
	peername peer;
};
typedef struct ypresp_master ypresp_master;
bool_t xdr_ypresp_master();


struct ypresp_order {
	ypstat stat;
	u_int ordernum;
};
typedef struct ypresp_order ypresp_order;
bool_t xdr_ypresp_order();


struct ypresp_all {
	bool_t more;
	union {
		ypresp_key_val val;
	} ypresp_all_u;
};
typedef struct ypresp_all ypresp_all;
bool_t __xdr_ypresp_all();


struct ypresp_xfr {
	u_int transid;
	ypxfrstat xfrstat;
};
typedef struct ypresp_xfr ypresp_xfr;
bool_t xdr_ypresp_xfr();


struct ypmaplist {
	mapname map;
	struct ypmaplist *next;
};
typedef struct ypmaplist ypmaplist;
bool_t xdr_ypmaplist();


struct ypresp_maplist {
	ypstat stat;
	ypmaplist *maps;
};
typedef struct ypresp_maplist ypresp_maplist;
bool_t xdr_ypresp_maplist();


enum yppush_status {
	YPPUSH_SUCC = 1,
	YPPUSH_AGE = 2,
	YPPUSH_NOMAP = -1,
	YPPUSH_NODOM = -2,
	YPPUSH_RSRC = -3,
	YPPUSH_RPC = -4,
	YPPUSH_MADDR = -5,
	YPPUSH_YPERR = -6,
	YPPUSH_BADARGS = -7,
	YPPUSH_DBM = -8,
	YPPUSH_FILE = -9,
	YPPUSH_SKEW = -10,
	YPPUSH_CLEAR = -11,
	YPPUSH_FORCE = -12,
	YPPUSH_XFRERR = -13,
	YPPUSH_REFUSED = -14,
};
typedef enum yppush_status yppush_status;
bool_t xdr_yppush_status();


struct yppushresp_xfr {
	u_int transid;
	yppush_status status;
};
typedef struct yppushresp_xfr yppushresp_xfr;
bool_t xdr_yppushresp_xfr();


enum ypbind_resptype {
	YPBIND_SUCC_VAL = 1,
	YPBIND_FAIL_VAL = 2,
};
typedef enum ypbind_resptype ypbind_resptype;
bool_t xdr_ypbind_resptype();


struct ypbind_binding {
	char ypbind_binding_addr[4];
	char ypbind_binding_port[2];
};
typedef struct ypbind_binding ypbind_binding;
bool_t xdr_ypbind_binding();


struct ypbind_resp {
	ypbind_resptype ypbind_status;
	union {
		u_int ypbind_error;
		ypbind_binding ypbind_bindinfo;
	} ypbind_resp_u;
};
typedef struct ypbind_resp ypbind_resp;
bool_t xdr_ypbind_resp();

#define YPBIND_ERR_ERR 1
#define YPBIND_ERR_NOSERV 2
#define YPBIND_ERR_RESC 3

struct ypbind_setdom {
	domainname ypsetdom_domain;
	ypbind_binding ypsetdom_binding;
	u_int ypsetdom_vers;
};
typedef struct ypbind_setdom ypbind_setdom;
bool_t xdr_ypbind_setdom();


#define YPPROG ((u_long)100004)
#define YPVERS ((u_long)2)
#define YPPROC_NULL ((u_long)0)
extern void *ypproc_null_2();
#define YPPROC_DOMAIN ((u_long)1)
extern bool_t *ypproc_domain_2();
#define YPPROC_DOMAIN_NONACK ((u_long)2)
extern bool_t *ypproc_domain_nonack_2();
#define YPPROC_MATCH ((u_long)3)
extern ypresp_val *ypproc_match_2();
#define YPPROC_FIRST ((u_long)4)
extern ypresp_key_val *ypproc_first_2();
#define YPPROC_NEXT ((u_long)5)
extern ypresp_key_val *ypproc_next_2();
#define YPPROC_XFR ((u_long)6)
extern ypresp_xfr *ypproc_xfr_2();
#define YPPROC_CLEAR ((u_long)7)
extern void *ypproc_clear_2();
#define YPPROC_ALL ((u_long)8)
extern ypresp_all *ypproc_all_2();
#define YPPROC_MASTER ((u_long)9)
extern ypresp_master *ypproc_master_2();
#define YPPROC_ORDER ((u_long)10)
extern ypresp_order *ypproc_order_2();
#define YPPROC_MAPLIST ((u_long)11)
extern ypresp_maplist *ypproc_maplist_2();


#define YPPUSH_XFRRESPPROG ((u_long)0x40000000)
#define YPPUSH_XFRRESPVERS ((u_long)1)
#define YPPUSHPROC_NULL ((u_long)0)
extern void *yppushproc_null_1();
#define YPPUSHPROC_XFRRESP ((u_long)1)
extern yppushresp_xfr *yppushproc_xfrresp_1();


#define YPBINDPROG ((u_long)100007)
#define YPBINDVERS ((u_long)2)
#define YPBINDPROC_NULL ((u_long)0)
extern void *ypbindproc_null_2();
#define YPBINDPROC_DOMAIN ((u_long)1)
extern ypbind_resp *ypbindproc_domain_2();
#define YPBINDPROC_SETDOM ((u_long)2)
extern void *ypbindproc_setdom_2();

