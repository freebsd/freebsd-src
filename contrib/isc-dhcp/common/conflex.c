/* conflex.c

   Lexical scanner for dhcpd config file... */

/*
 * Copyright (c) 1995-2002 Internet Software Consortium.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of The Internet Software Consortium nor the names
 *    of its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INTERNET SOFTWARE CONSORTIUM AND
 * CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE INTERNET SOFTWARE CONSORTIUM OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This software has been written for the Internet Software Consortium
 * by Ted Lemon in cooperation with Vixie Enterprises and Nominum, Inc.
 * To learn more about the Internet Software Consortium, see
 * ``http://www.isc.org/''.  To learn more about Vixie Enterprises,
 * see ``http://www.vix.com''.   To learn more about Nominum, Inc., see
 * ``http://www.nominum.com''.
 */

#ifndef lint
static char copyright[] =
"$Id: conflex.c,v 1.92.2.6 2002/11/17 02:26:56 dhankins Exp $ Copyright (c) 1995-2002 The Internet Software Consortium.  All rights reserved.\n";
#endif /* not lint */

#include "dhcpd.h"
#include <ctype.h>

static int get_char PROTO ((struct parse *));
static enum dhcp_token get_token PROTO ((struct parse *));
static void skip_to_eol PROTO ((struct parse *));
static enum dhcp_token read_string PROTO ((struct parse *));
static enum dhcp_token read_number PROTO ((int, struct parse *));
static enum dhcp_token read_num_or_name PROTO ((int, struct parse *));
static enum dhcp_token intern PROTO ((char *, enum dhcp_token));

isc_result_t new_parse (cfile, file, inbuf, buflen, name, eolp)
	struct parse **cfile;
	int file;
	char *inbuf;
	unsigned buflen;
	const char *name;
	int eolp;
{
	struct parse *tmp;

	tmp = dmalloc (sizeof (struct parse), MDL);
	if (!tmp)
		return ISC_R_NOMEMORY;
	memset (tmp, 0, sizeof *tmp);

	tmp -> token = 0;
	tmp -> tlname = name;
	tmp -> lpos = tmp -> line = 1;
	tmp -> cur_line = tmp -> line1;
	tmp -> prev_line = tmp -> line2;
	tmp -> token_line = tmp -> cur_line;
	tmp -> cur_line [0] = tmp -> prev_line [0] = 0;
	tmp -> warnings_occurred = 0;
	tmp -> file = file;
	tmp -> eol_token = eolp;

	tmp -> bufix = 0;
	tmp -> buflen = buflen;
	if (inbuf) {
		tmp -> bufsiz = 0;
		tmp -> inbuf = inbuf;
	} else {
		tmp -> inbuf = dmalloc (8192, MDL);
		if (!tmp -> inbuf) {
			dfree (tmp, MDL);
			return ISC_R_NOMEMORY;
		}
		tmp -> bufsiz = 8192;
	}

	*cfile = tmp;
	return ISC_R_SUCCESS;
}

isc_result_t end_parse (cfile)
	struct parse **cfile;
{
	if ((*cfile) -> bufsiz)
		dfree ((*cfile) -> inbuf, MDL);
	dfree (*cfile, MDL);
	*cfile = (struct parse *)0;
	return ISC_R_SUCCESS;
}

static int get_char (cfile)
	struct parse *cfile;
{
	/* My kingdom for WITH... */
	int c;

	if (cfile -> bufix == cfile -> buflen) {
		if (cfile -> file != -1) {
			cfile -> buflen =
				read (cfile -> file,
				      cfile -> inbuf, cfile -> bufsiz);
			if (cfile -> buflen == 0) {
				c = EOF;
				cfile -> bufix = 0;
			} else if (cfile -> buflen < 0) {
				c = EOF;
				cfile -> bufix = cfile -> buflen = 0;
			} else {
				c = cfile -> inbuf [0];
				cfile -> bufix = 1;
			}
		} else
			c = EOF;
	} else {
		c = cfile -> inbuf [cfile -> bufix];
		cfile -> bufix++;
	}

	if (!cfile -> ugflag) {
		if (c == EOL) {
			if (cfile -> cur_line == cfile -> line1) {	
				cfile -> cur_line = cfile -> line2;
				cfile -> prev_line = cfile -> line1;
			} else {
				cfile -> cur_line = cfile -> line1;
				cfile -> prev_line = cfile -> line2;
			}
			cfile -> line++;
			cfile -> lpos = 1;
			cfile -> cur_line [0] = 0;
		} else if (c != EOF) {
			if (cfile -> lpos <= 80) {
				cfile -> cur_line [cfile -> lpos - 1] = c;
				cfile -> cur_line [cfile -> lpos] = 0;
			}
			cfile -> lpos++;
		}
	} else
		cfile -> ugflag = 0;
	return c;		
}

