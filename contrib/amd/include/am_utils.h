/*
 * Copyright (c) 1997-2003 Erez Zadok
 * Copyright (c) 1990 Jan-Simon Pendry
 * Copyright (c) 1990 Imperial College of Science, Technology & Medicine
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Jan-Simon Pendry at Imperial College, London.
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
 *    must display the following acknowledgment:
 *      This product includes software developed by the University of
 *      California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *      %W% (Berkeley) %G%
 *
 * $Id: am_utils.h,v 1.11.2.12 2002/12/27 22:45:10 ezk Exp $
 *
 */

/*
 * Definitions that are specific to the am-utils package.
 */

#ifndef _AM_UTILS_H
#define _AM_UTILS_H


/**************************************************************************/
/*** MACROS								***/
/**************************************************************************/

/*
 * General macros.
 */
#ifndef FALSE
# define FALSE 0
#endif /* not FALSE */
#ifndef TRUE
# define TRUE 1
#endif /* not TRUE */
#ifndef MAX
# define MAX(a, b)	((a) > (b) ? (a) : (b))
#endif /* not MAX */
#ifndef MIN
# define MIN(a, b)	((a) < (b) ? (a) : (b))
#endif /* not MIN */

#define	ONE_HOUR	(60 * 60)	/* One hour in seconds */

#ifndef MAXHOSTNAMELEN
# ifdef HOSTNAMESZ
#  define MAXHOSTNAMELEN HOSTNAMESZ
# else /* not HOSTNAMESZ */
#  define MAXHOSTNAMELEN 256
# endif /* not HOSTNAMESZ */
#endif /* not MAXHOSTNAMELEN */

/*
 * for hlfsd, and amd for detecting uid/gid
 */
#ifndef INVALIDID
/* this is also defined in include/am_utils.h */
# define INVALIDID	(((unsigned short) ~0) - 3)
#endif /* not INVALIDID */

/*
 * String comparison macros
 */
#define STREQ(s1, s2)		(strcmp((s1), (s2)) == 0)
#define STRCEQ(s1, s2)		(strcasecmp((s1), (s2)) == 0)
#define NSTREQ(s1, s2, n)	(strncmp((s1), (s2), (n)) == 0)
#define NSTRCEQ(s1, s2, n)	(strncasecmp((s1), (s2), (n)) == 0)
#define FSTREQ(s1, s2)		((*(s1) == *(s2)) && STREQ((s1),(s2)))

/*
 * Logging options/flags
 */
#define	XLOG_FATAL	0x0001
#define	XLOG_ERROR	0x0002
#define	XLOG_USER	0x0004
#define	XLOG_WARNING	0x0008
#define	XLOG_INFO	0x0010
#define	XLOG_DEBUG	0x0020
#define	XLOG_MAP	0x0040
#define	XLOG_STATS	0x0080
#define XLOG_DEFSTR	"all,nomap,nostats"	/* Default log options */
#define XLOG_ALL	(XLOG_FATAL|XLOG_ERROR|XLOG_USER|XLOG_WARNING|XLOG_INFO|XLOG_MAP|XLOG_STATS)

#define clocktime() (clock_valid ? clock_valid : time(&clock_valid))

#ifndef ROOT_MAP
# define ROOT_MAP "\"root\""
#endif /* not ROOT_MAP */

#define NO_SUBNET	"notknown"	/* default subnet name for no subnet */
#define	NEXP_AP		(1022)	/* gdmr: was 254 */
#define NEXP_AP_MARGIN	(128)
#define	MAX_READDIR_ENTRIES	16

/*
 * Linked list macros
 */
#define	AM_FIRST(ty, q)	((ty *) ((q)->q_forw))
#define	AM_LAST(ty, q)	((ty *) ((q)->q_back))
#define	NEXT(ty, q)	((ty *) (((qelem *) q)->q_forw))
#define	PREV(ty, q)	((ty *) (((qelem *) q)->q_back))
#define	HEAD(ty, q)	((ty *) q)
#define	ITER(v, ty, q) \
	for ((v) = AM_FIRST(ty,(q)); (v) != HEAD(ty,(q)); (v) = NEXT(ty,(v)))

/* allocate anything of type ty */
#define	ALLOC(ty)	((ty *) xmalloc(sizeof(ty)))
#define	CALLOC(ty)	((ty *) xcalloc(1, sizeof(ty)))
/* simply allocate b bytes */
#define	SALLOC(b)	xmalloc((b))

/* converting am-filehandles to mount-points */
#define	fh_to_mp2(fhp, rp) fh_to_mp3(fhp, rp, VLOOK_CREATE)

/*
 * Systems which have the mount table in a file need to read it before
 * they can perform an unmount() system call.
 */
#define UMOUNT_FS(dir, mtb_name)	umount_fs(dir, mtb_name)
/* imported via $srcdir/conf/umount/umount_*.c */
extern int umount_fs(char *fs_name, const char *mnttabname);

