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
RCSID("$OpenBSD: auth-options.c,v 1.26 2002/07/30 17:03:55 markus Exp $");

#include "xmalloc.h"
#include "match.h"
#include "log.h"
#include "canohost.h"
#include "channels.h"
#include "auth-options.h"
#include "servconf.h"
#include "misc.h"
#include "monitor_wrap.h"
#include "auth.h"

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
	auth_debug_reset();
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
			auth_debug_add("Port forwarding disabled.");
			no_port_forwarding_flag = 1;
			opts += strlen(cp);
			goto next_option;
		}
		cp = "no-agent-forwarding";
		if (strncasecmp(opts, cp, strlen(cp)) == 0) {
			auth_debug_add("Agent forwarding disabled.");
			no_agent_forwarding_flag = 1;
			opts += strlen(cp);
			goto next_option;
		}
		cp = "no-X11-forwarding";
		if (strncasecmp(opts, cp, strlen(cp)) == 0) {
			auth_debug_add("X11 forwarding disabled.");
			no_x11_forwarding_flag = 1;
			opts += strlen(cp);
			goto next_option;
		}
		cp = "no-pty";
		if (strncasecmp(opts, cp, strlen(cp)) == 0) {
			auth_debug_add("Pty allocation disabled.");
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
				auth_debug_add("%.100s, line %lu: missing end quote",
				    file, linenum);
				xfree(forced_command);
				forced_command = NULL;
				goto bad_option;
			}
			forced_command[i] = 0;
			auth_debug_add("Forced command: %.900s", forced_command);
			opts++;
			goto next_option;
		}
		cp = "environment=\"";
		if (options.permit_user_env &&
		    strncasecmp(opts, cp, strlen(cp)) == 0) {
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
				auth_debug_add("%.100s, line %lu: missing end quote",
				    file, linenum);
				xfree(s);
				goto bad_option;
			}
			s[i] = 0;
			auth_debug_add("Adding to environment: %.900s", s);
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
			const char *remote_ip = get_remote_ipaddr();
			const char *remote_host = get_canonical_hostname(
			    options.verify_reverse_mapping);
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
				auth_debug_add("%.100s, line %lu: missing end quote",
				    file, linenum);
				xfree(patterns);
				goto bad_option;
			}
			patterns[i] = 0;
			opts++;
			if (match_host_and_ip(remote_host, remote_ip,
			    patterns) != 1) {
				xfree(patterns);
				log("Authentication tried for %.100s with "
				    "correct key but not from a permitted "
				    "host (host=%.200s, ip=%.200s).",
				    pw->pw_name, remote_host, remote_ip);
				auth_debug_add("Your host '%.200s' is not "
				    "permitted to use this key for login.",
				    remote_host);
				/* deny access */
				return 0;
			}
			xfree(patterns);
			/* Host name matches. */
			goto next_option;
		}
		cp = "permitopen=\"";
		if (strncasecmp(opts, cp, strlen(cp)) == 0) {
			char host[256], sport[6];
			u_short port;
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
				auth_debug_add("%.100s, line %lu: missing end quote",
				    file, linenum);
				xfree(patterns);
				goto bad_option;
			}
			patterns[i] = 0;
			opts++;
			if (sscanf(patterns, "%255[^:]:%5[0-9]", host, sport) != 2 &&
			    sscanf(patterns, "%255[^/]/%5[0-9]", host, sport) != 2) {
				debug("%.100s, line %lu: Bad permitopen specification "
				    "<%.100s>", file, linenum, patterns);
				auth_debug_add("%.100s, line %lu: "
				    "Bad permitopen specification", file, linenum);
				xfree(patterns);
				goto bad_option;
			}
			if ((port = a2port(sport)) == 0) {
				debug("%.100s, line %lu: Bad permitopen port <%.100s>",
				    file, linenum, sport);
				auth_debug_add("%.100s, line %lu: "
				    "Bad permitopen port", file, linenum);
				xfree(patterns);
				goto bad_option;
			}
			if (options.allow_tcp_forwarding)
				channel_add_permitted_opens(host, port);
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

	if (!use_privsep)
		auth_debug_send();

	/* grant access */
	return 1;

bad_option:
	log("Bad options in %.100s file, line %lu: %.50s",
	    file, linenum, opts);
	auth_debug_add("Bad options in %.100s file, line %lu: %.50s",
	    file, linenum, opts);

	if (!use_privsep)
		auth_debug_send();

	/* deny access */
	return 0;
}