static enum dhcp_token get_token (cfile)
	struct parse *cfile;
{
	int c;
	enum dhcp_token ttok;
	static char tb [2];
	int l, p, u;

	do {
		l = cfile -> line;
		p = cfile -> lpos;
		u = cfile -> ugflag;

		c = get_char (cfile);
#ifdef OLD_LEXER
		if (c == '\n' && p == 1 && !u
		    && cfile -> comment_index < sizeof cfile -> comments)
			cfile -> comments [cfile -> comment_index++] = '\n';
#endif

		if (!(c == '\n' && cfile -> eol_token)
		    && isascii (c) && isspace (c))
			continue;
		if (c == '#') {
#ifdef OLD_LEXER
			if (cfile -> comment_index < sizeof cfile -> comments)
			    cfile -> comments [cfile -> comment_index++] = '#';
#endif
			skip_to_eol (cfile);
			continue;
		}
		if (c == '"') {
			cfile -> lexline = l;
			cfile -> lexchar = p;
			ttok = read_string (cfile);
			break;
		}
		if ((isascii (c) && isdigit (c)) || c == '-') {
			cfile -> lexline = l;
			cfile -> lexchar = p;
			ttok = read_number (c, cfile);
			break;
		} else if (isascii (c) && isalpha (c)) {
			cfile -> lexline = l;
			cfile -> lexchar = p;
			ttok = read_num_or_name (c, cfile);
			break;
		} else if (c == EOF) {
			ttok = END_OF_FILE;
			cfile -> tlen = 0;
			break;
		} else {
			cfile -> lexline = l;
			cfile -> lexchar = p;
			tb [0] = c;
			tb [1] = 0;
			cfile -> tval = tb;
			cfile -> tlen = 1;
			ttok = c;
			break;
		}
	} while (1);
	return ttok;
}

enum dhcp_token next_token (rval, rlen, cfile)
	const char **rval;
	unsigned *rlen;
	struct parse *cfile;
{
	int rv;

	if (cfile -> token) {
		if (cfile -> lexline != cfile -> tline)
			cfile -> token_line = cfile -> cur_line;
		cfile -> lexchar = cfile -> tlpos;
		cfile -> lexline = cfile -> tline;
		rv = cfile -> token;
		cfile -> token = 0;
	} else {
		rv = get_token (cfile);
		cfile -> token_line = cfile -> cur_line;
	}
	if (rval)
		*rval = cfile -> tval;
	if (rlen)
		*rlen = cfile -> tlen;
#ifdef DEBUG_TOKENS
	fprintf (stderr, "%s:%d ", cfile -> tval, rv);
#endif
	return rv;
}

enum dhcp_token peek_token (rval, rlen, cfile)
	const char **rval;
	unsigned int *rlen;
	struct parse *cfile;
{
	int x;

	if (!cfile -> token) {
		cfile -> tlpos = cfile -> lexchar;
		cfile -> tline = cfile -> lexline;
		cfile -> token = get_token (cfile);
		if (cfile -> lexline != cfile -> tline)
			cfile -> token_line = cfile -> prev_line;

		x = cfile -> lexchar;
		cfile -> lexchar = cfile -> tlpos;
		cfile -> tlpos = x;

		x = cfile -> lexline;
		cfile -> lexline = cfile -> tline;
		cfile -> tline = x;
	}
	if (rval)
		*rval = cfile -> tval;
	if (rlen)
		*rlen = cfile -> tlen;
#ifdef DEBUG_TOKENS
	fprintf (stderr, "(%s:%d) ", cfile -> tval, cfile -> token);
#endif
	return cfile -> token;
}

static void skip_to_eol (cfile)
	struct parse *cfile;
{
	int c;
	do {
		c = get_char (cfile);
		if (c == EOF)
			return;
#ifdef OLD_LEXER
		if (cfile -> comment_index < sizeof (cfile -> comments))
			comments [cfile -> comment_index++] = c;
#endif
		if (c == EOL) {
			return;
		}
	} while (1);
}

static enum dhcp_token read_string (cfile)
	struct parse *cfile;
{
	int i;
	int bs = 0;
	int c;
	int value;
	int hex;

