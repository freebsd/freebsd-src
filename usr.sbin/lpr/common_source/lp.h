/*
 * Copyright (c) 1983, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
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
 * 	From: @(#)lp.h	8.2 (Berkeley) 4/28/95
 *	$Id$
 */

#include <sys/queue.h>

/*
 * All this information used to be in global static variables shared
 * mysteriously by various parts of the lpr/lpd suite.
 * This structure attempts to centralize all these declarations in the
 * hope that they can later be made more dynamic.
 */
enum	lpd_filters { LPF_CIFPLOT, LPF_DVI, LPF_GRAPH, LPF_INPUT,
		      LPF_DITROFF, LPF_OUTPUT, LPF_FORTRAN, LPF_TROFF,
		      LPF_RASTER, LPF_COUNT };
/* NB: there is a table in common.c giving the mapping from capability names */

struct	printer {
	char	*printer;	/* printer name */
	int	 remote;	/* true if RM points to a remote host */
	int	 tof;		/* true if we are at top-of-form */
	/* ------------------------------------------------------ */
	char	*acct_file;	/* AF: accounting file */
	long	 baud_rate;	/* BR: baud rate if lp is a tty */
	char	*filters[LPF_COUNT]; /* CF, DF, GF, IF, NF, OF, RF, TF, VF */
	long	 conn_timeout;	/* CT: TCP connection timeout */
	long	 daemon_user;	/* DU: daemon user id -- XXX belongs ???? */
	char	*form_feed;	/* FF: form feed */
	long	 header_last;	/* HL: print header last */
	char	*log_file;	/* LF: log file */
	char	*lock_file;	/* LO: lock file */
	char	*lp;		/* LP: device name or network address */
	long	 max_copies;	/* MC: maximum number of copies allowed */
	long	 max_blocks;	/* MX: maximum number of blocks to copy */
	long	 price100;	/* PC: price per 100 units of output */
	long	 page_length;	/* PL: page length */
	long	 page_width;	/* PW: page width */
	long	 page_pwidth;	/* PX: page width in pixels */
	long	 page_plength;	/* PY: page length in pixels */
	char	*restrict_grp;	/* RG: restricted group */
	char	*remote_host;	/* RM: remote machine name */
	char	*remote_queue;	/* RP: remote printer name */
	long	 restricted;	/* RS: restricted to those with local accts */
	long	 rw;		/* RW: open LP for reading and writing */
	long	 short_banner;	/* SB: short banner */
	long	 no_copies;	/* SC: suppress multiple copies */
	char	*spool_dir;	/* SD: spool directory */
	long	 no_formfeed;	/* SF: suppress FF on each print job */
	long	 no_header;	/* SH: suppress header page */
	char	*status_file;	/* ST: status file name */
	char	*trailer;	/* TR: trailer string send when Q empties */
	char	*mode_set;	/* MS: mode set, a la stty */
};

/*
 * Lists of user names and job numbers, for the benefit of the structs
 * defined below.  We use TAILQs so that requests don't get mysteriously
 * reversed in process.
 */
struct	req_user {
	TAILQ_ENTRY(req_user)	ru_link; /* macro glue */
	char	ru_uname[1];	/* name of user */
};
TAILQ_HEAD(req_user_head, req_user);

struct	req_file {
	TAILQ_ENTRY(req_file)	rf_link; /* macro glue */
	char	 rf_type;	/* type (lowercase cf file letter) of file */
	char	*rf_prettyname;	/* user-visible name of file */
	char	 rf_fname[1];	/* name of file */
};
TAILQ_HEAD(req_file_head, req_file);

struct	req_jobid {
	TAILQ_ENTRY(req_jobid)	rj_link; /* macro glue */
	int	rj_job;		/* job number */
};
TAILQ_HEAD(req_jobid_head, req_jobid);

/*
 * Encapsulate all the information relevant to a request in the
 * lpr/lpd protocol.
 */
enum	req_type { REQ_START, REQ_RECVJOB, REQ_LIST, REQ_DELETE };

