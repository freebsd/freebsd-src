/*
 * Copyright (c) 1999, Boris Popov
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    This product includes software developed by Boris Popov.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef _NCP_LIB_H_
#define _NCP_LIB_H_

#define IPX
#define INET

#include <netncp/ncp.h>
#include <netncp/ncp_conn.h>
#include <netncp/ncp_user.h>
#include <netncp/ncp_rq.h>

#define ncp_printf printf

#define sipx_cnetwork	sipx_addr.x_net.c_net
#define sipx_node	sipx_addr.x_host.c_host
#define ipx_netlong(iaddr) (((union ipx_net_u *)(&((iaddr).x_net)))->long_e)

#define	STDPARAM_ARGS	'A':case 'B':case 'C':case 'I':case 'M': \
		   case 'N':case 'U':case 'R':case 'S':case 'T': \
		   case 'W':case 'O':case 'P'

#define STDPARAM_OPT	"A:BCI:M:N:O:P:U:R:S:T:W:"

#ifndef min
#define	min(a,b)	(((a)<(b)) ? (a) : (b))
#endif


/*
 * An attempt to do a unified options parser
 */
enum ncp_argtype {NCA_STR,NCA_INT,NCA_BOOL};

struct ncp_args;

typedef int ncp_setopt_t (struct ncp_args*);

#define	NAFL_NONE	0x0000
#define	NAFL_HAVEMIN	0x0001
#define	NAFL_HAVEMAX	0x0002
#define	NAFL_MINMAX	NAFL_HAVEMIN | NAFL_HAVEMAX

struct ncp_args {
	enum ncp_argtype at;
	int	opt;	/* command line option */
	char	*name;	/* rc file equiv */
	int	flag;	/* NAFL_* */
	int	ival;	/* int/bool values, or max len for str value */
	char	*str;	/* string value */
	int	min;	/* min for ival */
	int	max;	/* max for ival */
	ncp_setopt_t *fn;/* call back to validate */
};

typedef struct {
  nuint8    day;
  nuint8    month;
  nuint16   year;
} NW_DATE;

/* hours is a nuint16  so that this structure will be the same length as a dword */
typedef struct {
  nuint8    seconds;
  nuint8    minutes;
  nuint16   hours;
} NW_TIME;

struct ncp_bitname {
	u_int	bn_bit;
	char	*bn_name;
};

int ncp_args_parserc(struct ncp_args *na, char *sect, ncp_setopt_t *set_callback);
int ncp_args_parseopt(struct ncp_args *na, int opt, char *optarg, ncp_setopt_t *set_callback);


struct sockaddr_ipx;
struct ipx_addr;
struct sockaddr;
struct ncp_buf;
struct rcfile;

int  ncp_initlib(void);
int  ncp_connect(struct ncp_conn_args *li, int *connHandle);
int  ncp_connect_addr(struct sockaddr *sa, NWCONN_HANDLE *chp);
int  ncp_disconnect(int connHandle);
int  ncp_request(int connHandle,int function, struct ncp_buf *ncpbuf);
int  ncp_conn_request(int connHandle, struct ncp_buf *ncpbuf);
int  ncp_login(int connHandle, const char *user, int objtype, const char *password);
int  ncp_conn_scan(struct ncp_conn_loginfo *li, int *connHandle);
int  ncp_conn_cnt(void);
void *ncp_conn_list(void);
int  ncp_conn_getinfo(int connHandle, struct ncp_conn_stat *ps);
int  ncp_conn_getuser(int connHandle, char **user);
int  ncp_conn2ref(int connHandle, int *connRef);
int  ncp_conn_dup(NWCONN_HANDLE org, NWCONN_HANDLE *res);
int  ncp_path2conn(char *path, int *connHandle);
int  ncp_li_init(struct ncp_conn_loginfo *li, int argc, char *argv[]);
void ncp_li_done(struct ncp_conn_loginfo *li);
int  ncp_li_login(struct ncp_conn_loginfo *li, int *aconnHandle);
int  ncp_li_readrc(struct ncp_conn_loginfo *li);
int  ncp_li_check(struct ncp_conn_loginfo *li);
int  ncp_li_arg(struct ncp_conn_loginfo *li, int opt, char *arg);
int  ncp_li_setserver(struct ncp_conn_loginfo *li, const char *arg);
int  ncp_li_setuser(struct ncp_conn_loginfo *li, char *arg);
int  ncp_li_setpassword(struct ncp_conn_loginfo *li, const char *passwd);
int  ncp_conn_setflags(int connHandle, u_int16_t mask, u_int16_t flags);
int  ncp_conn_find(char *server, char *user);
NWCCODE NWRequest(NWCONN_HANDLE cH, nuint16 fn,
	nuint16 nrq, NW_FRAGMENT* rq, 
	nuint16 nrp, NW_FRAGMENT* rp) ;