	for (i = 0; i < sizeof cfile -> tokbuf; i++) {
	      again:
		c = get_char (cfile);
		if (c == EOF) {
			parse_warn (cfile, "eof in string constant");
			break;
		}
		if (bs == 1) {
			switch (c) {
			      case 't':
				cfile -> tokbuf [i] = '\t';
				break;
			      case 'r':
				cfile -> tokbuf [i] = '\r';
				break;
			      case 'n':
				cfile -> tokbuf [i] = '\n';
				break;
			      case 'b':
				cfile -> tokbuf [i] = '\b';
				break;
			      case '0':
			      case '1':
			      case '2':
			      case '3':
				hex = 0;
				value = c - '0';
				++bs;
				goto again;
			      case 'x':
				hex = 1;
				value = 0;
				++bs;
				goto again;
			      default:
				cfile -> tokbuf [i] = c;
				bs = 0;
				break;
			}
			bs = 0;
		} else if (bs > 1) {
			if (hex) {
				if (c >= '0' && c <= '9') {
					value = value * 16 + (c - '0');
				} else if (c >= 'a' && c <= 'f') {
					value = value * 16 + (c - 'a' + 10);
				} else if (c >= 'A' && c <= 'F') {
					value = value * 16 + (c - 'A' + 10);
				} else {
					parse_warn (cfile,
						    "invalid hex digit: %x",
						    c);
					bs = 0;
					continue;
				}
				if (++bs == 4) {
					cfile -> tokbuf [i] = value;
					bs = 0;
				} else
					goto again;
			} else {
				if (c >= '0' && c <= '9') {
					value = value * 8 + (c - '0');
				} else {
				    if (value != 0) {
					parse_warn (cfile,
						    "invalid octal digit %x",
						    c);
					continue;
				    } else
					cfile -> tokbuf [i] = 0;
				    bs = 0;
				}
				if (++bs == 4) {
					cfile -> tokbuf [i] = value;
					bs = 0;
				} else
					goto again;
			}
		} else if (c == '\\') {
			bs = 1;
			goto again;
		} else if (c == '"')
			break;
		else
			cfile -> tokbuf [i] = c;
	}
	/* Normally, I'd feel guilty about this, but we're talking about
	   strings that'll fit in a DHCP packet here... */
	if (i == sizeof cfile -> tokbuf) {
		parse_warn (cfile,
			    "string constant larger than internal buffer");
		--i;
	}
	cfile -> tokbuf [i] = 0;
	cfile -> tlen = i;
	cfile -> tval = cfile -> tokbuf;
	return STRING;
}

static enum dhcp_token read_number (c, cfile)
	int c;
	struct parse *cfile;
{
	int seenx = 0;
	int i = 0;
	int token = NUMBER;

	cfile -> tokbuf [i++] = c;
	for (; i < sizeof cfile -> tokbuf; i++) {
		c = get_char (cfile);
		if (!seenx && c == 'x') {
			seenx = 1;
#ifndef OLD_LEXER
		} else if (isascii (c) && !isxdigit (c) &&
			   (c == '-' || c == '_' || isalpha (c))) {
			token = NAME;
		} else if (isascii (c) && !isdigit (c) && isxdigit (c)) {
			token = NUMBER_OR_NAME;
#endif
		} else if (!isascii (c) || !isxdigit (c)) {
			if (c != EOF) {
				cfile -> bufix--;
				cfile -> ugflag = 1;
			}
			break;
		}
		cfile -> tokbuf [i] = c;
	}
	if (i == sizeof cfile -> tokbuf) {
		parse_warn (cfile,
			    "numeric token larger than internal buffer");
		--i;
	}
	cfile -> tokbuf [i] = 0;
	cfile -> tlen = i;
	cfile -> tval = cfile -> tokbuf;
	return token;
}

static enum dhcp_token read_num_or_name (c, cfile)
	int c;
	struct parse *cfile;
{
	int i = 0;
	enum dhcp_token rv = NUMBER_OR_NAME;
	cfile -> tokbuf [i++] = c;
	for (; i < sizeof cfile -> tokbuf; i++) {
		c = get_char (cfile);
		if (!isascii (c) ||
		    (c != '-' && c != '_' && !isalnum (c))) {
			if (c != EOF) {
				cfile -> bufix--;
				cfile -> ugflag = 1;
			}
			break;
		}
		if (!isxdigit (c))
			rv = NAME;
		cfile -> tokbuf [i] = c;
	}
	if (i == sizeof cfile -> tokbuf) {
		parse_warn (cfile, "token larger than internal buffer");
		--i;
	}
	cfile -> tokbuf [i] = 0;
	cfile -> tlen = i;
	cfile -> tval = cfile -> tokbuf;
	return intern (cfile -> tval, rv);
}

static enum dhcp_token intern (atom, dfv)
	char *atom;
	enum dhcp_token dfv;
{
	if (!isascii (atom [0]))
		return dfv;

