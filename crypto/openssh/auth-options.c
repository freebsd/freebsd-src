/*
 * Author: Tatu Ylonen <ylo@cs.hut.fi>
 * Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 *                    All rights reserved
 * As far as I am concerned, the code I have written for this software
 * can be used freely for any purpose.  Any derived versions of this
 * software must be clearly marked as such, and if the derived work is
 * incompatible with the protocol description in the RFC file, it must be
 * called by a name other than "ssh" or "Secure Shell".
 */

#include "includes.h"
RCSID("$OpenBSD: auth-options.c,v 1.16 2001/03/18 12:07:52 markus Exp $");

#include "packet.h"
#include "xmalloc.h"
#include "match.h"
#include "log.h"
#include "canohost.h"
#include "channels.h"
#include "auth-options.h"
#include "servconf.h"

/* Flags set authorized_keys flags */
int no_port_forwarding_flag = 0;
int no_agent_forwarding_flag = 0;
int no_x11_forwarding_flag = 0;
int no_pty_flag = 0;

/* "command=" option. */
char *forced_command = NULL;

/* "environment=" options. */
struct envstring *custom_environment = NULL;

extern ServerOptions options;

void
auth_clear_options(void)
{
	no_agent_forwarding_flag = 0;
	no_port_forwarding_flag = 0;
	no_pty_flag = 0;
	no_x11_forwarding_flag = 0;
	while (custom_environment) {
		struct envstring *ce = custom_environment;
		custom_environment = ce->next;
		xfree(ce->s);
		xfree(ce);
	}
	if (forced_command) {
		xfree(forced_command);
		forced_command = NULL;
	}
	channel_clear_permitted_opens();
}

/*
 * return 1 if access is granted, 0 if not.
 * side effect: sets key option flags
 */