/*
 * macros for automounter vfs/vnode operations.
 */
#define	VLOOK_CREATE	0x1
#define	VLOOK_DELETE	0x2
#define FS_DIRECTORY	0x0001	/* This looks like a dir, not a link */
#define	FS_MBACKGROUND	0x0002	/* Should background this mount */
#define	FS_NOTIMEOUT	0x0004	/* Don't bother with timeouts */
#define FS_MKMNT	0x0008	/* Need to make the mount point */
#define FS_UBACKGROUND	0x0010	/* Unmount in background */
#define	FS_BACKGROUND	(FS_MBACKGROUND|FS_UBACKGROUND)
#define	FS_DISCARD	0x0020	/* Discard immediately on last reference */
#define	FS_AMQINFO	0x0040	/* Amq is interested in this fs type */

/*
 * macros for struct fserver.
 */
#define	FSF_VALID	0x0001	/* Valid information available */
#define	FSF_DOWN	0x0002	/* This fileserver is thought to be down */
#define	FSF_ERROR	0x0004	/* Permanent error has occurred */
#define	FSF_WANT	0x0008	/* Want a wakeup call */
#define	FSF_PINGING	0x0010	/* Already doing pings */
#define	FSRV_ISDOWN(fs)	(((fs)->fs_flags & (FSF_DOWN|FSF_VALID)) == (FSF_DOWN|FSF_VALID))
#define	FSRV_ISUP(fs)	(((fs)->fs_flags & (FSF_DOWN|FSF_VALID)) == (FSF_VALID))

/*
 * macros for struct mntfs (list of mounted filesystems)
 */
#define	MFF_MOUNTED	0x0001	/* Node is mounted */
#define	MFF_MOUNTING	0x0002	/* Mount is in progress */
#define	MFF_UNMOUNTING	0x0004	/* Unmount is in progress */
#define	MFF_RESTART	0x0008	/* Restarted node */
#define MFF_MKMNT	0x0010	/* Delete this node's am_mount */
#define	MFF_ERROR	0x0020	/* This node failed to mount */
#define	MFF_LOGDOWN	0x0040	/* Logged that this mount is down */
#define	MFF_RSTKEEP	0x0080	/* Don't timeout this filesystem - restarted */
#define	MFF_WANTTIMO	0x0100	/* Need a timeout call when not busy */
#ifdef HAVE_AMU_FS_NFSL
# define MFF_NFSLINK	0x0200	/* nfsl type, and deemed a link */
#endif /* HAVE_AMU_FS_NFSL */

/*
 * macros for struct am_node (map of auto-mount points).
 */
#define	AMF_NOTIMEOUT	0x0001	/* This node never times out */
#define	AMF_ROOT	0x0002	/* This is a root node */
#define AMF_AUTOFS	0x0004	/* This node is of type autofs -- not yet supported */

/*
 * The following values can be tuned...
 */
#define	ALLOWED_MOUNT_TIME	40	/* 40s for a mount */
#define	AM_TTL			(5 * 60)	/* Default cache period */
#define	AM_TTL_W		(2 * 60)	/* Default unmount interval */
#define	AM_PINGER		30 /* NFS ping interval for live systems */
#define	AMFS_AUTO_TIMEO		8 /* Default amfs_auto timeout - .8s */

/*
 * default amfs_auto retrans - 1/10th seconds
 */
#define	AMFS_AUTO_RETRANS	((ALLOWED_MOUNT_TIME*10+5*gopt.amfs_auto_timeo)/gopt.amfs_auto_timeo * 2)

/*
 * RPC-related macros.
 */
#define	RPC_XID_PORTMAP		0
#define	RPC_XID_MOUNTD		1
#define	RPC_XID_NFSPING		2
#define	RPC_XID_MASK		(0x0f)	/* 16 id's for now */
#define	MK_RPC_XID(type_id, uniq)	((type_id) | ((uniq) << 4))

/*
 * What level of AMD are we backward compatible with?
 * This only applies to externally visible characteristics.
 * Rev.Minor.Branch.Patch (2 digits each)
 */
#define	AMD_COMPAT	5000000	/* 5.0 */

/*
 * Error to return if remote host is not available.
 * Try, in order, "host down", "host unreachable", "invalid argument".
 */
#ifdef EHOSTDOWN
# define AM_ERRNO_HOST_DOWN	EHOSTDOWN
# else /* not EHOSTDOWN */
# ifdef EHOSTUNREACH
#  define AM_ERRNO_HOST_DOWN	EHOSTUNREACH
# else /* not EHOSTUNREACH */
#  define AM_ERRNO_HOST_DOWN	EINVAL
# endif /* not EHOSTUNREACH */
#endif /* not EHOSTDOWN */


/**************************************************************************/
/*** STRUCTURES AND TYPEDEFS						***/
/**************************************************************************/