#define ncp_setpermanent(connHandle,on)	ncp_conn_setflags(connHandle, NCPFL_PERMANENT, (on) ? NCPFL_PERMANENT : 0)
#define ncp_setprimary(connHandle,on)	ncp_conn_setflags(connHandle, NCPFL_PRIMARY, (on) ? NCPFL_PRIMARY : 0)

int  ncp_find_fileserver(struct ncp_conn_loginfo *li, int af,char *name);
int  ncp_find_server(struct ncp_conn_loginfo *li, int type, int af,char *name);

/* misc rotines */
char* ncp_str_upper(char *name);
int  ncp_open_rcfile(void);
int  ncp_getopt(int nargc, char * const *nargv, const char *ostr);
void NWUnpackDateTime(nuint32 dateTime, NW_DATE *sDate, NW_TIME *sTime);
void NWUnpackDate(nuint16 date, NW_DATE *sDate);
void NWUnpackTime(nuint16 time, NW_TIME *sTime);
time_t ncp_UnpackDateTime(nuint32 dateTime);
int  ncp_GetFileServerDateAndTime(NWCONN_HANDLE cH, time_t *target);
int  ncp_SetFileServerDateAndTime(NWCONN_HANDLE cH, time_t * source);
NWCCODE NWDownFileServer(NWCONN_HANDLE cH, int force);
NWCCODE NWCloseBindery(NWCONN_HANDLE cH);
NWCCODE NWOpenBindery(NWCONN_HANDLE cH);
NWCCODE NWDisableTTS(NWCONN_HANDLE cH);
NWCCODE NWEnableTTS(NWCONN_HANDLE cH);
NWCCODE NWDisableFileServerLogin(NWCONN_HANDLE cH);
NWCCODE NWEnableFileServerLogin(NWCONN_HANDLE cH);
void ncp_error(char *fmt, int error,...);
char *ncp_printb(char *dest, int flags, const struct ncp_bitname *bnp);
void nw_keyhash(const u_char *key, const u_char *buf, int buflen, u_char *target);
void nw_encrypt(const u_char *fra, const u_char *buf, u_char *target);
void ipx_print_addr(struct ipx_addr *ipx);

/* bindery calls */
int  ncp_get_bindery_object_id(int connHandle, u_int16_t object_type, const char *object_name,
		struct ncp_bindery_object *target);
int  ncp_get_bindery_object_name(int connHandle, u_int32_t object_id, 
		struct ncp_bindery_object *target);
int  ncp_scan_bindery_object(int connHandle, u_int32_t last_id, u_int16_t object_type, 
		char *search_string, struct ncp_bindery_object *target);
int  ncp_read_property_value(int connHandle,int object_type, const char *object_name,
		int segment, const char *prop_name, struct nw_property *target);
void shuffle(const u_char *lon, const u_char *buf, int buflen, u_char *target);
int  ncp_get_encryption_key(NWCONN_HANDLE cH, char *target);
int  ncp_change_obj_passwd(NWCONN_HANDLE connid, 
	const struct ncp_bindery_object *object,
	const u_char *key,
	const u_char *oldpasswd, const u_char *newpasswd);
int  ncp_keyed_verify_password(NWCONN_HANDLE cH, char *key, char *passwd,
			    struct ncp_bindery_object *objinfo);