int
auth_parse_options(struct passwd *pw, char *opts, char *file, u_long linenum)
{
	const char *cp;
	int i;

	/* reset options */
	auth_clear_options();

	if (!opts)
		return 1;

	while (*opts && *opts != ' ' && *opts != '\t') {
		cp = "no-port-forwarding";
		if (strncasecmp(opts, cp, strlen(cp)) == 0) {
			packet_send_debug("Port forwarding disabled.");
			no_port_forwarding_flag = 1;
			opts += strlen(cp);
			goto next_option;
		}
		cp = "no-agent-forwarding";
		if (strncasecmp(opts, cp, strlen(cp)) == 0) {
			packet_send_debug("Agent forwarding disabled.");
			no_agent_forwarding_flag = 1;
			opts += strlen(cp);
			goto next_option;
		}
		cp = "no-X11-forwarding";
		if (strncasecmp(opts, cp, strlen(cp)) == 0) {
			packet_send_debug("X11 forwarding disabled.");
			no_x11_forwarding_flag = 1;
			opts += strlen(cp);
			goto next_option;
		}
		cp = "no-pty";
		if (strncasecmp(opts, cp, strlen(cp)) == 0) {
			packet_send_debug("Pty allocation disabled.");
			no_pty_flag = 1;
			opts += strlen(cp);
			goto next_option;
		}
		cp = "command=\"";
		if (strncasecmp(opts, cp, strlen(cp)) == 0) {
			opts += strlen(cp);
			forced_command = xmalloc(strlen(opts) + 1);
			i = 0;
			while (*opts) {
				if (*opts == '"')
					break;
				if (*opts == '\\' && opts[1] == '"') {
					opts += 2;
					forced_command[i++] = '"';
					continue;
				}
				forced_command[i++] = *opts++;
			}
			if (!*opts) {
				debug("%.100s, line %lu: missing end quote",
				    file, linenum);
				packet_send_debug("%.100s, line %lu: missing end quote",
				    file, linenum);
				xfree(forced_command);
				forced_command = NULL;
				goto bad_option;
			}
			forced_command[i] = 0;
			packet_send_debug("Forced command: %.900s", forced_command);
			opts++;
			goto next_option;
		}
		cp = "environment=\"";
		if (strncasecmp(opts, cp, strlen(cp)) == 0) {
			char *s;
			struct envstring *new_envstring;

			opts += strlen(cp);
			s = xmalloc(strlen(opts) + 1);
			i = 0;
			while (*opts) {
				if (*opts == '"')
					break;
				if (*opts == '\\' && opts[1] == '"') {
					opts += 2;
					s[i++] = '"';
					continue;
				}
				s[i++] = *opts++;
			}
			if (!*opts) {
				debug("%.100s, line %lu: missing end quote",
				    file, linenum);
				packet_send_debug("%.100s, line %lu: missing end quote",
				    file, linenum);
				xfree(s);
				goto bad_option;
			}
			s[i] = 0;
			packet_send_debug("Adding to environment: %.900s", s);
			debug("Adding to environment: %.900s", s);
			opts++;
			new_envstring = xmalloc(sizeof(struct envstring));
			new_envstring->s = s;
			new_envstring->next = custom_environment;
			custom_environment = new_envstring;
			goto next_option;
		}
		cp = "from=\"";
		if (strncasecmp(opts, cp, strlen(cp)) == 0) {
			int mname, mip;
			const char *remote_ip = get_remote_ipaddr();
			const char *remote_host = get_canonical_hostname(
			    options.reverse_mapping_check);
			char *patterns = xmalloc(strlen(opts) + 1);

			opts += strlen(cp);
			i = 0;
			while (*opts) {
				if (*opts == '"')
					break;
				if (*opts == '\\' && opts[1] == '"') {
					opts += 2;
					patterns[i++] = '"';
					continue;
				}
				patterns[i++] = *opts++;
			}
			if (!*opts) {
				debug("%.100s, line %lu: missing end quote",
				    file, linenum);
				packet_send_debug("%.100s, line %lu: missing end quote",
				    file, linenum);
				xfree(patterns);
				goto bad_option;
			}
			patterns[i] = 0;
			opts++;
			/*
			 * Deny access if we get a negative
			 * match for the hostname or the ip
			 * or if we get not match at all
			 */
			mname = match_hostname(remote_host, patterns,
			    strlen(patterns));
			mip = match_hostname(remote_ip, patterns,
			    strlen(patterns));
			xfree(patterns);
			if (mname == -1 || mip == -1 ||
			    (mname != 1 && mip != 1)) {
				log("Authentication tried for %.100s with "
				    "correct key but not from a permitted "
				    "host (host=%.200s, ip=%.200s).",
				    pw->pw_name, remote_host, remote_ip);
				packet_send_debug("Your host '%.200s' is not "
				    "permitted to use this key for login.",
				    remote_host);
				/* deny access */
				return 0;
			}
			/* Host name matches. */
			goto next_option;
		}
		cp = "permitopen=\"";
		if (strncasecmp(opts, cp, strlen(cp)) == 0) {
			u_short port;
			char *c, *ep;
			char *patterns = xmalloc(strlen(opts) + 1);

			opts += strlen(cp);
			i = 0;
			while (*opts) {
				if (*opts == '"')
					break;
				if (*opts == '\\' && opts[1] == '"') {
					opts += 2;
					patterns[i++] = '"';
					continue;
				}
				patterns[i++] = *opts++;
			}
			if (!*opts) {
				debug("%.100s, line %lu: missing end quote",
				    file, linenum);
				packet_send_debug("%.100s, line %lu: missing end quote",
				    file, linenum);
				xfree(patterns);
				goto bad_option;
			}
			patterns[i] = 0;
			opts++;
			c = strchr(patterns, ':');
			if (c == NULL) {
				debug("%.100s, line %lu: permitopen: missing colon <%.100s>",
				    file, linenum, patterns);
				packet_send_debug("%.100s, line %lu: missing colon",
				    file, linenum);
				xfree(patterns);
				goto bad_option;
			}
			*c = 0;
			c++;
			port = strtol(c, &ep, 0);
			if (c == ep) {
				debug("%.100s, line %lu: permitopen: missing port <%.100s>",
				    file, linenum, patterns);
				packet_send_debug("%.100s, line %lu: missing port",
				    file, linenum);
				xfree(patterns);
				goto bad_option;
			}
			if (options.allow_tcp_forwarding)
				channel_add_permitted_opens(patterns, port);
			xfree(patterns);
			goto next_option;
		}
next_option:
		/*
		 * Skip the comma, and move to the next option
		 * (or break out if there are no more).
		 */
		if (!*opts)
			fatal("Bugs in auth-options.c option processing.");
		if (*opts == ' ' || *opts == '\t')
			break;		/* End of options. */
		if (*opts != ',')
			goto bad_option;
		opts++;
		/* Process the next option. */
	}
	/* grant access */
	return 1;

bad_option:
	log("Bad options in %.100s file, line %lu: %.50s",
	    file, linenum, opts);
	packet_send_debug("Bad options in %.100s file, line %lu: %.50s",
	    file, linenum, opts);
	/* deny access */
	return 0;
}