/* some typedefs must come first */
typedef char *amq_string;
typedef struct mntfs mntfs;
typedef struct am_opts am_opts;
typedef struct am_ops am_ops;
typedef struct am_node am_node;
typedef struct _qelem qelem;
typedef struct mntlist mntlist;
typedef struct fserver fserver;

/*
 * Linked list
 * (the name 'struct qelem' conflicts with linux's unistd.h)
 */
struct _qelem {
  qelem *q_forw;
  qelem *q_back;
};

/*
 * Option tables
 */
struct opt_tab {
  char *opt;
  int flag;
};

/*
 * Server states
 */
typedef enum {
  Start,
  Run,
  Finishing,
  Quit,
  Done
} serv_state;

/*
 * Options
 */
struct am_opts {
  char *fs_glob;		/* Smashed copy of global options */
  char *fs_local;		/* Expanded copy of local options */
  char *fs_mtab;		/* Mount table entry */
  /* Other options ... */
  char *opt_dev;
  char *opt_delay;
  char *opt_dir;
  char *opt_fs;
  char *opt_group;
  char *opt_mount;
  char *opt_opts;
  char *opt_remopts;
  char *opt_pref;
  char *opt_cache;
  char *opt_rfs;
  char *opt_rhost;
  char *opt_sublink;
  char *opt_type;
  char *opt_unmount;
  char *opt_user;
  char *opt_maptype;		/* map type: file, nis, hesiod, etc. */
  char *opt_cachedir;		/* cache directory */
  char *opt_addopts;		/* options to add to opt_opts */
};

/*
 * List of mounted filesystems
 */
struct mntfs {
  qelem mf_q;			/* List of mounted filesystems */
  am_ops *mf_ops;		/* Operations on this mountpoint */
  am_opts *mf_fo;		/* File opts */
  char *mf_mount;		/* "/a/kiska/home/kiska" */
  char *mf_info;		/* Mount info */
  char *mf_auto;		/* Automount opts */
  char *mf_mopts;		/* FS mount opts */
  char *mf_remopts;		/* Remote FS mount opts */
  fserver *mf_server;		/* File server */
  int mf_flags;			/* Flags MFF_* */
  int mf_error;			/* Error code from background mount */
  int mf_refc;			/* Number of references to this node */
  int mf_cid;			/* Callout id */
  void (*mf_prfree) (voidp);	/* Free private space */
  voidp mf_private;		/* Private - per-fs data */
};

/*
 * File Handle
 *
 * This is interpreted by indexing the exported array
 * by fhh_id.
 *
 * The whole structure is mapped onto a standard fhandle_t
 * when transmitted.
 */
struct am_fh {
  int fhh_pid;			/* process id */
  int fhh_id;			/* map id */
  int fhh_gen;			/* generation number */
};

/*
 * Multi-protocol NFS file handle
 */
union am_nfs_handle {
				/* placeholder for V4 file handle */
#ifdef HAVE_FS_NFS3
  struct mountres3	v3;	/* NFS version 3 handle */
#endif /* HAVE_FS_NFS3 */
  struct fhstatus	v2;	/* NFS version 2 handle */
};
typedef union am_nfs_handle am_nfs_handle_t;

/*
 * automounter vfs/vnode operations.
 */
typedef char *(*vfs_match) (am_opts *);
typedef int (*vfs_init) (mntfs *);
typedef int (*vmount_fs) (am_node *);
typedef int (*vfmount_fs) (mntfs *);
typedef int (*vumount_fs) (am_node *);
typedef int (*vfumount_fs) (mntfs *);
typedef am_node *(*vlookuppn) (am_node *, char *, int *, int);
typedef int (*vreaddir) (am_node *, nfscookie, nfsdirlist *, nfsentry *, int);
typedef am_node *(*vreadlink) (am_node *, int *);
typedef void (*vmounted) (mntfs *);
typedef void (*vumounted) (am_node *);
typedef fserver *(*vffserver) (mntfs *);

struct am_ops {
  char		*fs_type;	/* type of filesystems "nfsx" */
  vfs_match	fs_match;	/* fxn: match */
  vfs_init	fs_init;	/* fxn: initialization */
  vmount_fs	mount_fs;	/* fxn: mount vnode */
  vfmount_fs	fmount_fs;	/* fxn: mount VFS */
  vumount_fs	umount_fs;	/* fxn: unmount vnode */
  vfumount_fs	fumount_fs;	/* fxn: unmount VFS */
  vlookuppn	lookuppn;	/* fxn: lookup path-name */
  vreaddir	readdir;	/* fxn: read directory */
  vreadlink	readlink;	/* fxn: read link */
  vmounted	mounted;	/* fxn: after-mount extra actions */
  vumounted	umounted;	/* fxn: after-umount extra actions */
  vffserver	ffserver;	/* fxn: find a file server */
  int		fs_flags;	/* filesystem flags FS_* */
};

typedef int (*task_fun) (voidp);
typedef void (*cb_fun) (int, int, voidp);
typedef void (*fwd_fun) P((voidp, int, struct sockaddr_in *,
			   struct sockaddr_in *, voidp, int));

