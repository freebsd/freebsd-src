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
 *	$Id$
 */

#include <sys/types.h>

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#include "fetch.h"

struct uri_scheme *schemes[] = {
	&http_scheme, &ftp_scheme, &file_scheme, 0
};

static struct uri_scheme *
find_scheme(const char *name)
{
	int i;

	for (i = 0; schemes[i]; i++) {
		if (strcasecmp(schemes[i]->sc_name, name) == 0)
			return schemes[i];
	}
	return 0;
}

void
init_schemes(void)
{
	int i;
	char schemebuf[32];
	const char *s, *t;
	struct uri_scheme *scp;

	for (i = 0; schemes[i]; i++) {
		if (getenv(schemes[i]->sc_proxy_envar) != 0)
			schemes[i]->sc_can_proxy = 1;
	}

	for (i = 0; schemes[i]; i++) {
		s = schemes[i]->sc_proxy_by;
		while (s && *s) {
			t = strchr(s, ',');
			if (t) {
				schemebuf[0] = '\0';
				strncat(schemebuf, s, t - s);
				s = t + 1;
			} else {
				strcpy(schemebuf, s);
				s = 0;
			}
			scp = find_scheme(schemebuf);
			if (scp && scp->sc_can_proxy) {
				schemes[i]->sc_proxyproto = scp;
				break;
			}
		}
	}
}

int
parse_uri(struct fetch_state *fs, const char *uri)
{
	const char *colon, *slash;
	char *scheme;
	struct uri_scheme *scp;

	fs->fs_status = "parsing URI";
	colon = strchr(uri, ':');
	slash = strchr(uri, '/');
	if (!colon || !slash || slash < colon) {
		warnx("%s: an absolute URI is required", uri);
		return EX_USAGE;
	}

	scheme = alloca(colon - uri + 1);
	scheme[0] = '\0';
	strncat(scheme, uri, colon - uri);
	scp = find_scheme(scheme);

	if (scp == 0) {
		warnx("%s: unknown URI scheme", scheme);
		return EX_USAGE;
	}
	if (scp->sc_proxyproto)
		return scp->sc_proxyproto->sc_proxy_parse(fs, uri);
	else
		return scp->sc_parse(fs, uri);
}
	
