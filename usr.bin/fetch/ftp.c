/*-
 * Copyright 1997 Massachusetts Institute of Technology
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby
 * granted, provided that both the above copyright notice and this
 * permission notice appear in all copies, that both the above
 * copyright notice and this permission notice appear in all
 * supporting documentation, and that the name of M.I.T. not be used
 * in advertising or publicity pertaining to distribution of the
 * software without specific, written prior permission.  M.I.T. makes
 * no representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied
 * warranty.
 * 
 * THIS SOFTWARE IS PROVIDED BY M.I.T. ``AS IS''.  M.I.T. DISCLAIMS
 * ALL EXPRESS OR IMPLIED WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT
 * SHALL M.I.T. BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	$Id: ftp.c,v 1.6 1997/03/11 15:13:28 jmg Exp $
 */

#include <sys/types.h>

#include <err.h>
#include <errno.h>
#include <ftpio.h>
#include <limits.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#include <sys/param.h>
#include <sys/stat.h>

#include "fetch.h"

struct ftp_state {
	char *ftp_hostname;
	char *ftp_user;
	char *ftp_password;
	char *ftp_remote_file;
	unsigned ftp_port;
};

static int ftp_close(struct fetch_state *fs);
static int ftp_retrieve(struct fetch_state *fs);
static int ftp_parse(struct fetch_state *fs, const char *uri);
static int ftp_proxy_parse(struct fetch_state *fs, const char *uri);

struct uri_scheme ftp_scheme =
	{ "ftp", ftp_parse, ftp_proxy_parse, "FTP_PROXY", "ftp,http" };

static int 
ftp_parse(struct fetch_state *fs, const char *uri)
{
	const char *p, *slash, *q;
	char *hostname, *atsign, *colon;
	unsigned port;
	struct ftp_state *ftps;

	p = uri + 4;
	port = 0;

	if (p[0] != '/' || p[1] != '/') {
		warnx("`%s': invalid `ftp' URL", uri);
		return EX_USAGE;
	}

	p += 2;
	slash = strchr(p, '/');
	if (slash == 0) {
		warnx("`%s': malformed `ftp' URL", uri);
		return EX_USAGE;
	}
	hostname = alloca(slash - p + 1);
	hostname[0] = '\0';
	strncat(hostname, p, slash - p);

	if ((atsign = strrchr(hostname, '@')) == 0)
		q = hostname;
	else
		q = atsign + 1;

	if ((colon = strchr(q, ':')) != 0)
		*colon = '\0';

	if (colon && *(colon + 1)) {
		unsigned long ul;
		char *ep;

		errno = 0;
		ul = strtoul(colon + 1, &ep, 10);
		if (*ep || errno != 0 || ul < 1 || ul > 65534) {
			if (errno)
				warn("`%s': invalid port in URL", uri);
			else
				warnx("`%s': invalid port in URL", uri);
			return EX_USAGE;
		}

		port = ul;
	} else {
		port = 21;
	}

	p = slash + 1;

	ftps = safe_malloc(sizeof *ftps);
	ftps->ftp_password = 0;
	ftps->ftp_user = 0;

	/*
	 * Now, we have a copy of the hostname in hostname, the specified port
	 * (or the default value) in port, and p points to the filename part
	 * of the URI.  We just need to check for a user in the hostname,
	 * and then save all the bits in our state.
	 */
	if (atsign) {
		if (atsign[1] == '\0') {
			warnx("`%s': malformed `ftp' hostname", hostname);
			free(ftps);
			return EX_USAGE;
		}

		*atsign = '\0';
		if ((colon = strchr(hostname, ':')) != 0)
			*colon = '\0';
		if (hostname[0] == '\0') {
			warnx("`%s': malformed `ftp' user", atsign + 1);
			free(ftps);
			return EX_USAGE;
		}
		if (colon != 0)
			ftps->ftp_password = percent_decode(colon + 1);
		ftps->ftp_user = percent_decode(hostname);
		ftps->ftp_hostname = safe_strdup(atsign + 1);
	} else
		ftps->ftp_hostname = safe_strdup(hostname);
	ftps->ftp_port = port;

	p = ftps->ftp_remote_file = percent_decode(p);
	/* now p is the decoded version */

	if (fs->fs_outputfile == 0) {
		slash = strrchr(p, '/');
		fs->fs_outputfile = slash ? slash + 1 : p;
	}

	if (ftps->ftp_password == 0)
		ftps->ftp_password = getenv("FTP_PASSWORD");
	if (ftps->ftp_password != 0) {
		ftps->ftp_password = safe_strdup(ftps->ftp_password);
	} else {
		char *pw;
		const char *logname;
		char localhost[MAXHOSTNAMELEN];

		logname = getlogin();
		if (logname == 0)
			logname = "root";
		gethostname(localhost, sizeof localhost);
		pw = safe_malloc(strlen(logname) + 1 + strlen(localhost) + 1);
		strcpy(pw, logname);
		strcat(pw, "@");
		strcat(pw, localhost);
		ftps->ftp_password = pw;
		setenv("FTP_PASSWORD", pw, 0); /* cache the result */
	}

	if (ftps->ftp_user == 0)
		ftps->ftp_user = getenv("FTP_LOGIN");
	if (ftps->ftp_user != 0)
		ftps->ftp_user = safe_strdup(ftps->ftp_user);

	fs->fs_proto = ftps;
	fs->fs_close = ftp_close;
	fs->fs_retrieve = ftp_retrieve;
	return 0;
}

