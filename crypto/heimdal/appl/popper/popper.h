/*
 * Copyright (c) 1989 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 * static char copyright[] = "Copyright (c) 1990 Regents of the University of California.\nAll rights reserved.\n";
 * static char SccsId[] = "@(#)@(#)popper.h	2.2  2.2 4/2/91";
 *
 */

/* $Id: popper.h,v 1.50 2001/09/10 11:56:34 joda Exp $ */

/* 
 *  Header file for the POP programs
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#define UIDL
#define XOVER
#define XDELE
#define DEBUG
#define RETURN_PATH_HANDLING
#endif

/* Common include files */

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <setjmp.h>
#include <ctype.h>
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_PWD_H
#include <pwd.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_IO_H
#include <io.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_SYS_FILE_H
#include <sys/file.h>
#endif
#ifdef TIME_WITH_SYS_TIME
#include <sys/time.h>
#include <time.h>
#elif defined(HAVE_SYS_TIME_H)
#include <sys/time.h>
#else
#include <time.h>
#endif
#ifdef HAVE_SYS_RESOURCE_H
#include <sys/resource.h>
#endif
#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_NETINET_IN6_H
#include <netinet/in6.h>
#endif
#ifdef HAVE_NETINET6_IN6_H
#include <netinet6/in6.h>
#endif

#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif
#ifdef HAVE_ARPA_INET_H
#ifdef _AIX
struct sockaddr_dl; /* AIX fun */
struct ether_addr;
#endif
#include <arpa/inet.h>
#endif
#ifdef HAVE_SYSLOG_H
#include <syslog.h>
#endif
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#include "version.h"

#ifdef SOCKS
#include <socks.h>
#endif

#include <err.h>
#include <roken.h>
#include <getarg.h>

#ifdef KRB4
#include <krb.h>
#include <prot.h>
#endif
#ifdef KRB5
#include <krb5.h>
#endif

#define MAXUSERNAMELEN  65
#define MAXLINELEN      1024
#define MAXMSGLINELEN   1024
#define MAXCMDLEN       4
#define MAXPARMCOUNT    10
#define MAXPARMLEN      10
#define ALLOC_MSGS  20
#define MAIL_COMMAND    "/usr/lib/sendmail"

#define POP_FACILITY    LOG_LOCAL0
#define POP_PRIORITY    LOG_NOTICE
#define POP_DEBUG       LOG_DEBUG
#define POP_INFO	LOG_INFO
#define POP_LOGOPTS     0

#ifdef HAVE_PATHS_H
#include <paths.h>
#endif
#ifdef HAVE_MAILLOCK_H
#include <maillock.h>
#endif

#ifdef OTP
#include <otp.h>
#endif

#if defined(KRB4_MAILDIR)
#define POP_MAILDIR	KRB4_MAILDIR
#elif defined(_PATH_MAILDIR)
#define POP_MAILDIR     _PATH_MAILDIR
#elif defined(MAILDIR)
#define POP_MAILDIR	MAILDIR
#else
#define POP_MAILDIR	"/usr/spool/mail"
#endif

#define POP_DROP        POP_MAILDIR "/.%s.pop"
	/* POP_TMPSIZE needs to be big enough to hold the string
	 * defined by POP_TMPDROP.  POP_DROP and POP_TMPDROP
	 * must be in the same filesystem.
	 */
#define POP_TMPDROP     POP_MAILDIR "/tmpXXXXXX"
#define POP_TMPSIZE	256
#define POP_TMPXMIT     "/tmp/xmitXXXXXX"
#define POP_OK          "+OK"
#define POP_ERR         "-ERR"
#define POP_SUCCESS     1
#define POP_FAILURE     0
#define POP_TERMINATE   '.'
#define POP_TIMEOUT     120     /* timeout connection after this many secs */

extern int              pop_timeout;

extern int              hangup;

#define AUTH_NONE 0
#define AUTH_OTP  1

#define pop_command         pop_parm[0]     /*  POP command is first token */
#define pop_subcommand      pop_parm[1]     /*  POP XTND subcommand is the 
                                                second token */

typedef enum {                              /*  POP processing states */
    auth1,                                  /*  Authorization: waiting for 
                                                USER command */
    auth2,                                  /*  Authorization: waiting for 
                                                PASS command */
    trans,                                  /*  Transaction */
    update,                                 /*  Update:  session ended, 
                                                process maildrop changes */
    halt,                                   /*  (Halt):  stop processing 
                                                and exit */
    error                                   /*  (Error): something really 
                                                bad happened */
} state;


#define DEL_FLAG	1
#define RETR_FLAG	2
#define NEW_FLAG	4

typedef struct {                                /*  Message information */
    int         number;                         /*  Message number relative to 
                                                    the beginning of list */
    long        length;                         /*  Length of message in 
                                                    bytes */
    int         lines;                          /*  Number of (null-terminated)                                                     lines in the message */
    long        offset;                         /*  Offset from beginning of 
                                                    file */
    unsigned	flags;

#if defined(UIDL) || defined(XOVER)
    char        *msg_id;	                /*  The POP UIDL uniqueifier */
#endif
#ifdef XOVER
    char	*subject;
    char	*from;
    char	*date;
#endif
    char	*name;
} MsgInfoList;

