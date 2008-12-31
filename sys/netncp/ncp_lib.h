/*-
 * Copyright (c) 1999, 2000, 2001 Boris Popov
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
 * $FreeBSD: src/sys/netncp/ncp_lib.h,v 1.7.18.1 2008/11/25 02:59:29 kensmith Exp $
 */

#ifndef _NETNCP_NCP_LIB_H_
#define _NETNCP_NCP_LIB_H_

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

#define STDPARAM_OPT	"A:BCI:M:NO:P:U:R:S:T:W:"

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
	const char	*bn_name;
};

__BEGIN_DECLS

int ncp_args_parserc(struct ncp_args *, char *, ncp_setopt_t *);
int ncp_args_parseopt(struct ncp_args *, int, char *, ncp_setopt_t *);


struct sockaddr_ipx;
struct ipx_addr;
struct sockaddr;
struct ncp_buf;
struct rcfile;

int  ncp_initlib(void);
int  ncp_connect(struct ncp_conn_args *, int *);
int  ncp_connect_addr(struct sockaddr *, NWCONN_HANDLE *);
int  ncp_disconnect(int);
int  ncp_request(int, int, struct ncp_buf *);
int  ncp_conn_request(int, struct ncp_buf *);
int  ncp_login(int, const char *, int, const char *);
int  ncp_conn_scan(struct ncp_conn_loginfo *, int *);
int  ncp_conn_cnt(void);
void *ncp_conn_list(void);
int  ncp_conn_getinfo(int, struct ncp_conn_stat *);
int  ncp_conn_getuser(int, char **);
int  ncp_conn2ref(int, int *);
int  ncp_conn_dup(NWCONN_HANDLE, NWCONN_HANDLE *);
int  ncp_path2conn(char *, int *);
int  ncp_li_init(struct ncp_conn_loginfo *, int, char *[]);
void ncp_li_done(struct ncp_conn_loginfo *);
int  ncp_li_login(struct ncp_conn_loginfo *, int *);
int  ncp_li_readrc(struct ncp_conn_loginfo *);
int  ncp_li_check(struct ncp_conn_loginfo *);
int  ncp_li_arg(struct ncp_conn_loginfo *, int, char *);
int  ncp_li_setserver(struct ncp_conn_loginfo *, const char *);
int  ncp_li_setuser(struct ncp_conn_loginfo *, char *);
int  ncp_li_setpassword(struct ncp_conn_loginfo *, const char *);
int  ncp_conn_setflags(int, u_int16_t, u_int16_t);
int  ncp_conn_find(char *, char *);
NWCCODE NWRequest(NWCONN_HANDLE, nuint16, nuint16, NW_FRAGMENT *,
    nuint16, NW_FRAGMENT *);

#define ncp_setpermanent(connHandle,on)	ncp_conn_setflags(connHandle, NCPFL_PERMANENT, (on) ? NCPFL_PERMANENT : 0)
#define ncp_setprimary(connHandle,on)	ncp_conn_setflags(connHandle, NCPFL_PRIMARY, (on) ? NCPFL_PRIMARY : 0)

int  ncp_find_fileserver(struct ncp_conn_loginfo *, int, char *);
int  ncp_find_server(struct ncp_conn_loginfo *, int, int, char *);

/* misc rotines */
char* ncp_str_upper(char *);
int  ncp_open_rcfile(void);
int  ncp_getopt(int, char * const *, const char *);
void NWUnpackDateTime(nuint32, NW_DATE *, NW_TIME *);
void NWUnpackDate(nuint16, NW_DATE *);
void NWUnpackTime(nuint16, NW_TIME *);
time_t ncp_UnpackDateTime(nuint32);
int  ncp_GetFileServerDateAndTime(NWCONN_HANDLE, time_t *);
int  ncp_SetFileServerDateAndTime(NWCONN_HANDLE, time_t *);
NWCCODE NWDownFileServer(NWCONN_HANDLE, int);
NWCCODE NWCloseBindery(NWCONN_HANDLE);
NWCCODE NWOpenBindery(NWCONN_HANDLE);
NWCCODE NWDisableTTS(NWCONN_HANDLE);
NWCCODE NWEnableTTS(NWCONN_HANDLE);
NWCCODE NWDisableFileServerLogin(NWCONN_HANDLE);
NWCCODE NWEnableFileServerLogin(NWCONN_HANDLE);
void ncp_error(const char *, int, ...) __printf0like(1, 3);
char *ncp_printb(char *, int, const struct ncp_bitname *);
void nw_keyhash(const u_char *, const u_char *, int, u_char *);
void nw_encrypt(const u_char *, const u_char *, u_char *);
void ipx_print_addr(struct ipx_addr *);