/*
 * The only URIs we can handle in the FTP proxy are FTP URLs.
 * This makes it possible to take a few short cuts.
 */
static int
ftp_proxy_parse(struct fetch_state *fs, const char *uri)
{
	int rv;
	char *hostname;
	char *port;
	const char *user;
	char *newuser;
	unsigned portno;
	struct ftp_state *ftps;

	hostname = getenv("FTP_PROXY");
	port = strchr(hostname, ':');
	if (port == 0) {
		portno = 21;
	} else {
		unsigned long ul;
		char *ep;

		/* All this to avoid modifying the environment. */
		ep = alloca(strlen(hostname) + 1);
		strcpy(ep, hostname);
		port = ep + (port - hostname);
		hostname = ep;

		*port++ = '\0';
		errno = 0;
		ul = strtoul(port, &ep, 0);
		if (*ep || !*port || errno != 0 || ul < 1 || ul > 65534) {
			warnx("`%s': invalid port specification for FTP proxy",
			      port);
			return EX_USAGE;
		}
		portno = ul;
	}

	/* ftp_parse() does most of the work; we can just fix things up */
	rv = ftp_parse(fs, uri);
	if (rv)
		return rv;
	/* Oops.. it got turned into a file: */
	if (fs->fs_retrieve != ftp_retrieve) {
		return 0;
	}

	ftps = fs->fs_proto;

	user = ftps->ftp_user ? ftps->ftp_user : "anonymous";
	/* user @ hostname [ @port ] \0 */
	newuser = safe_malloc(strlen(user) + 1 + strlen(ftps->ftp_hostname)
			      + ((ftps->ftp_port != 21) ? 6 : 0) + 1);

	strcpy(newuser, user);
	strcat(newuser, "@");
	strcat(newuser, ftps->ftp_hostname);
	if (ftps->ftp_port != 21) {
		char numbuf[6];

		snprintf(numbuf, sizeof(numbuf), "%d", ftps->ftp_port);
		numbuf[sizeof(numbuf)-1] = '\0';
		strcat(newuser, "@");
		strcat(newuser, numbuf);
	}
	
	ftps->ftp_port = portno;
	free(ftps->ftp_hostname);
	ftps->ftp_hostname = safe_strdup(hostname);
	free(ftps->ftp_user);
	ftps->ftp_user = newuser;
	return 0;
}

static int
ftp_close(struct fetch_state *fs)
{
	struct ftp_state *ftps = fs->fs_proto;

	if (ftps->ftp_user)
		free(ftps->ftp_user);
	free(ftps->ftp_hostname);
	free(ftps->ftp_password);
	free(ftps->ftp_remote_file);
	free(ftps);
	fs->fs_proto = 0;
	fs->fs_outputfile = 0;
	return 0;
}