/*
 * List of mount table entries
 */
struct mntlist {
  struct mntlist *mnext;
  mntent_t *mnt;
};

/*
 * Mount map
 */
typedef struct mnt_map mnt_map;

/*
 * Per-mountpoint statistics
 */
struct am_stats {
  time_t s_mtime;		/* Mount time */
  u_short s_uid;		/* Uid of mounter */
  int s_getattr;		/* Count of getattrs */
  int s_lookup;			/* Count of lookups */
  int s_readdir;		/* Count of readdirs */
  int s_readlink;		/* Count of readlinks */
  int s_statfs;			/* Count of statfs */
};
typedef struct am_stats am_stats;

/*
 * System statistics
 */
struct amd_stats {
  int d_drops;			/* Dropped requests */
  int d_stale;			/* Stale NFS handles */
  int d_mok;			/* Successful mounts */
  int d_merr;			/* Failed mounts */
  int d_uerr;			/* Failed unmounts */
};
extern struct amd_stats amd_stats;

/*
 * List of fileservers
 */
struct fserver {
  qelem fs_q;			/* List of fileservers */
  int fs_refc;			/* Number of references to this node */
  char *fs_host;		/* Normalized hostname of server */
  struct sockaddr_in *fs_ip;	/* Network address of server */
  int fs_cid;			/* Callout id */
  int fs_pinger;		/* Ping (keepalive) interval */
  int fs_flags;			/* Flags */
  char *fs_type;		/* File server type */
  u_long fs_version;		/* NFS version of server (2, 3, etc.)*/
  char *fs_proto;		/* NFS protocol of server (tcp, udp, etc.) */
  voidp fs_private;		/* Private data */
  void (*fs_prfree) (voidp);	/* Free private data */
};

/*
 * Map of auto-mount points.
 */
struct am_node {
  int am_mapno;		/* Map number */
  mntfs *am_mnt;	/* Mounted filesystem */
  char *am_name;	/* "kiska": name of this node */
  char *am_path;	/* "/home/kiska": path of this node's mount point */
  char *am_link;	/* "/a/kiska/home/kiska/this/that": link to sub-dir */
  am_node *am_parent;	/* Parent of this node */
  am_node *am_ysib;	/* Younger sibling of this node */
  am_node *am_osib;	/* Older sibling of this node */
  am_node *am_child;	/* First child of this node */
  nfsattrstat am_attr;	/* File attributes */
#define am_fattr	am_attr.ns_u.ns_attr_u
  int am_flags;		/* Boolean flags AMF_* */
  int am_error;		/* Specific mount error */
  time_t am_ttl;	/* Time to live */
  int am_timeo_w;	/* Wait interval */
  int am_timeo;		/* Timeout interval */
  u_int am_gen;		/* Generation number */
  char *am_pref;	/* Mount info prefix */
  am_stats am_stats;	/* Statistics gathering */
  SVCXPRT *am_transp;	/* Info for quick reply */
};


/**************************************************************************/
/*** EXTERNALS								***/
/**************************************************************************/

/*
 * Useful constants
 */
extern char *mnttab_file_name;	/* Mount table */
extern char *cpu;		/* "CPU type" */
extern char *endian;		/* "big" */
extern char *hostdomain;	/* "southseas.nz" */
extern char copyright[];	/* Copyright info */
extern char hostd[];		/* "kiska.southseas.nz" */
extern char pid_fsname[];	/* kiska.southseas.nz:(pid%d) */
extern char version[];		/* Version info */

/*
 * Global variables.
 */
extern AUTH *nfs_auth;		/* Dummy authorization for remote servers */
extern FILE *logfp;		/* Log file */
extern SVCXPRT *nfsxprt;
extern am_node **exported_ap;	/* List of nodes */
extern am_node *root_node;	/* Node for "root" */
extern char *PrimNetName;	/* Name of primary connected network */
extern char *PrimNetNum;	/* Name of primary connected network */
extern char *SubsNetName;	/* Name of subsidiary connected network */
extern char *SubsNetNum;	/* Name of subsidiary connected network */

extern void am_set_progname(char *pn); /* "amd" */
extern const char *am_get_progname(void); /* "amd" */
extern void am_set_hostname(char *hn);
extern const char *am_get_hostname(void);
extern pid_t am_set_mypid(void);
extern pid_t am_mypid;

extern int first_free_map;	/* First free node */
extern int foreground;		/* Foreground process */
extern int immediate_abort;	/* Should close-down unmounts be retried */
extern int last_used_map;	/* Last map being used for mounts */
extern int orig_umask;		/* umask() on startup */
extern int task_notify_todo;	/* Task notifier needs running */
extern int xlog_level;		/* Logging level */
extern int xlog_level_init;
extern serv_state amd_state;	/* Should we go now */
extern struct in_addr myipaddr;	/* (An) IP address of this host */
extern struct opt_tab xlog_opt[];
extern time_t clock_valid;	/* Clock needs recalculating */
extern time_t do_mapc_reload;	/* Flush & reload mount map cache */
extern time_t next_softclock;	/* Time to call softclock() */
extern u_short nfs_port;	/* Our NFS service port */