#define IS_MAILDIR(P) ((P)->temp_drop[0] == '\0')

typedef struct  {                               /*  POP parameter block */
    int                 debug;                  /*  Debugging requested */
    char            *   myname;                 /*  The name of this POP 
                                                    daemon program */
    char                myhost[MaxHostNameLen]; /*  The name of our host 
                                                    computer */
    char		client[MaxHostNameLen];	/*  Canonical name of client 
                                                    computer */
    char                ipaddr[MaxHostNameLen];	/*  Dotted-notation format of 
                                                    client IP address */
    unsigned short      ipport;                 /*  Client port for privileged 
                                                    operations */
    char                user[MAXUSERNAMELEN];   /*  Name of the POP user */
    state               CurrentState;           /*  The current POP operational                                                     state */
    MsgInfoList     *   mlp;                    /*  Message information list */
    int                 msg_count;              /*  Number of messages in 
                                                    the maildrop */
    int                 msgs_deleted;           /*  Number of messages flagged 
                                                    for deletion */
    int                 last_msg;               /*  Last message touched by 
                                                    the user */
    long                bytes_deleted;          /*  Number of maildrop bytes 
                                                    flagged for deletion */
    char                drop_name[MAXPATHLEN];  /*  The name of the user's 
                                                    maildrop */
    char                temp_drop[MAXPATHLEN];  /*  The name of the user's 
                                                    temporary maildrop */
    long                drop_size;              /*  Size of the maildrop in
                                                    bytes */
    FILE            *   drop;                   /*  (Temporary) mail drop */
    FILE            *   input;                  /*  Input TCP/IP communication 
                                                    stream */
    FILE            *   output;                 /*  Output TCP/IP communication                                                     stream */
    FILE            *   trace;                  /*  Debugging trace file */
    char            *   pop_parm[MAXPARMCOUNT]; /*  Parse POP parameter list */
    int                 parm_count;             /*  Number of parameters in 
                                                    parsed list */
    int			kerberosp;		/*  Using KPOP? */
#ifdef KRB4
    AUTH_DAT		kdata;
#endif
#ifdef KRB5
    krb5_context	context;
    krb5_principal	principal;              /*  principal auth as */
    krb5_log_facility*  logf;
#endif
    int			version;                /*  4 or 5? */
    int			auth_level;		/*  Dont allow cleartext */
#ifdef OTP
    OtpContext		otp_ctx;		/*  OTP context */
#endif
} POP;

typedef struct {                                /*  State information for 
                                                    each POP command */
    state       ValidCurrentState;              /*  The operating state of 
                                                    the command */
    char   *    command;                        /*  The POP command */
    int         min_parms;                      /*  Minimum number of parms 
                                                    for the command */
    int         max_parms;                      /*  Maximum number of parms 
                                                    for the command */
    int         (*function) ();                 /*  The function that process 
                                                    the command */
    state       result[2];                      /*  The resulting state after 
                                                    command processing */
#define success_state   result[0]               /*  State when a command 
                                                    succeeds */
} state_table;

typedef struct {                                /*  Table of extensions */
    char   *    subcommand;                     /*  The POP XTND subcommand */
    int         min_parms;                      /*  Minimum number of parms for
                                                    the subcommand */
    int         max_parms;                      /*  Maximum number of parms for
                                                    the subcommand */
    int         (*function) ();                 /*  The function that processes 
                                                    the subcommand */
} xtnd_table;

int pop_dele(POP *p);
int pop_dropcopy(POP *p, struct passwd *pwp);
int pop_dropinfo(POP *p);
int pop_init(POP *p,int argcount,char **argmessage);
int pop_last(POP *p);
int pop_list(POP *p);
int pop_parse(POP *p, char *buf);
int pop_pass(POP *p);
int pop_quit(POP *p);
int pop_rset(POP *p);
int pop_send(POP *p);
int pop_stat(POP *p);
int pop_updt(POP *p);
int pop_user(POP *p);
#ifdef UIDL
int pop_uidl(POP *p);
#endif
#ifdef XOVER
int pop_xover(POP *p);
#endif
#ifdef XDELE
int pop_xdele(POP *p);
#endif
int pop_help(POP *p);
state_table *pop_get_command(POP *p, char *mp);
void pop_lower(char *buf);

int pop_log(POP *p, int stat, char *format, ...)
#ifdef __GNUC__
__attribute__ ((format (printf, 3, 4)))
#endif
;

int pop_msg(POP *p, int stat, char *format, ...)
#ifdef __GNUC__
__attribute__ ((format (printf, 3, 4)))
#endif
;

int pop_maildir_info (POP*);
int pop_maildir_open (POP*, MsgInfoList*);
int pop_maildir_update (POP*);

int changeuser(POP*, struct passwd*);
void parse_header(MsgInfoList*, char*);
int add_missing_headers(POP*, MsgInfoList*);