static int
ftp_retrieve(struct fetch_state *fs)
{
	struct ftp_state *ftps = fs->fs_proto;
	FILE *ftp, *remote, *local;
	int status;
	off_t size;
	off_t seekloc, wehave;
	time_t modtime;
	size_t readresult, writeresult;

	ftp = ftpLogin(ftps->ftp_hostname, 
		       (char *)(ftps->ftp_user ? ftps->ftp_user : "anonymous"),
		       /* XXX ^^^^ bad API */
		       ftps->ftp_password, ftps->ftp_port, fs->fs_verbose > 1,
		       &status);
	if (ftp == 0) {
		warnx("%s: %s", ftps->ftp_hostname, 
		      status ? ftpErrString(status) : hstrerror(h_errno));
		return EX_IOERR;
	}
	ftpBinary(ftp);
	ftpPassive(ftp, fs->fs_passive_mode);
	size = ftpGetSize(ftp, ftps->ftp_remote_file);
	modtime = ftpGetModtime(ftp, ftps->ftp_remote_file);
	if (modtime <= 0) {	/* xxx */
		warnx("%s: cannot get remote modification time", 
		      ftps->ftp_remote_file);
		modtime = -1;
	}
	fs->fs_modtime = modtime;
	seekloc = wehave = 0;
	if (fs->fs_restart || fs->fs_mirror) {
		struct stat stab;
		
		if (fs->fs_outputfile[0] == '-' 
		    && fs->fs_outputfile[1] == '\0')
			status = fstat(STDOUT_FILENO, &stab);
		else
			status = stat(fs->fs_outputfile, &stab);
		if (status < 0) {
			stab.st_mtime = -1;
			stab.st_size = 0;
		}
		if (status == 0 && !S_ISREG(stab.st_mode)) {
			fs->fs_restart = 0;
			fs->fs_mirror = 0;
		}
		if (fs->fs_mirror && stab.st_size == size
		    && modtime <= stab.st_mtime) {
			fclose(ftp);
			return 0;
		}
		if (fs->fs_restart) {
			if (stab.st_size != 0 && stab.st_size < size)
				seekloc = wehave = stab.st_size;
		}
	}

	remote = ftpGet(ftp, ftps->ftp_remote_file, &seekloc);
	if (remote == 0) {
		if (ftpErrno(ftp)) {
			warnx("ftp://%s/%s: FTP error:",
				ftps->ftp_hostname, ftps->ftp_remote_file);
			warnx("%s", ftpErrString(ftpErrno(ftp)));
			fclose(ftp);
			return EX_IOERR;
		} else {
			warn("ftpGet");
			return EX_OSERR;
		}
	}

	if (fs->fs_outputfile[0] == '-' && fs->fs_outputfile[1] == '\0')
		local = fopen("/dev/stdout", wehave ? "a" : "w");
	else
		local = fopen(fs->fs_outputfile, wehave ? "a" : "w");
	if (local == 0) {
		warn("%s", fs->fs_outputfile);
		fclose(remote);
		fclose(ftp);
		return EX_OSERR;
	}

	if (fs->fs_timeout) {
		char buf[sizeof("18446744073709551616")]; /* 2**64 */
		snprintf(buf, sizeof buf, "%d", fs->fs_timeout);
		setenv("FTP_TIMEOUT", buf, 1);
	} else {
		char *env = getenv("FTP_TIMEOUT");
		char *ep;
		unsigned long ul;

		if (env) {
			errno = 0;
			ul = strtoul(env, &ep, 0);
			if (*env && *ep == '\0' && errno == 0 && ul <= INT_MAX)
				fs->fs_timeout = ul;
			else
				warnx("`%s': invalid FTP timeout", env);
		}
	}

	display(fs, size, wehave);
	setup_sigalrm();

	do {
		char buf[BUFFER_SIZE];

		alarm(fs->fs_timeout);
		readresult = fread(buf, 1, sizeof buf, remote);
		alarm(0);
		if (readresult == 0)
			break;
		display(fs, size, readresult);
		writeresult = fwrite(buf, 1, readresult, local);
	} while (writeresult == readresult);
	unsetup_sigalrm();

	if (ferror(remote)) {
		warn("reading remote file from %s", ftps->ftp_hostname);
		fclose(local);
		fclose(remote);
		fclose(ftp);
		rm(fs);
		return EX_IOERR;
	} else if(ferror(local)) {
		warn("%s", fs->fs_outputfile);
		fclose(local);
		fclose(remote);
		fclose(ftp);
		rm(fs);
		return EX_IOERR;
	}

	fclose(local);
	fclose(remote);
	fclose(ftp);
	display(fs, size, -1);
	adjmodtime(fs);
	return 0;
}