/*
 * Global routines
 */
extern CLIENT *get_mount_client(char *unused_host, struct sockaddr_in *sin, struct timeval *tv, int *sock, u_long mnt_version);
extern RETSIGTYPE sigchld(int);
extern am_node *efs_lookuppn(am_node *, char *, int *, int);
extern am_node *exported_ap_alloc(void);
extern am_node *fh_to_mp(am_nfs_fh *);
extern am_node *fh_to_mp3(am_nfs_fh *, int *, int);
extern am_node *find_mf(mntfs *);
extern am_node *next_map(int *);
extern am_node *root_ap(char *, int);
extern am_ops *ops_match(am_opts *, char *, char *, char *, char *, char *);
extern bool_t xdr_amq_string(XDR *xdrs, amq_string *objp);
extern bool_t xdr_dirpath(XDR *xdrs, dirpath *objp);
extern char **strsplit(char *, int, int);
extern char *expand_key(char *);
extern char *get_version_string(void);
extern char *inet_dquad(char *, u_long);
extern char *print_wires(void);
extern char *str3cat(char *, char *, char *, char *);
extern char *strealloc(char *, char *);
extern char *strip_selectors(char *, char *);
extern char *strnsave(const char *, int);
extern fserver *dup_srvr(fserver *);
extern int amu_close(int fd);
extern int background(void);
extern int bind_resv_port(int, u_short *);
extern int cmdoption(char *, struct opt_tab *, int *);
extern int compute_automounter_mount_flags(mntent_t *);
extern int compute_mount_flags(mntent_t *);
extern int efs_readdir(am_node *, nfscookie, nfsdirlist *, nfsentry *, int);
extern int eval_fs_opts(am_opts *, char *, char *, char *, char *, char *);
extern int fwd_init(void);
extern int fwd_packet(int, voidp, int, struct sockaddr_in *, struct sockaddr_in *, voidp, fwd_fun);
extern int get_amd_program_number(void);
extern int getcreds(struct svc_req *, uid_t *, gid_t *, SVCXPRT *);
extern int hasmntval(mntent_t *, char *);
extern char *hasmnteq(mntent_t *, char *);
extern char *haseq(char *);
extern int is_network_member(const char *net);
extern int islocalnet(u_long);
extern int make_nfs_auth(void);
extern int make_rpc_packet(char *, int, u_long, struct rpc_msg *, voidp, XDRPROC_T_TYPE, AUTH *);
extern int mapc_keyiter(mnt_map *, void(*)(char *, voidp), voidp);
extern int mapc_search(mnt_map *, char *, char **);
extern int mapc_type_exists(const char *type);
extern int mkdirs(char *, int);
extern int mount_auto_node(char *, voidp);
extern int mount_automounter(int);
extern int mount_exported(void);
extern int mount_fs(mntent_t *, int, caddr_t, int, MTYPE_TYPE, u_long, const char *, const char *);
extern int mount_node(am_node *);
extern int nfs_srvr_port(fserver *, u_short *, voidp);
extern int pickup_rpc_reply(voidp, int, voidp, XDRPROC_T_TYPE);
extern int root_keyiter(void(*)(char *, voidp), voidp);
extern int softclock(void);
extern int switch_option(char *);
extern int switch_to_logfile(char *logfile, int orig_umask);
extern int timeout(u_int, void (*fn)(voidp), voidp);
extern int valid_key(char *);
extern mnt_map *mapc_find(char *, char *, const char *);
extern mntfs *dup_mntfs(mntfs *);
extern mntfs *find_mntfs(am_ops *, am_opts *, char *, char *, char *, char *, char *);
extern mntfs *new_mntfs(void);
extern mntfs *realloc_mntfs(mntfs *, am_ops *, am_opts *, char *, char *, char *, char *, char *);
extern mntlist *read_mtab(char *, const char *);
extern struct sockaddr_in *amu_svc_getcaller(SVCXPRT *xprt);
extern time_t time(time_t *);
extern void am_mounted(am_node *);
extern void am_unmounted(am_node *);
extern void amq_program_1(struct svc_req *rqstp, SVCXPRT *transp);
extern void amu_get_myaddress(struct in_addr *iap);
extern void amu_release_controlling_tty(void);
extern void compute_automounter_nfs_args(nfs_args_t *nap, mntent_t *mntp);
extern void deslashify(char *);
extern void discard_mntlist(mntlist *mp);
extern void do_task_notify(void);
extern void flush_mntfs(void);
extern void flush_nfs_fhandle_cache(fserver *);
extern void forcibly_timeout_mp(am_node *);
extern void free_map(am_node *);
extern void free_mntfs(voidp);
extern void free_mntlist(mntlist *);
extern void free_opts(am_opts *);
extern void free_srvr(fserver *);
extern void fwd_reply(void);
extern void get_args(int argc, char *argv[]);
extern void getwire(char **name1, char **number1);
extern void going_down(int);
extern void host_normalize(char **);
extern void init_map(am_node *, char *);
extern void ins_que(qelem *, qelem *);
extern void insert_am(am_node *, am_node *);
extern void make_root_node(void);
extern void map_flush_srvr(fserver *);
extern void mapc_add_kv(mnt_map *, char *, char *);
extern void mapc_free(voidp);
extern void mapc_reload(void);
extern void mapc_showtypes(char *buf);
extern void mk_fattr(am_node *, nfsftype);
extern void mnt_free(mntent_t *);
extern void mp_to_fh(am_node *, am_nfs_fh *);
extern void new_ttl(am_node *);
extern void nfs_program_2(struct svc_req *rqstp, SVCXPRT *transp);
extern void normalize_slash(char *);
extern void ops_showamfstypes(char *buf);
extern void ops_showfstypes(char *outbuf);
extern void plog(int, const char *,...)
     __attribute__ ((__format__ (__printf__, 2, 3)));
