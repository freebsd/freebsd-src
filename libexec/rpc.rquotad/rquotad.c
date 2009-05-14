/*
 * by Manuel Bouyer (bouyer@ensta.fr)
 *
 * There is no copyright, you can use it as you want.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/mount.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/socket.h>

#include <ufs/ufs/quota.h>
#include <rpc/rpc.h>
#include <rpcsvc/rquota.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <ctype.h>
#include <errno.h>
#include <fstab.h>
#include <grp.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

void rquota_service(struct svc_req *request, SVCXPRT *transp);
void sendquota(struct svc_req *request, SVCXPRT *transp);
void printerr_reply(SVCXPRT *transp);
void initfs(void);
int getfsquota(long id, char *path, struct dqblk *dqblk);
int hasquota(struct fstab *fs, char **qfnamep);

/*
 * structure containing informations about ufs filesystems
 * initialised by initfs()
 */
struct fs_stat {
	struct fs_stat *fs_next;	/* next element */
	char   *fs_file;		/* mount point of the filesystem */
	char   *qfpathname;		/* pathname of the quota file */
	dev_t   st_dev;			/* device of the filesystem */
} fs_stat;
struct fs_stat *fs_begin = NULL;

int from_inetd = 1;

static void
cleanup(int sig)
{

	(void) sig;
	(void) rpcb_unset(RQUOTAPROG, RQUOTAVERS, NULL);
	exit(0);
}

int
main(void)
{
	SVCXPRT *transp;
	int ok;
	struct sockaddr_storage from;
	socklen_t fromlen;

	fromlen = sizeof(from);
	if (getsockname(0, (struct sockaddr *)&from, &fromlen) < 0) {
		from_inetd = 0;
	}

	if (!from_inetd) {
		daemon(0, 0);

		(void) rpcb_unset(RQUOTAPROG, RQUOTAVERS, NULL);

		(void) signal(SIGINT, cleanup);
		(void) signal(SIGTERM, cleanup);
		(void) signal(SIGHUP, cleanup);
	}

	openlog("rpc.rquotad", LOG_CONS|LOG_PID, LOG_DAEMON);

	/* create and register the service */
	if (from_inetd) {
		transp = svc_tli_create(0, NULL, NULL, 0, 0);
		if (transp == NULL) {
			syslog(LOG_ERR, "couldn't create udp service.");
			exit(1);
		}
		ok = svc_reg(transp, RQUOTAPROG, RQUOTAVERS,
			     rquota_service, NULL);
	} else
		ok = svc_create(rquota_service,
				RQUOTAPROG, RQUOTAVERS, "udp");
	if (!ok) {
		syslog(LOG_ERR,
		    "unable to register (RQUOTAPROG, RQUOTAVERS, %s)",
		    from_inetd ? "(inetd)" : "udp");
		exit(1);
	}

	initfs();
	svc_run();
	syslog(LOG_ERR, "svc_run returned");
	exit(1);
}

void
rquota_service(struct svc_req *request, SVCXPRT *transp)
{

	switch (request->rq_proc) {
	case NULLPROC:
		(void)svc_sendreply(transp, (xdrproc_t)xdr_void, (char *)NULL);
		break;

	case RQUOTAPROC_GETQUOTA:
	case RQUOTAPROC_GETACTIVEQUOTA:
		sendquota(request, transp);
		break;

	default:
		svcerr_noproc(transp);
		break;
	}
	if (from_inetd)
		exit(0);
}

/* read quota for the specified id, and send it */
void
sendquota(struct svc_req *request, SVCXPRT *transp)
{
	struct getquota_args getq_args;
	struct getquota_rslt getq_rslt;
	struct dqblk dqblk;
	struct timeval timev;

	bzero((char *)&getq_args, sizeof(getq_args));
	if (!svc_getargs(transp, (xdrproc_t)xdr_getquota_args, &getq_args)) {
		svcerr_decode(transp);
		return;
	}
	if (request->rq_cred.oa_flavor != AUTH_UNIX) {
		/* bad auth */
		getq_rslt.status = Q_EPERM;
	} else if (!getfsquota(getq_args.gqa_uid, getq_args.gqa_pathp, &dqblk)) {
		/* failed, return noquota */
		getq_rslt.status = Q_NOQUOTA;
	} else {
		gettimeofday(&timev, NULL);
		getq_rslt.status = Q_OK;
		getq_rslt.getquota_rslt_u.gqr_rquota.rq_active = TRUE;
		getq_rslt.getquota_rslt_u.gqr_rquota.rq_bsize = DEV_BSIZE;
		getq_rslt.getquota_rslt_u.gqr_rquota.rq_bhardlimit =
		    dqblk.dqb_bhardlimit;
		getq_rslt.getquota_rslt_u.gqr_rquota.rq_bsoftlimit =
		    dqblk.dqb_bsoftlimit;
		getq_rslt.getquota_rslt_u.gqr_rquota.rq_curblocks =
		    dqblk.dqb_curblocks;
		getq_rslt.getquota_rslt_u.gqr_rquota.rq_fhardlimit =
		    dqblk.dqb_ihardlimit;
		getq_rslt.getquota_rslt_u.gqr_rquota.rq_fsoftlimit =
		    dqblk.dqb_isoftlimit;
		getq_rslt.getquota_rslt_u.gqr_rquota.rq_curfiles =
		    dqblk.dqb_curinodes;
		getq_rslt.getquota_rslt_u.gqr_rquota.rq_btimeleft =
		    dqblk.dqb_btime - timev.tv_sec;
		getq_rslt.getquota_rslt_u.gqr_rquota.rq_ftimeleft =
		    dqblk.dqb_itime - timev.tv_sec;
	}
	if (!svc_sendreply(transp, (xdrproc_t)xdr_getquota_rslt, &getq_rslt)) {
		svcerr_systemerr(transp);
	}
	if (!svc_freeargs(transp, (xdrproc_t)xdr_getquota_args, &getq_args)) {
		syslog(LOG_ERR, "unable to free arguments");
		exit(1);
	}
}

