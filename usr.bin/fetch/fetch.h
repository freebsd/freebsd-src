/*
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
 *	$Id$
 */

#ifndef fetch_h
#define	fetch_h	1


#define BUFFER_SIZE 1024
#define	FETCH_VERSION "fetch/1.0"
#define PATH_CP "/bin/cp"

struct fetch_state {
	const char *fs_status;
	const char *fs_outputfile;
	int fs_verbose;		/* -q, -v option */
	int fs_newtime;		/* -n option */
	int fs_mirror;		/* -m option */
	int fs_restart;		/* -r option */
	int fs_timeout;		/* -T option */
	int fs_passive_mode;	/* -p option */
	int fs_linkfile;	/* -l option */
	int fs_precious;	/* -R option */
	time_t fs_modtime;
	void *fs_proto;
	int (*fs_retrieve)(struct fetch_state *);
	int (*fs_close)(struct fetch_state *);
};

struct uri_scheme {
	const char *sc_name;	/* name of the scheme, <32 characters */
	int (*sc_parse)(struct fetch_state *, const char *);
				/* routine to parse a URI and build state */
	int (*sc_proxy_parse)(struct fetch_state *, const char *);
				/* same, but for proxy case */
	const char *sc_proxy_envar; /* envar used to determine proxy */
	const char *sc_proxy_by; /* list of protos which can proxy us */

	/* The rest is filled in dynamically... */
	int sc_can_proxy;
	struct uri_scheme *sc_proxyproto;
};

extern	struct uri_scheme file_scheme, ftp_scheme, http_scheme;

void	adjmodtime(struct fetch_state *fs);
void	catchsig(int signo);
void	display(struct fetch_state *fs, off_t total, ssize_t thisincr);
void	init_schemes(void);
void	rm(struct fetch_state *fs);
void	setup_sigalrm(void);
void	unsetup_sigalrm(void);
char	*percent_decode(const char *orig);
char	*safe_strdup(const char *orig);
char	*safe_strndup(const char *orig, size_t len);
char	*to_base64(const unsigned char *buf, size_t len);
int	from_base64(const char *orig, unsigned char *buf, size_t *lenp);
int	parse_host_port(const char *str, char **hostname, int *port);
int	parse_uri(struct fetch_state *fs, const char *uri);
#endif /* ! fetch_h */