extern void rem_que(qelem *);
extern void reschedule_timeout_mp(void);
extern void restart(void);
extern void rmdirs(char *);
extern void rpc_msg_init(struct rpc_msg *, u_long, u_long, u_long);
extern void run_task(task_fun, voidp, cb_fun, voidp);
extern void sched_task(cb_fun, voidp, voidp);
extern void set_amd_program_number(int program);
extern void show_opts(int ch, struct opt_tab *);
extern void show_rcs_info(const char *, char *);
extern void srvrlog(fserver *, char *);
extern void timeout_mp(voidp);
extern void umount_exported(void);
extern void unregister_amq(void);
extern void untimeout(int);
extern void wakeup(voidp);
extern void wakeup_srvr(fserver *);
extern void wakeup_task(int, int, voidp);
extern voidp xmalloc(int);
extern voidp xrealloc(voidp, int);
extern voidp xzalloc(int);
extern u_long get_nfs_version(char *host, struct sockaddr_in *sin, u_long nfs_version, const char *proto);


#ifdef MOUNT_TABLE_ON_FILE
extern void rewrite_mtab(mntlist *, const char *);
extern void unlock_mntlist(void);
extern void write_mntent(mntent_t *, const char *);
#endif /* MOUNT_TABLE_ON_FILE */

#if defined(HAVE_SYSLOG_H) || defined(HAVE_SYS_SYSLOG_H)
extern int syslogging;
#endif /* defined(HAVE_SYSLOG_H) || defined(HAVE_SYS_SYSLOG_H) */

#ifdef HAVE_TRANSPORT_TYPE_TLI

extern void compute_nfs_args(nfs_args_t *nap, mntent_t *mntp, int genflags, struct netconfig *nfsncp, struct sockaddr_in *ip_addr, u_long nfs_version, char *nfs_proto, am_nfs_handle_t *fhp, char *host_name, char *fs_name);
extern int create_amq_service(int *udp_soAMQp, SVCXPRT **udp_amqpp, struct netconfig **udp_amqncpp, int *tcp_soAMQp, SVCXPRT **tcp_amqpp, struct netconfig **tcp_amqncpp);
extern int create_nfs_service(int *soNFSp, u_short *nfs_portp, SVCXPRT **nfs_xprtp, void (*dispatch_fxn)(struct svc_req *rqstp, SVCXPRT *transp));
extern int get_knetconfig(struct knetconfig **kncpp, struct netconfig *in_ncp, char *nc_protoname);
extern struct netconfig *nfsncp;
extern void free_knetconfig(struct knetconfig *kncp);

#else /* not HAVE_TRANSPORT_TYPE_TLI */

extern void compute_nfs_args(nfs_args_t *nap, mntent_t *mntp, int genflags, struct sockaddr_in *ip_addr, u_long nfs_version, char *nfs_proto, am_nfs_handle_t *fhp, char *host_name, char *fs_name);
extern enum clnt_stat pmap_ping(struct sockaddr_in *address);
extern int create_amq_service(int *udp_soAMQp, SVCXPRT **udp_amqpp, int *tcp_soAMQp, SVCXPRT **tcp_amqpp);
extern int create_nfs_service(int *soNFSp, u_short *nfs_portp, SVCXPRT **nfs_xprtp, void (*dispatch_fxn)(struct svc_req *rqstp, SVCXPRT *transp));

#endif /* not HAVE_TRANSPORT_TYPE_TLI */

#ifndef HAVE_STRUCT_FHSTATUS_FHS_FH
# define fhs_fh  fhstatus_u.fhs_fhandle
#endif /* not HAVE_STRUCT_FHSTATUS_FHS_FH */