	switch (tolower (atom [0])) {
	      case '-':
		if (atom [1] == 0)
			return MINUS;
		break;

	      case 'a':
		if (!strncasecmp (atom + 1, "uth", 3)) {
			if (!strncasecmp (atom + 3, "uthenticat", 10)) {
				if (!strcasecmp (atom + 13, "ed"))
					return AUTHENTICATED;
				if (!strcasecmp (atom + 13, "ion"))
					return AUTHENTICATION;
				break;
			}
			if (!strcasecmp (atom + 1, "uthoritative"))
				return AUTHORITATIVE;
			break;
		}
		if (!strcasecmp (atom + 1, "nd"))
			return AND;
		if (!strcasecmp (atom + 1, "ppend"))
			return APPEND;
		if (!strcasecmp (atom + 1, "llow"))
			return ALLOW;
		if (!strcasecmp (atom + 1, "lias"))
			return ALIAS;
		if (!strcasecmp (atom + 1, "lgorithm"))
			return ALGORITHM;
		if (!strcasecmp (atom + 1, "bandoned"))
			return TOKEN_ABANDONED;
		if (!strcasecmp (atom + 1, "dd"))
			return TOKEN_ADD;
		if (!strcasecmp (atom + 1, "ll"))
			return ALL;
		if (!strcasecmp (atom + 1, "t"))
			return AT;
		if (!strcasecmp (atom + 1, "rray"))
			return ARRAY;
		if (!strcasecmp (atom + 1, "ddress"))
			return ADDRESS;
		if (!strcasecmp (atom + 1, "ctive"))
			return TOKEN_ACTIVE;
		break;
	      case 'b':
		if (!strcasecmp (atom + 1, "ackup"))
			return TOKEN_BACKUP;
		if (!strcasecmp (atom + 1, "ootp"))
			return TOKEN_BOOTP;
		if (!strcasecmp (atom + 1, "inding"))
			return BINDING;
		if (!strcasecmp (atom + 1, "inary-to-ascii"))
			return BINARY_TO_ASCII;
		if (!strcasecmp (atom + 1, "ackoff-cutoff"))
			return BACKOFF_CUTOFF;
		if (!strcasecmp (atom + 1, "ooting"))
			return BOOTING;
		if (!strcasecmp (atom + 1, "oot-unknown-clients"))
			return BOOT_UNKNOWN_CLIENTS;
		if (!strcasecmp (atom + 1, "reak"))
			return BREAK;
		if (!strcasecmp (atom + 1, "illing"))
			return BILLING;
		if (!strcasecmp (atom + 1, "oolean"))
			return BOOLEAN;
		if (!strcasecmp (atom + 1, "alance"))
			return BALANCE;
		if (!strcasecmp (atom + 1, "ound"))
			return BOUND;
		break;
	      case 'c':
		if (!strcasecmp (atom + 1, "ase"))
			return CASE;
		if (!strcasecmp (atom + 1, "ommit"))
			return COMMIT;
		if (!strcasecmp (atom + 1, "ode"))
			return CODE;
		if (!strcasecmp (atom + 1, "onfig-option"))
			return CONFIG_OPTION;
		if (!strcasecmp (atom + 1, "heck"))
			return CHECK;
		if (!strcasecmp (atom + 1, "lass"))
			return CLASS;
		if (!strcasecmp (atom + 1, "lose"))
			return TOKEN_CLOSE;
		if (!strcasecmp (atom + 1, "reate"))
			return TOKEN_CREATE;
		if (!strcasecmp (atom + 1, "iaddr"))
			return CIADDR;
		if (!strncasecmp (atom + 1, "lient", 5)) {
			if (!strcasecmp (atom + 6, "-identifier"))
				return CLIENT_IDENTIFIER;
			if (!strcasecmp (atom + 6, "-hostname"))
				return CLIENT_HOSTNAME;
			if (!strcasecmp (atom + 6, "-state"))
				return CLIENT_STATE;
			if (!strcasecmp (atom + 6, "-updates"))
				return CLIENT_UPDATES;
			if (!strcasecmp (atom + 6, "s"))
				return CLIENTS;
		}
		if (!strcasecmp (atom + 1, "oncat"))
			return CONCAT;
		if (!strcasecmp (atom + 1, "onnect"))
			return CONNECT;
		if (!strcasecmp (atom + 1, "ommunications-interrupted"))
			return COMMUNICATIONS_INTERRUPTED;
		if (!strcasecmp (atom + 1, "ltt"))
			return CLTT;
		break;
	      case 'd':
		if (!strcasecmp (atom + 1, "ns-update"))
			return DNS_UPDATE;
		if (!strcasecmp (atom + 1, "ns-delete"))
			return DNS_DELETE;
		if (!strcasecmp (atom + 1, "omain"))
			return DOMAIN;
		if (!strcasecmp (atom + 1, "omain-name"))
			return DOMAIN_NAME;
		if (!strcasecmp (atom + 1, "o-forward-update"))
			return DO_FORWARD_UPDATE;
		if (!strcasecmp (atom + 1, "ebug"))
			return TOKEN_DEBUG;
		if (!strcasecmp (atom + 1, "eny"))
			return DENY;
		if (!strcasecmp (atom + 1, "eleted"))
			return TOKEN_DELETED;
		if (!strcasecmp (atom + 1, "elete"))
			return TOKEN_DELETE;
		if (!strncasecmp (atom + 1, "efault", 6)) {
			if (!atom [7])
				return DEFAULT;
			if (!strcasecmp (atom + 7, "-lease-time"))
				return DEFAULT_LEASE_TIME;
			break;
		}
		if (!strncasecmp (atom + 1, "ynamic", 6)) {
			if (!atom [7])
				return DYNAMIC;
			if (!strncasecmp (atom + 7, "-bootp", 6)) {
				if (!atom [13])
					return DYNAMIC_BOOTP;
				if (!strcasecmp (atom + 13, "-lease-cutoff"))
					return DYNAMIC_BOOTP_LEASE_CUTOFF;
				if (!strcasecmp (atom + 13, "-lease-length"))
					return DYNAMIC_BOOTP_LEASE_LENGTH;
				break;
			}
		}
		if (!strcasecmp (atom + 1, "uplicates"))
			return DUPLICATES;
		if (!strcasecmp (atom + 1, "eclines"))
			return DECLINES;
		if (!strncasecmp (atom + 1, "efine", 5)) {
			if (!strcasecmp (atom + 6, "d"))
				return DEFINED;
			if (!atom [6])
				return DEFINE;
		}
		break;
	      case 'e':
		if (isascii (atom [1]) && tolower (atom [1]) == 'x') {
			if (!strcasecmp (atom + 2, "tract-int"))
				return EXTRACT_INT;
			if (!strcasecmp (atom + 2, "ists"))
				return EXISTS;
			if (!strcasecmp (atom + 2, "piry"))
				return EXPIRY;
			if (!strcasecmp (atom + 2, "pire"))
				return EXPIRE;
			if (!strcasecmp (atom + 2, "pired"))
				return TOKEN_EXPIRED;
		}
		if (!strcasecmp (atom + 1, "ncode-int"))
			return ENCODE_INT;
		if (!strcasecmp (atom + 1, "thernet"))
			return ETHERNET;
		if (!strcasecmp (atom + 1, "nds"))
			return ENDS;
		if (!strncasecmp (atom + 1, "ls", 2)) {
			if (!strcasecmp (atom + 3, "e"))
				return ELSE;
			if (!strcasecmp (atom + 3, "if"))
				return ELSIF;
			break;
		}
		if (!strcasecmp (atom + 1, "rror"))
			return ERROR;
		if (!strcasecmp (atom + 1, "val"))
			return EVAL;
		if (!strcasecmp (atom + 1, "ncapsulate"))
			return ENCAPSULATE;
		break;
	      case 'f':
		if (!strcasecmp (atom + 1, "atal"))
			return FATAL;
		if (!strcasecmp (atom + 1, "ilename"))
			return FILENAME;
		if (!strcasecmp (atom + 1, "ixed-address"))
			return FIXED_ADDR;
		if (!strcasecmp (atom + 1, "ddi"))
			return FDDI;
		if (!strcasecmp (atom + 1, "ormerr"))
			return NS_FORMERR;
		if (!strcasecmp (atom + 1, "unction"))
			return FUNCTION;
		if (!strcasecmp (atom + 1, "ailover"))
			return FAILOVER;
		if (!strcasecmp (atom + 1, "ree"))
			return TOKEN_FREE;
		break;
	      case 'g':
		if (!strcasecmp (atom + 1, "iaddr"))
			return GIADDR;
		if (!strcasecmp (atom + 1, "roup"))
			return GROUP;
		if (!strcasecmp (atom + 1, "et-lease-hostnames"))
			return GET_LEASE_HOSTNAMES;
		break;
	      case 'h':
		if (!strcasecmp (atom + 1, "ba"))
			return HBA;
		if (!strcasecmp (atom + 1, "ost"))
			return HOST;
		if (!strcasecmp (atom + 1, "ost-decl-name"))
			return HOST_DECL_NAME;
		if (!strcasecmp (atom + 1, "ardware"))
			return HARDWARE;
		if (!strcasecmp (atom + 1, "ostname"))
			return HOSTNAME;
		if (!strcasecmp (atom + 1, "elp"))
			return TOKEN_HELP;
		break;
	      case 'i':
		if (!strcasecmp (atom + 1, "nclude"))
			return INCLUDE;
		if (!strcasecmp (atom + 1, "nteger"))
			return INTEGER;
		if (!strcasecmp (atom + 1, "nfinite"))
			return INFINITE;
		if (!strcasecmp (atom + 1, "nfo"))
			return INFO;
		if (!strcasecmp (atom + 1, "p-address"))
			return IP_ADDRESS;
		if (!strcasecmp (atom + 1, "nitial-interval"))
			return INITIAL_INTERVAL;
		if (!strcasecmp (atom + 1, "nterface"))
			return INTERFACE;
		if (!strcasecmp (atom + 1, "dentifier"))
			return IDENTIFIER;
		if (!strcasecmp (atom + 1, "f"))
			return IF;
		if (!strcasecmp (atom + 1, "s"))
			return IS;
		if (!strcasecmp (atom + 1, "gnore"))
			return IGNORE;
		break;
	      case 'k':
		if (!strcasecmp (atom + 1, "nown"))
			return KNOWN;
		if (!strcasecmp (atom + 1, "ey"))
			return KEY;
		break;
	      case 'l':
		if (!strcasecmp (atom + 1, "ease"))
			return LEASE;
		if (!strcasecmp (atom + 1, "eased-address"))
			return LEASED_ADDRESS;
		if (!strcasecmp (atom + 1, "ease-time"))
			return LEASE_TIME;
		if (!strcasecmp (atom + 1, "imit"))
			return LIMIT;
		if (!strcasecmp (atom + 1, "et"))
			return LET;
		if (!strcasecmp (atom + 1, "oad"))
			return LOAD;
		if (!strcasecmp (atom + 1, "og"))
			return LOG;
		break;
	      case 'm':
		if (!strncasecmp (atom + 1, "ax", 2)) {
			if (!atom [3])
				return TOKEN_MAX;
			if (!strcasecmp (atom + 3, "-lease-time"))
				return MAX_LEASE_TIME;
			if (!strcasecmp (atom + 3, "-transmit-idle"))
				return MAX_TRANSMIT_IDLE;
			if (!strcasecmp (atom + 3, "-response-delay"))
				return MAX_RESPONSE_DELAY;
			if (!strcasecmp (atom + 3, "-unacked-updates"))
				return MAX_UNACKED_UPDATES;
		}
		if (!strncasecmp (atom + 1, "in-", 3)) {
			if (!strcasecmp (atom + 4, "lease-time"))
				return MIN_LEASE_TIME;
			if (!strcasecmp (atom + 4, "secs"))
				return MIN_SECS;
			break;
		}
		if (!strncasecmp (atom + 1, "edi", 3)) {
			if (!strcasecmp (atom + 4, "a"))
				return MEDIA;
			if (!strcasecmp (atom + 4, "um"))
				return MEDIUM;
			break;
		}
		if (!strcasecmp (atom + 1, "atch"))
			return MATCH;
		if (!strcasecmp (atom + 1, "embers"))
			return MEMBERS;
		if (!strcasecmp (atom + 1, "y"))
			return MY;
		if (!strcasecmp (atom + 1, "clt"))
			return MCLT;
		break;
	      case 'n':
		if (!strcasecmp (atom + 1, "ormal"))
			return NORMAL;
		if (!strcasecmp (atom + 1, "ameserver"))
			return NAMESERVER;
		if (!strcasecmp (atom + 1, "etmask"))
			return NETMASK;
		if (!strcasecmp (atom + 1, "ever"))
			return NEVER;
		if (!strcasecmp (atom + 1, "ext-server"))
			return NEXT_SERVER;
		if (!strcasecmp (atom + 1, "ot"))
			return TOKEN_NOT;
		if (!strcasecmp (atom + 1, "o"))
			return NO;
		if (!strcasecmp (atom + 1, "s-update"))
			return NS_UPDATE;
		if (!strcasecmp (atom + 1, "oerror"))
			return NS_NOERROR;
		if (!strcasecmp (atom + 1, "otauth"))
			return NS_NOTAUTH;
		if (!strcasecmp (atom + 1, "otimp"))
			return NS_NOTIMP;
		if (!strcasecmp (atom + 1, "otzone"))
			return NS_NOTZONE;
		if (!strcasecmp (atom + 1, "xdomain"))
			return NS_NXDOMAIN;
		if (!strcasecmp (atom + 1, "xrrset"))
			return NS_NXRRSET;
		if (!strcasecmp (atom + 1, "ull"))
			return TOKEN_NULL;
		if (!strcasecmp (atom + 1, "ext"))
			return TOKEN_NEXT;
		if (!strcasecmp (atom + 1, "ew"))
			return TOKEN_NEW;
		break;
	      case 'o':
		if (!strcasecmp (atom + 1, "mapi"))
			return OMAPI;
		if (!strcasecmp (atom + 1, "r"))
			return OR;
		if (!strcasecmp (atom + 1, "n"))
			return ON;
		if (!strcasecmp (atom + 1, "pen"))
			return TOKEN_OPEN;
		if (!strcasecmp (atom + 1, "ption"))
			return OPTION;
		if (!strcasecmp (atom + 1, "ne-lease-per-client"))
			return ONE_LEASE_PER_CLIENT;
		if (!strcasecmp (atom + 1, "f"))
			return OF;
		if (!strcasecmp (atom + 1, "wner"))
			return OWNER;
		break;
	      case 'p':
		if (!strcasecmp (atom + 1, "repend"))
			return PREPEND;
		if (!strcasecmp (atom + 1, "acket"))
			return PACKET;
		if (!strcasecmp (atom + 1, "ool"))
			return POOL;
		if (!strcasecmp (atom + 1, "seudo"))
			return PSEUDO;
		if (!strcasecmp (atom + 1, "eer"))
			return PEER;
		if (!strcasecmp (atom + 1, "rimary"))
			return PRIMARY;
		if (!strncasecmp (atom + 1, "artner", 6)) {
			if (!atom [7])
				return PARTNER;
			if (!strcasecmp (atom + 7, "-down"))
				return PARTNER_DOWN;
		}
		if (!strcasecmp (atom + 1, "ort"))
			return PORT;
		if (!strcasecmp (atom + 1, "otential-conflict"))
			return POTENTIAL_CONFLICT;
		if (!strcasecmp (atom + 1, "ick-first-value") ||
		    !strcasecmp (atom + 1, "ick"))
			return PICK;
		if (!strcasecmp (atom + 1, "aused"))
			return PAUSED;
		break;
	      case 'r':
		if (!strcasecmp (atom + 1, "esolution-interrupted"))
			return RESOLUTION_INTERRUPTED;
		if (!strcasecmp (atom + 1, "ange"))
			return RANGE;
		if (!strcasecmp (atom + 1, "ecover"))
			return RECOVER;
		if (!strcasecmp (atom + 1, "ecover-done"))
			return RECOVER_DONE;
		if (!strcasecmp (atom + 1, "ecover-wait"))
			return RECOVER_WAIT;
		if (!strcasecmp (atom + 1, "econtact-interval"))
			return RECONTACT_INTERVAL;
		if (!strcasecmp (atom + 1, "equest"))
			return REQUEST;
		if (!strcasecmp (atom + 1, "equire"))
			return REQUIRE;
		if (!strcasecmp (atom + 1, "equire"))
			return REQUIRE;
		if (!strcasecmp (atom + 1, "etry"))
			return RETRY;
		if (!strcasecmp (atom + 1, "eturn"))
			return RETURN;
		if (!strcasecmp (atom + 1, "enew"))
			return RENEW;
		if (!strcasecmp (atom + 1, "ebind"))
			return REBIND;
		if (!strcasecmp (atom + 1, "eboot"))
			return REBOOT;
		if (!strcasecmp (atom + 1, "eject"))
			return REJECT;
		if (!strcasecmp (atom + 1, "everse"))
			return REVERSE;
		if (!strcasecmp (atom + 1, "elease"))
			return RELEASE;
		if (!strcasecmp (atom + 1, "efused"))
			return NS_REFUSED;
		if (!strcasecmp (atom + 1, "eleased"))
			return TOKEN_RELEASED;
		if (!strcasecmp (atom + 1, "eset"))
			return TOKEN_RESET;
		if (!strcasecmp (atom + 1, "eserved"))
			return TOKEN_RESERVED;
		if (!strcasecmp (atom + 1, "emove"))
			return REMOVE;
		if (!strcasecmp (atom + 1, "efresh"))
			return REFRESH;
		break;
	      case 's':
		if (!strcasecmp (atom + 1, "tate"))
			return STATE;
		if (!strcasecmp (atom + 1, "ecret"))
			return SECRET;
		if (!strcasecmp (atom + 1, "ervfail"))
			return NS_SERVFAIL;
		if (!strcasecmp (atom + 1, "witch"))
			return SWITCH;
		if (!strcasecmp (atom + 1, "igned"))
			return SIGNED;
		if (!strcasecmp (atom + 1, "tring"))
			return STRING_TOKEN;
		if (!strcasecmp (atom + 1, "uffix"))
			return SUFFIX;
		if (!strcasecmp (atom + 1, "earch"))
			return SEARCH;
		if (!strcasecmp (atom + 1, "tarts"))
			return STARTS;
		if (!strcasecmp (atom + 1, "iaddr"))
			return SIADDR;
		if (!strcasecmp (atom + 1, "hared-network"))
			return SHARED_NETWORK;
		if (!strcasecmp (atom + 1, "econdary"))
			return SECONDARY;
		if (!strcasecmp (atom + 1, "erver-name"))
			return SERVER_NAME;
		if (!strcasecmp (atom + 1, "erver-identifier"))
			return SERVER_IDENTIFIER;
		if (!strcasecmp (atom + 1, "erver"))
			return SERVER;
		if (!strcasecmp (atom + 1, "elect-timeout"))
			return SELECT_TIMEOUT;
		if (!strcasecmp (atom + 1, "elect"))
			return SELECT;
		if (!strcasecmp (atom + 1, "end"))
			return SEND;
		if (!strcasecmp (atom + 1, "cript"))
			return SCRIPT;
		if (!strcasecmp (atom + 1, "upersede"))
			return SUPERSEDE;
		if (!strncasecmp (atom + 1, "ub", 2)) {
			if (!strcasecmp (atom + 3, "string"))
				return SUBSTRING;
			if (!strcasecmp (atom + 3, "net"))
				return SUBNET;
			if (!strcasecmp (atom + 3, "class"))
				return SUBCLASS;
			break;
		}
		if (!strcasecmp (atom + 1, "pawn"))
			return SPAWN;
		if (!strcasecmp (atom + 1, "pace"))
			return SPACE;
		if (!strcasecmp (atom + 1, "tatic"))
			return STATIC;
		if (!strcasecmp (atom + 1, "plit"))
			return SPLIT;
		if (!strcasecmp (atom + 1, "et"))
			return TOKEN_SET;
		if (!strcasecmp (atom + 1, "econds"))
			return SECONDS;
		if (!strcasecmp (atom + 1, "hutdown"))
			return SHUTDOWN;
		if (!strcasecmp (atom + 1, "tartup"))
			return STARTUP;
		break;
	      case 't':
		if (!strcasecmp (atom + 1, "imestamp"))
			return TIMESTAMP;
		if (!strcasecmp (atom + 1, "imeout"))
			return TIMEOUT;
		if (!strcasecmp (atom + 1, "oken-ring"))
			return TOKEN_RING;
		if (!strcasecmp (atom + 1, "ext"))
			return TEXT;
		if (!strcasecmp (atom + 1, "stp"))
			return TSTP;
		if (!strcasecmp (atom + 1, "sfp"))
			return TSFP;
		if (!strcasecmp (atom + 1, "ransmission"))
			return TRANSMISSION;
		break;
	      case 'u':
		if (!strcasecmp (atom + 1, "nset"))
			return UNSET;
		if (!strcasecmp (atom + 1, "nsigned"))
			return UNSIGNED;
		if (!strcasecmp (atom + 1, "id"))
			return UID;
		if (!strncasecmp (atom + 1, "se", 2)) {
			if (!strcasecmp (atom + 3, "r-class"))
				return USER_CLASS;
			if (!strcasecmp (atom + 3, "-host-decl-names"))
				return USE_HOST_DECL_NAMES;
			if (!strcasecmp (atom + 3,
					 "-lease-addr-for-default-route"))
				return USE_LEASE_ADDR_FOR_DEFAULT_ROUTE;
			break;
		}
		if (!strncasecmp (atom + 1, "nknown", 6)) {
			if (!strcasecmp (atom + 7, "-clients"))
				return UNKNOWN_CLIENTS;
			if (!strcasecmp (atom + 7, "-state"))
				return UNKNOWN_STATE;
			if (!atom [7])
				return UNKNOWN;
			break;
		}
		if (!strcasecmp (atom + 1, "nauthenticated"))
			return AUTHENTICATED;
		if (!strcasecmp (atom + 1, "pdated-dns-rr"))
			return UPDATED_DNS_RR;
		if (!strcasecmp (atom + 1, "pdate"))
			return UPDATE;
		break;
	      case 'v':
		if (!strcasecmp (atom + 1, "endor-class"))
			return VENDOR_CLASS;
		if (!strcasecmp (atom + 1, "endor"))
			return VENDOR;
		break;
	      case 'w':
		if (!strcasecmp (atom + 1, "ith"))
			return WITH;
		break;
	      case 'y':
		if (!strcasecmp (atom + 1, "iaddr"))
			return YIADDR;
		if (!strcasecmp (atom + 1, "xdomain"))
			return NS_YXDOMAIN;
		if (!strcasecmp (atom + 1, "xrrset"))
			return NS_YXRRSET;
		break;
	      case 'z':
		if (!strcasecmp (atom + 1, "one"))
			return ZONE;
		break;
	}
	return dfv;
}