void
printerr_reply(SVCXPRT *transp)	/* when a reply to a request failed */
{
	char name[INET6_ADDRSTRLEN];
	struct sockaddr *caller;
	int     save_errno;

	save_errno = errno;

	caller = (struct sockaddr *)svc_getrpccaller(transp)->buf;
	getnameinfo(caller, caller->sa_len, name, sizeof (name),
		    NULL, 0, NI_NUMERICHOST);
	errno = save_errno;
	if (errno == 0)
		syslog(LOG_ERR, "couldn't send reply to %s", name);
	else
		syslog(LOG_ERR, "couldn't send reply to %s: %m", name);
}

/* initialise the fs_tab list from entries in /etc/fstab */
void
initfs(void)
{
	struct fs_stat *fs_current = NULL;
	struct fs_stat *fs_next = NULL;
	char *qfpathname;
	struct fstab *fs;
	struct stat st;

	setfsent();
	while ((fs = getfsent())) {
		if (strcmp(fs->fs_vfstype, "ufs"))
			continue;
		if (!hasquota(fs, &qfpathname))
			continue;

		fs_current = (struct fs_stat *) malloc(sizeof(struct fs_stat));
		fs_current->fs_next = fs_next;	/* next element */

		fs_current->fs_file =
		    malloc(sizeof(char) * (strlen(fs->fs_file) + 1));
		strcpy(fs_current->fs_file, fs->fs_file);

		fs_current->qfpathname =
		    malloc(sizeof(char) * (strlen(qfpathname) + 1));
		strcpy(fs_current->qfpathname, qfpathname);

		stat(fs_current->fs_file, &st);
		fs_current->st_dev = st.st_dev;

		fs_next = fs_current;
	}
	endfsent();
	fs_begin = fs_current;
}

/*
 * gets the quotas for id, filesystem path.
 * Return 0 if fail, 1 otherwise
 */
int
getfsquota(long id, char *path, struct dqblk *dqblk)
{
	struct stat st_path;
	struct fs_stat *fs;
	int	qcmd, fd, ret = 0;

	if (stat(path, &st_path) < 0)
		return (0);

	qcmd = QCMD(Q_GETQUOTA, USRQUOTA);

	for (fs = fs_begin; fs != NULL; fs = fs->fs_next) {
		/* where the devise is the same as path */
		if (fs->st_dev != st_path.st_dev)
			continue;

		/* find the specified filesystem. get and return quota */
		if (quotactl(fs->fs_file, qcmd, id, dqblk) == 0)
			return (1);

		if ((fd = open(fs->qfpathname, O_RDONLY)) < 0) {
			syslog(LOG_ERR, "open error: %s: %m", fs->qfpathname);
			return (0);
		}
		if (lseek(fd, (off_t)(id * sizeof(struct dqblk)), L_SET) == (off_t)-1) {
			close(fd);
			return (1);
		}
		switch (read(fd, dqblk, sizeof(struct dqblk))) {
		case 0:
			/*
                         * Convert implicit 0 quota (EOF)
                         * into an explicit one (zero'ed dqblk)
                         */
			bzero(dqblk, sizeof(struct dqblk));
			ret = 1;
			break;
		case sizeof(struct dqblk):	/* OK */
			ret = 1;
			break;
		default:	/* ERROR */
			syslog(LOG_ERR, "read error: %s: %m", fs->qfpathname);
			close(fd);
			return (0);
		}
		close(fd);
	}
	return (ret);
}

/*
 * Check to see if a particular quota is to be enabled.
 * Comes from quota.c, NetBSD 0.9
 */
int
hasquota(struct fstab *fs, char  **qfnamep)
{
	static char initname, usrname[100];
	static char buf[BUFSIZ];
	char *opt, *cp;
	const char *qfextension[] = INITQFNAMES;

	if (!initname) {
		sprintf(usrname, "%s%s", qfextension[USRQUOTA], QUOTAFILENAME);
		initname = 1;
	}
	strcpy(buf, fs->fs_mntops);
	for (opt = strtok(buf, ","); opt; opt = strtok(NULL, ",")) {
		if ((cp = index(opt, '=')))
			*cp++ = '\0';
		if (strcmp(opt, usrname) == 0)
			break;
	}
	if (!opt)
		return (0);
	if (cp) {
		*qfnamep = cp;
		return (1);
	}
	sprintf(buf, "%s/%s.%s", fs->fs_file, QUOTAFILENAME,
	    qfextension[USRQUOTA]);
	*qfnamep = buf;
	return (1);
}