/**************************************************************************/
/*** Generic file-system types, implemented as part of the native O/S.	***/
/**************************************************************************/

/*
 * Loopback File System
 * Many systems can't support this, and in any case most of the
 * functionality is available with Symlink FS.
 */
#ifdef HAVE_FS_LOFS
extern am_ops lofs_ops;
#endif /* HAVE_FS_LOFS */

/*
 * CD-ROM File System (CD-ROM)
 * (HSFS: High Sierra F/S on some machines)
 * Many systems can't support this, and in any case most of the
 * functionality is available with program FS.
 */
#ifdef HAVE_FS_CDFS
extern am_ops cdfs_ops;
#endif /* HAVE_FS_CDFS */

/*
 * PC File System (MS-DOS)
 * Many systems can't support this, and in any case most of the
 * functionality is available with program FS.
 */
#ifdef HAVE_FS_PCFS
extern am_ops pcfs_ops;
#endif /* HAVE_FS_PCFS */

/*
 * Caching File System (Solaris)
 */
#ifdef HAVE_FS_CACHEFS
extern am_ops cachefs_ops;
#endif /* HAVE_FS_CACHEFS */

/*
 * Network File System
 * Good, slow, NFS V.2.
 */
#ifdef HAVE_FS_NFS
extern am_ops nfs_ops;		/* NFS */
extern fserver *find_nfs_srvr (mntfs *);
extern int nfs_fmount(mntfs *mf);
extern int nfs_fumount(mntfs *mf);
extern int nfs_init(mntfs *mf);
extern qelem nfs_srvr_list;
extern void nfs_umounted(am_node *mp);
#endif /* HAVE_FS_NFS */


/*
 * Network File System: the new generation
 * NFS V.3
 */
#ifdef HAVE_FS_NFS3
# ifndef NFS_VERSION3
#  define NFS_VERSION3 ((u_int) 3)
# endif /* not NFS_VERSION3 */
#endif /* HAVE_FS_NFS3 */

/*
 * Un*x File System
 * Normal local disk file system.
 */
#ifdef HAVE_FS_UFS
extern am_ops ufs_ops;		/* Un*x file system */
#endif /* HAVE_FS_UFS */


/**************************************************************************/
/*** Automounter file-system types, implemented by amd.			***/
/**************************************************************************/

/*
 * Automount File System
 */
#ifdef HAVE_AMU_FS_AUTO
extern am_ops amfs_auto_ops;	/* Automount file system (this!) */
extern am_ops amfs_toplvl_ops;	/* Top-level automount file system */
extern am_ops amfs_root_ops;	/* Root file system */
extern qelem amfs_auto_srvr_list;
extern am_node *amfs_auto_lookuppn(am_node *mp, char *fname, int *error_return, int op);
extern am_node *next_nonerror_node(am_node *xp);
extern char *amfs_auto_match(am_opts *fo);
extern fserver *find_amfs_auto_srvr(mntfs *);
extern int amfs_auto_readdir(am_node *mp, nfscookie cookie, nfsdirlist *dp, nfsentry *ep, int count);
extern int amfs_auto_umount(am_node *mp);
extern int amfs_auto_fmount(am_node *mp);
extern int amfs_auto_fumount(am_node *mp);
#endif /* HAVE_AMU_FS_AUTO */

/*
 * Toplvl Automount File System
 */
#ifdef HAVE_AMU_FS_TOPLVL
extern am_ops amfs_toplvl_ops;	/* Toplvl Automount file system */
extern int amfs_toplvl_mount(am_node *mp);
extern int amfs_toplvl_umount(am_node *mp);
extern void amfs_toplvl_mounted(mntfs *mf);
#endif /* HAVE_AMU_FS_TOPLVL */

/*
 * Direct Automount File System
 */
#ifdef HAVE_AMU_FS_DIRECT
extern am_ops amfs_direct_ops;	/* Direct Automount file system (this too) */
#endif /* HAVE_AMU_FS_DIRECT */

/*
 * Error File System
 */
#ifdef HAVE_AMU_FS_ERROR
extern am_ops amfs_error_ops;	/* Error file system */
extern am_node *amfs_error_lookuppn(am_node *mp, char *fname, int *error_return, int op);
extern int amfs_error_readdir(am_node *mp, nfscookie cookie, nfsdirlist *dp, nfsentry *ep, int count);
#endif /* HAVE_AMU_FS_ERROR */

/*
 * Inheritance File System
 */
#ifdef HAVE_AMU_FS_INHERIT
extern am_ops amfs_inherit_ops;	/* Inheritance file system */
#endif /* HAVE_AMU_FS_INHERIT */

/*
 * NFS mounts with local existence check.
 */
#ifdef HAVE_AMU_FS_NFSL
extern am_ops amfs_nfsl_ops;	/* NFSL */
#endif /* HAVE_AMU_FS_NFSL */

/*
 * Multi-nfs mounts.
 */