struct	request {
	enum	 req_type type;	/* what sort of request is this? */
	struct	 printer prtr;	/* which printer is it for? */
	int	 remote;	/* did request arrive over network? */
	char	*logname;	/* login name of requesting user */
	char	*authname;	/* authenticated identity of requesting user */
	char	*prettyname;	/* ``pretty'' name of requesting user */
	int	 privileged;	/* was the request from a privileged user? */
	void	*authinfo;	/* authentication information */
	int	 authentic;	/* was the request securely authenticated? */

	/* Information for queries and deletes... */
	int	 nusers;	/* length of following list... */
	struct	 req_user_head users; /* list of users to query/delete */
	int	 njobids;	/* length of following list... */
	struct	 req_jobid_head jobids;	/* list of jobids to query/delete */
};

/*
 * Global definitions for the line printer system.
 */
extern char	line[BUFSIZ];
extern char	*name;		/* program name */
				/* host machine name */
extern char	host[MAXHOSTNAMELEN];
extern char	*from;		/* client's machine name */

extern int	requ[];		/* job number of spool entries */
extern int	requests;	/* # of spool requests */
extern char	*user[];        /* users to process */
extern int	users;		/* # of users in user array */
extern char	*person;	/* name of person doing lprm */

/*
 * Structure used for building a sorted list of control files.
 */
struct queue {
	time_t	q_time;			/* modification time */
	char	q_name[MAXNAMLEN+1];	/* control file name */
};

/*
 * Error codes for our mini printcap library.
 */
#define	PCAPERR_TCLOOP		(-3)
#define	PCAPERR_OSERR		(-2)
#define	PCAPERR_NOTFOUND	(-1)
#define	PCAPERR_SUCCESS		0
#define	PCAPERR_TCOPEN		1

/*
 * File modes for the various status files maintained by lpd.
 */
#define	LOCK_FILE_MODE	(S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH)
#define	LFM_PRINT_DIS	(S_IXUSR)
#define	LFM_QUEUE_DIS	(S_IXGRP)
#define	LFM_RESET_QUE	(S_IXOTH)

#define	STAT_FILE_MODE	(S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH)
#define	LOG_FILE_MODE	(S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH)
#define	TEMP_FILE_MODE	(S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH)

/*
 * Command codes used in the protocol.
 */
#define	CMD_CHECK_QUE	'\1'
#define	CMD_TAKE_THIS	'\2'
#define	CMD_SHOWQ_SHORT	'\3'
#define	CMD_SHOWQ_LONG	'\4'
#define	CMD_RMJOB	'\5'

#include <sys/cdefs.h>

__BEGIN_DECLS
struct	 dirent;

void     blankfill __P((int));
char	*checkremote __P((struct printer *pp));
int      chk __P((char *));
void	 closeallfds __P((int start));
void     delay __P((int));
void     displayq __P((struct printer *pp, int format));
void     dump __P((char *, char *, int));
void	 fatal __P((const struct printer *pp, const char *fmp, ...));
int	 firstprinter __P((struct printer *pp, int *status));
void	 free_printer __P((struct printer *pp));
void	 free_request __P((struct request *rp));
int	 getline __P((FILE *));
int	 getport __P((const struct printer *pp, const char *, int));
int	 getprintcap __P((const char *printer, struct printer *pp));
int	 getq __P((const struct printer *, struct queue *(*[])));
void     header __P((void));
void     inform __P((const struct printer *pp, char *cf));
void	 init_printer __P((struct printer *pp));
void	 init_request __P((struct request *rp));
int      inlist __P((char *, char *));
int      iscf __P((struct dirent *));
int      isowner __P((char *, char *));
void     ldump __P((char *, char *, int));
void	 lastprinter __P((void));
int      lockchk __P((struct printer *pp, char *));
char	*lock_file_name __P((const struct printer *pp, char *buf, size_t len));
int	 nextprinter __P((struct printer *pp, int *status));
const
char	*pcaperr __P((int error));
void     prank __P((int));
void     process __P((const struct printer *pp, char *));
void     rmjob __P((const char *printer));
void     rmremote __P((const struct printer *pp));
void	 setprintcap __P((char *newprintcap));
void     show __P((char *, char *, int));
int      startdaemon __P((const struct printer *pp));
char	*status_file_name __P((const struct printer *pp, char *buf,
			       size_t len));
ssize_t	 writel __P((int s, ...));
__END_DECLS