/* queue calls */
int  ncp_create_queue_job_and_file(int connHandle, u_int32_t queue_id, struct queue_job *job);
int  ncp_close_file_and_start_job(int connHandle, u_int32_t queue_id,  struct queue_job *job);
int  ncp_attach_to_queue(int connHandle, u_int32_t queue_id);
int  ncp_detach_from_queue(int connHandle, u_int32_t queue_id);
int  ncp_service_queue_job(int connHandle, u_int32_t queue_id, u_int16_t job_type,
		struct queue_job *job);
int  ncp_finish_servicing_job(int connHandle, u_int32_t queue_id, u_int32_t job_number,
	u_int32_t charge_info);
int  ncp_abort_servicing_job(int connHandle, u_int32_t queue_id, u_int32_t job_number);
int  ncp_get_queue_length(int connHandle, u_int32_t queue_id, u_int32_t *queue_length);
int  ncp_get_queue_job_ids(int connHandle, u_int32_t queue_id, u_int32_t queue_section,
                       u_int32_t *length1, u_int32_t *length2, u_int32_t ids[]);
int  ncp_get_queue_job_info(int connHandle, u_int32_t queue_id, u_int32_t job_id,
                        struct nw_queue_job_entry *jobdata);
/*
 * file system and volume calls 
 */
int  ncp_read(int connHandle, ncp_fh *fh, off_t offset, size_t count, char *target);
int  ncp_write(int connHandle, ncp_fh *fh, off_t offset, size_t count, char *source);
int  ncp_geteinfo(char *path, struct nw_entry_info *fi);
int  ncp_NSEntryInfo(NWCONN_HANDLE cH, nuint8 ns, nuint8 vol, nuint32 dirent,
	    NW_ENTRY_INFO *entryInfo);

NWCCODE NWGetVolumeName(NWCONN_HANDLE cH, u_char volume, char *name);

/* misc ncp calls */
int  ncp_get_file_server_information(int connHandle, struct ncp_file_server_info *target);
int  ncp_get_stations_logged_info(int connHandle, u_int32_t connection,
		struct ncp_bindery_object *target, time_t *login_time);
int  ncp_get_internet_address(int connHandle, u_int32_t connection, struct ipx_addr *target,
			 u_int8_t * conn_type);
NWCCODE NWGetObjectConnectionNumbers(NWCONN_HANDLE connHandle,
		pnstr8 pObjName, nuint16 objType,
		pnuint16 pNumConns, pnuint16 pConnHandleList,
		nuint16 maxConns);
/*
 * Message broadcast
 */
NWCCODE NWDisableBroadcasts(NWCONN_HANDLE connHandle);
NWCCODE	NWEnableBroadcasts(NWCONN_HANDLE connHandle);
NWCCODE	NWBroadcastToConsole(NWCONN_HANDLE  connHandle, pnstr8 message);
NWCCODE NWSendBroadcastMessage(NWCONN_HANDLE  connHandle, pnstr8 message,
	    nuint16 connCount, pnuint16 connList, pnuint8 resultList);
NWCCODE NWGetBroadcastMessage(NWCONN_HANDLE connHandle, pnstr8 message);

/*
 * RPC calls
 */
NWCCODE	NWSMExecuteNCFFile(NWCONN_HANDLE cH, pnstr8 NCFFileName);
NWCCODE	NWSMLoadNLM(NWCONN_HANDLE cH, pnstr8 cmd);
NWCCODE NWSMUnloadNLM(NWCONN_HANDLE cH, pnstr8 cmd);
NWCCODE NWSMMountVolume(NWCONN_HANDLE cH, pnstr8 volName, nuint32* volnum);
NWCCODE NWSMDismountVolumeByName(NWCONN_HANDLE cH, pnstr8 vol);
NWCCODE NWSMSetDynamicCmdIntValue(NWCONN_HANDLE cH, pnstr8 setCommandName, nuint32 cmdValue);
NWCCODE NWSMSetDynamicCmdStrValue(NWCONN_HANDLE cH, pnstr8 setCommandName, pnstr8 cmdValue);

int dostat(int modnum, char *modname, int *offset);

extern int  ncp_opterr, ncp_optind, ncp_optopt, ncp_optreset;
extern char *ncp_optarg;

extern struct rcfile *ncp_rc;
extern int sysentoffset;
#endif /* _NCP_LIB_H_ */