#ifdef HAVE_AMU_FS_NFSX
extern am_ops amfs_nfsx_ops;	/* NFSX */
#endif /* HAVE_AMU_FS_NFSX */

/*
 * NFS host - a whole tree.
 */
#ifdef HAVE_AMU_FS_HOST
extern am_ops amfs_host_ops;	/* NFS host */
#endif /* HAVE_AMU_FS_HOST */

/*
 * Program File System
 * This is useful for things like RVD.
 */
#ifdef HAVE_AMU_FS_PROGRAM
extern am_ops amfs_program_ops;	/* Program File System */
#endif /* HAVE_AMU_FS_PROGRAM */

/*
 * Symbolic-link file system.
 * A "filesystem" which is just a symbol link.
 */
#ifdef HAVE_AMU_FS_LINK
extern am_ops amfs_link_ops;	/* Symlink FS */
extern int amfs_link_fmount(mntfs *mf);
#endif /* HAVE_AMU_FS_LINK */

/*
 * Symbolic-link file system, which also checks that the target of
 * the symlink exists.
 * A "filesystem" which is just a symbol link.
 */
#ifdef HAVE_AMU_FS_LINKX
extern am_ops amfs_linkx_ops;	/* Symlink FS with existence check */
#endif /* HAVE_AMU_FS_LINKX */

/*
 * Union file system
 */
#ifdef HAVE_AMU_FS_UNION
extern am_ops amfs_union_ops;	/* Union FS */
#endif /* HAVE_AMU_FS_UNION */


/**************************************************************************/
/*** DEBUGGING								***/
/**************************************************************************/

/*
 * DEBUGGING:
 */
#ifdef DEBUG

# define	D_ALL		(~0)
# define	D_DAEMON	0x0001	/* Enter daemon mode */
# define	D_TRACE		0x0002	/* Do protocol trace */
# define	D_FULL		0x0004	/* Do full trace */
# define	D_MTAB		0x0008	/* Use local mtab */
# define	D_AMQ		0x0010	/* Register amq program */
# define	D_STR		0x0020	/* Debug string munging */
#  ifdef DEBUG_MEM
# define	D_MEM		0x0040	/* Trace memory allocations */
#  endif /* DEBUG_MEM */
# define	D_FORK		0x0080	/* Fork server */
		/* info service specific debugging (hesiod, nis, etc) */
# define	D_INFO		0x0100
# define	D_HRTIME	0x0200	/* Print high resolution time stamps */
# define	D_XDRTRACE	0x0400	/* Trace xdr routines */
# define	D_READDIR	0x0800	/* show browsable_dir progress */

/*
 * Normally, don't enter daemon mode, don't register amq, and don't trace xdr
 */
#  ifdef DEBUG_MEM
# define	D_TEST	(~(D_DAEMON|D_MEM|D_STR|D_XDRTRACE))
#  else /* not DEBUG_MEM */
# define	D_TEST	(~(D_DAEMON|D_STR|D_XDRTRACE))
#  endif /* not DEBUG_MEM */

# define	amuDebug(x)	if (debug_flags & (x))
# define	dlog		amuDebug(D_FULL) dplog
# define	amuDebugNo(x)	if (!(debug_flags & (x)))

/* debugging mount-table file to use */
# ifndef DEBUG_MNTTAB
#  define	DEBUG_MNTTAB	"./mnttab"
# endif /* not DEBUG_MNTTAB */

# ifdef DEBUG_MEM
/*
 * If debugging memory, then call a special freeing function that logs
 * more info, and resets the pointer to NULL so it cannot be used again.
 */
#  define	XFREE(x) dxfree(__FILE__,__LINE__,x)
extern void dxfree(char *file, int line, voidp ptr);
extern void malloc_verify(void);
# else /* not DEBUG_MEM */
/*
 * If regular debugging, then free the pointer and reset to NULL.
 * This should remain so for as long as am-utils is in alpha/beta testing.
 */
#  define	XFREE(x) do { free((voidp)x); x = NULL;} while (0)
# endif /* not DEBUG_MEM */

/* functions that depend solely on debugging */
extern void print_nfs_args(const nfs_args_t *nap, u_long nfs_version);
extern int debug_option (char *opt);

#else /* not DEBUG */

/*
 * if not debugging, then simple perform free, and don't bother
 * resetting the pointer.
 */
#  define	XFREE(x) free(x)

#define		amuDebug(x)	if (0)
#define		dlog		if (0) dplog
#define		amuDebugNo(x)	if (0)

#define		print_nfs_args(nap, nfs_version)
#define		debug_option(x)	(1)

#endif /* not DEBUG */

extern int debug_flags;		/* Debug options */
extern struct opt_tab dbg_opt[];
extern void dplog(const char *fmt, ...)
     __attribute__ ((__format__ (__printf__, 1, 2)));

/**************************************************************************/
/*** MISC (stuff left to autoconfiscate)				***/
/**************************************************************************/

#endif /* not _AM_UTILS_H */