/* bindery calls */
int  ncp_get_bindery_object_id(NWCONN_HANDLE, u_int16_t, const char *,
		struct ncp_bindery_object *);
int  ncp_get_bindery_object_name(NWCONN_HANDLE, u_int32_t,
		struct ncp_bindery_object *);
int  ncp_scan_bindery_object(NWCONN_HANDLE, u_int32_t, u_int16_t, 
		char *, struct ncp_bindery_object *);
int  ncp_read_property_value(NWCONN_HANDLE, int object_type, const char *,
		int, const char *, struct nw_property *);
int  ncp_get_encryption_key(NWCONN_HANDLE, char *);
int  ncp_change_obj_passwd(NWCONN_HANDLE, 
	const struct ncp_bindery_object *, const u_char *,
	const u_char *, const u_char *);
int  ncp_keyed_verify_password(NWCONN_HANDLE, char *, char *,
		struct ncp_bindery_object *);

/* queue calls */
int  ncp_create_queue_job_and_file(NWCONN_HANDLE, u_int32_t, struct queue_job *);
int  ncp_close_file_and_start_job(NWCONN_HANDLE, u_int32_t, struct queue_job *);
int  ncp_attach_to_queue(NWCONN_HANDLE, u_int32_t);
int  ncp_detach_from_queue(NWCONN_HANDLE, u_int32_t);
int  ncp_service_queue_job(NWCONN_HANDLE, u_int32_t, u_int16_t,
		struct queue_job *);
int  ncp_finish_servicing_job(NWCONN_HANDLE, u_int32_t, u_int32_t, u_int32_t);
int  ncp_abort_servicing_job(NWCONN_HANDLE, u_int32_t, u_int32_t);
int  ncp_get_queue_length(NWCONN_HANDLE, u_int32_t, u_int32_t *);
int  ncp_get_queue_job_ids(NWCONN_HANDLE, u_int32_t, u_int32_t,
		u_int32_t *, u_int32_t *, u_int32_t []);
int  ncp_get_queue_job_info(NWCONN_HANDLE, u_int32_t, u_int32_t,
		struct nw_queue_job_entry *);
/*
 * filesystem and volume calls 
 */
int  ncp_read(NWCONN_HANDLE, ncp_fh *, off_t, size_t, char *);
int  ncp_write(NWCONN_HANDLE, ncp_fh *, off_t, size_t, char *);
int  ncp_geteinfo(char *, struct nw_entry_info *);
int  ncp_NSEntryInfo(NWCONN_HANDLE, nuint8, nuint8, nuint32, NW_ENTRY_INFO *);

NWCCODE NWGetVolumeName(NWCONN_HANDLE, u_char, char *);

/* misc ncp calls */
int  ncp_get_file_server_information(NWCONN_HANDLE, struct ncp_file_server_info *);
int  ncp_get_stations_logged_info(NWCONN_HANDLE, u_int32_t,
		struct ncp_bindery_object *, time_t *);
int  ncp_get_internet_address(NWCONN_HANDLE, u_int32_t, struct ipx_addr *,
		u_int8_t *);
NWCCODE NWGetObjectConnectionNumbers(NWCONN_HANDLE, pnstr8, nuint16,
		pnuint16, pnuint16, nuint16);
/*
 * Message broadcast
 */
NWCCODE NWDisableBroadcasts(NWCONN_HANDLE);
NWCCODE	NWEnableBroadcasts(NWCONN_HANDLE);
NWCCODE	NWBroadcastToConsole(NWCONN_HANDLE, pnstr8);
NWCCODE NWSendBroadcastMessage(NWCONN_HANDLE, pnstr8, nuint16, pnuint16, pnuint8);
NWCCODE NWGetBroadcastMessage(NWCONN_HANDLE, pnstr8);

/*
 * RPC calls
 */
NWCCODE	NWSMExecuteNCFFile(NWCONN_HANDLE, pnstr8);
NWCCODE	NWSMLoadNLM(NWCONN_HANDLE, pnstr8);
NWCCODE NWSMUnloadNLM(NWCONN_HANDLE, pnstr8);
NWCCODE NWSMMountVolume(NWCONN_HANDLE, pnstr8, nuint32 *);
NWCCODE NWSMDismountVolumeByName(NWCONN_HANDLE, pnstr8);
NWCCODE NWSMSetDynamicCmdIntValue(NWCONN_HANDLE, pnstr8, nuint32);
NWCCODE NWSMSetDynamicCmdStrValue(NWCONN_HANDLE, pnstr8, pnstr8);

__END_DECLS

extern int ncp_opterr, ncp_optind, ncp_optopt, ncp_optreset;
extern char *ncp_optarg;

extern struct rcfile *ncp_rc;
extern int sysentoffset;
#endif /* _NETNCP_NCP_LIB_H_ */
