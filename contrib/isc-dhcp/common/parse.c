/* parse.c

   Common parser code for dhcpd and dhclient. */

/*
 * Copyright (c) 1995-2001 Internet Software Consortium.
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
"$Id: parse.c,v 1.104.2.10 2002/11/03 04:31:55 dhankins Exp $ Copyright (c) 1995-2001 The Internet Software Consortium.  All rights reserved.\n";
#endif /* not lint */

#include "dhcpd.h"

/* Enumerations can be specified in option formats, and are used for
   parsing, so we define the routines that manage them here. */

struct enumeration *enumerations;

void add_enumeration (struct enumeration *enumeration)
{
	enumeration -> next = enumerations;
	enumerations = enumeration;
}

struct enumeration *find_enumeration (const char *name, int length)
{
	struct enumeration *e;

	for (e = enumerations; e; e = e -> next)
		if (strlen (e -> name) == length &&
		    !memcmp (e -> name, name, (unsigned)length))
			return e;
	return (struct enumeration *)0;
}

struct enumeration_value *find_enumeration_value (const char *name,
						  int length,
						  const char *value)
{
	struct enumeration *e;
	int i;

	e = find_enumeration (name, length);
	if (e) {
		for (i = 0; e -> values [i].name; i++) {
			if (!strcmp (value, e -> values [i].name))
				return &e -> values [i];
		}
	}
	return (struct enumeration_value *)0;
}

/* Skip to the semicolon ending the current statement.   If we encounter
   braces, the matching closing brace terminates the statement.   If we
   encounter a right brace but haven't encountered a left brace, return
   leaving the brace in the token buffer for the caller.   If we see a
   semicolon and haven't seen a left brace, return.   This lets us skip
   over:

   	statement;
	statement foo bar { }
	statement foo bar { statement { } }
	statement}
 
	...et cetera. */

void skip_to_semi (cfile)
	struct parse *cfile;
{
	skip_to_rbrace (cfile, 0);
}

void skip_to_rbrace (cfile, brace_count)
	struct parse *cfile;
	int brace_count;
{
	enum dhcp_token token;
	const char *val;

#if defined (DEBUG_TOKEN)
	log_error ("skip_to_rbrace: %d\n", brace_count);
#endif
	do {
		token = peek_token (&val, (unsigned *)0, cfile);
		if (token == RBRACE) {
			token = next_token (&val, (unsigned *)0, cfile);
			if (brace_count) {
				if (!--brace_count)
					return;
			} else
				return;
		} else if (token == LBRACE) {
			brace_count++;
		} else if (token == SEMI && !brace_count) {
			token = next_token (&val, (unsigned *)0, cfile);
			return;
		} else if (token == EOL) {
			/* EOL only happens when parsing /etc/resolv.conf,
			   and we treat it like a semicolon because the
			   resolv.conf file is line-oriented. */
			token = next_token (&val, (unsigned *)0, cfile);
			return;
		}
		token = next_token (&val, (unsigned *)0, cfile);
	} while (token != END_OF_FILE);
}

int parse_semi (cfile)
	struct parse *cfile;
{
	enum dhcp_token token;
	const char *val;

	token = next_token (&val, (unsigned *)0, cfile);
	if (token != SEMI) {
		parse_warn (cfile, "semicolon expected.");
		skip_to_semi (cfile);
		return 0;
	}
	return 1;
}

/* string-parameter :== STRING SEMI */

int parse_string (cfile, sptr, lptr)
	struct parse *cfile;
	char **sptr;
	unsigned *lptr;
{
	const char *val;
	enum dhcp_token token;
	char *s;
	unsigned len;

	token = next_token (&val, &len, cfile);
	if (token != STRING) {
		parse_warn (cfile, "expecting a string");
		skip_to_semi (cfile);
		return 0;
	}
	s = (char *)dmalloc (len + 1, MDL);
	if (!s)
		log_fatal ("no memory for string %s.", val);
	memcpy (s, val, len + 1);

	if (!parse_semi (cfile)) {
		dfree (s, MDL);
		return 0;
	}
	if (sptr)
		*sptr = s;
	else
		dfree (s, MDL);
	if (lptr)
		*lptr = len;
	return 1;
}

/*
 * hostname :== IDENTIFIER
 *		| IDENTIFIER DOT
 *		| hostname DOT IDENTIFIER
 */

char *parse_host_name (cfile)
	struct parse *cfile;
{
	const char *val;
	enum dhcp_token token;
	unsigned len = 0;
	char *s;
	char *t;
	pair c = (pair)0;
	int ltid = 0;
	
	/* Read a dotted hostname... */
	do {
		/* Read a token, which should be an identifier. */
		token = peek_token (&val, (unsigned *)0, cfile);
		if (!is_identifier (token) && token != NUMBER)
			break;
		token = next_token (&val, (unsigned *)0, cfile);

		/* Store this identifier... */
		if (!(s = (char *)dmalloc (strlen (val) + 1, MDL)))
			log_fatal ("can't allocate temp space for hostname.");
		strcpy (s, val);
		c = cons ((caddr_t)s, c);
		len += strlen (s) + 1;
		/* Look for a dot; if it's there, keep going, otherwise
		   we're done. */
		token = peek_token (&val, (unsigned *)0, cfile);
		if (token == DOT) {
			token = next_token (&val, (unsigned *)0, cfile);
			ltid = 1;
		} else
			ltid = 0;
	} while (token == DOT);

	/* Should be at least one token. */
	if (!len)
		return (char *)0;

	/* Assemble the hostname together into a string. */
	if (!(s = (char *)dmalloc (len + ltid, MDL)))
		log_fatal ("can't allocate space for hostname.");
	t = s + len + ltid;
	*--t = 0;
	if (ltid)
		*--t = '.';
	while (c) {
		pair cdr = c -> cdr;
		unsigned l = strlen ((char *)(c -> car));
		t -= l;
		memcpy (t, (char *)(c -> car), l);
		/* Free up temp space. */
		dfree (c -> car, MDL);
		dfree (c, MDL);
		c = cdr;
		if (t != s)
			*--t = '.';
	}
	return s;
}

/* ip-addr-or-hostname :== ip-address | hostname
   ip-address :== NUMBER DOT NUMBER DOT NUMBER DOT NUMBER
   
   Parse an ip address or a hostname.   If uniform is zero, put in
   an expr_substring node to limit hostnames that evaluate to more
   than one IP address. */

int parse_ip_addr_or_hostname (expr, cfile, uniform)
	struct expression **expr;
	struct parse *cfile;
	int uniform;
{
	const char *val;
	enum dhcp_token token;
	unsigned char addr [4];
	unsigned len = sizeof addr;
	char *name;
	struct expression *x = (struct expression *)0;

	token = peek_token (&val, (unsigned *)0, cfile);
	if (is_identifier (token)) {
		name = parse_host_name (cfile);
		if (!name)
			return 0;
		if (!make_host_lookup (expr, name))
			return 0;
		if (!uniform) {
			if (!make_limit (&x, *expr, 4))
				return 0;
			expression_dereference (expr, MDL);
			*expr = x;
		}
	} else if (token == NUMBER) {
		if (!parse_numeric_aggregate (cfile, addr, &len, DOT, 10, 8))
			return 0;
		return make_const_data (expr, addr, len, 0, 1, MDL);
	} else {
		if (token != RBRACE && token != LBRACE)
			token = next_token (&val, (unsigned *)0, cfile);
		parse_warn (cfile, "%s (%d): expecting IP address or hostname",
			    val, token);
		if (token != SEMI)
			skip_to_semi (cfile);
		return 0;
	}

	return 1;
}	
	
/*
 * ip-address :== NUMBER DOT NUMBER DOT NUMBER DOT NUMBER
 */

int parse_ip_addr (cfile, addr)
	struct parse *cfile;
	struct iaddr *addr;
{
	const char *val;
	enum dhcp_token token;

	addr -> len = 4;
	if (parse_numeric_aggregate (cfile, addr -> iabuf,
				     &addr -> len, DOT, 10, 8))
		return 1;
	return 0;
}	

/*
 * hardware-parameter :== HARDWARE hardware-type colon-seperated-hex-list SEMI
 * hardware-type :== ETHERNET | TOKEN_RING | FDDI
 */

void parse_hardware_param (cfile, hardware)
	struct parse *cfile;
	struct hardware *hardware;
{
	const char *val;
	enum dhcp_token token;
	unsigned hlen;
	unsigned char *t;

	token = next_token (&val, (unsigned *)0, cfile);
	switch (token) {
	      case ETHERNET:
		hardware -> hbuf [0] = HTYPE_ETHER;
		break;
	      case TOKEN_RING:
		hardware -> hbuf [0] = HTYPE_IEEE802;
		break;
	      case FDDI:
		hardware -> hbuf [0] = HTYPE_FDDI;
		break;
	      default:
		if (!strncmp (val, "unknown-", 8)) {
			hardware -> hbuf [0] = atoi (&val [8]);
		} else {
			parse_warn (cfile,
				    "expecting a network hardware type");
			skip_to_semi (cfile);

			return;
		}
	}

	/* Parse the hardware address information.   Technically,
	   it would make a lot of sense to restrict the length of the
	   data we'll accept here to the length of a particular hardware
	   address type.   Unfortunately, there are some broken clients
	   out there that put bogus data in the chaddr buffer, and we accept
	   that data in the lease file rather than simply failing on such
	   clients.   Yuck. */
	hlen = 0;
	token = peek_token (&val, (unsigned *)0, cfile);
	if (token == SEMI) {
		hardware -> hlen = 1;
		goto out;
	}
	t = parse_numeric_aggregate (cfile, (unsigned char *)0, &hlen,
				     COLON, 16, 8);
	if (!t) {
		hardware -> hlen = 1;
		return;
	}
	if (hlen + 1 > sizeof hardware -> hbuf) {
		dfree (t, MDL);
		parse_warn (cfile, "hardware address too long");
	} else {
		hardware -> hlen = hlen + 1;
		memcpy ((unsigned char *)&hardware -> hbuf [1], t, hlen);
		if (hlen + 1 < sizeof hardware -> hbuf)
			memset (&hardware -> hbuf [hlen + 1], 0,
				(sizeof hardware -> hbuf) - hlen - 1);
		dfree (t, MDL);
	}
	
      out:
	token = next_token (&val, (unsigned *)0, cfile);
	if (token != SEMI) {
		parse_warn (cfile, "expecting semicolon.");
		skip_to_semi (cfile);
	}
}

/* lease-time :== NUMBER SEMI */

void parse_lease_time (cfile, timep)
	struct parse *cfile;
	TIME *timep;
{
	const char *val;
	enum dhcp_token token;

	token = next_token (&val, (unsigned *)0, cfile);
	if (token != NUMBER) {
		parse_warn (cfile, "Expecting numeric lease time");
		skip_to_semi (cfile);
		return;
	}
	convert_num (cfile, (unsigned char *)timep, val, 10, 32);
	/* Unswap the number - convert_num returns stuff in NBO. */
	*timep = ntohl (*timep); /* XXX */

	parse_semi (cfile);
}

/* No BNF for numeric aggregates - that's defined by the caller.  What
   this function does is to parse a sequence of numbers seperated by
   the token specified in seperator.  If max is zero, any number of
   numbers will be parsed; otherwise, exactly max numbers are
   expected.  Base and size tell us how to internalize the numbers
   once they've been tokenized. */

unsigned char *parse_numeric_aggregate (cfile, buf,
					max, seperator, base, size)
	struct parse *cfile;
	unsigned char *buf;
	unsigned *max;
	int seperator;
	int base;
	unsigned size;
{
	const char *val;
	enum dhcp_token token;
	unsigned char *bufp = buf, *s, *t;
	unsigned count = 0;
	pair c = (pair)0;

	if (!bufp && *max) {
		bufp = (unsigned char *)dmalloc (*max * size / 8, MDL);
		if (!bufp)
			log_fatal ("no space for numeric aggregate");
		s = 0;
	} else
		s = bufp;

	do {
		if (count) {
			token = peek_token (&val, (unsigned *)0, cfile);
			if (token != seperator) {
				if (!*max)
					break;
				if (token != RBRACE && token != LBRACE)
					token = next_token (&val,
							    (unsigned *)0,
							    cfile);
				parse_warn (cfile, "too few numbers.");
				if (token != SEMI)
					skip_to_semi (cfile);
				return (unsigned char *)0;
			}
			token = next_token (&val, (unsigned *)0, cfile);
		}
		token = next_token (&val, (unsigned *)0, cfile);

		if (token == END_OF_FILE) {
			parse_warn (cfile, "unexpected end of file");
			break;
		}

		/* Allow NUMBER_OR_NAME if base is 16. */
		if (token != NUMBER &&
		    (base != 16 || token != NUMBER_OR_NAME)) {
			parse_warn (cfile, "expecting numeric value.");
			skip_to_semi (cfile);
			return (unsigned char *)0;
		}
		/* If we can, convert the number now; otherwise, build
		   a linked list of all the numbers. */
		if (s) {
			convert_num (cfile, s, val, base, size);
			s += size / 8;
		} else {
			t = (unsigned char *)dmalloc (strlen (val) + 1, MDL);
			if (!t)
				log_fatal ("no temp space for number.");
			strcpy ((char *)t, val);
			c = cons ((caddr_t)t, c);
		}
	} while (++count != *max);

	/* If we had to cons up a list, convert it now. */
	if (c) {
		bufp = (unsigned char *)dmalloc (count * size / 8, MDL);
		if (!bufp)
			log_fatal ("no space for numeric aggregate.");
		s = bufp + count - size / 8;
		*max = count;
	}
	while (c) {
		pair cdr = c -> cdr;
		convert_num (cfile, s, (char *)(c -> car), base, size);
		s -= size / 8;
		/* Free up temp space. */
		dfree (c -> car, MDL);
		dfree (c, MDL);
		c = cdr;
	}
	return bufp;
}

void convert_num (cfile, buf, str, base, size)
	struct parse *cfile;
	unsigned char *buf;
	const char *str;
	int base;
	unsigned size;
{
	const char *ptr = str;
	int negative = 0;
	u_int32_t val = 0;
	int tval;
	int max;

	if (*ptr == '-') {
		negative = 1;
		++ptr;
	}

	/* If base wasn't specified, figure it out from the data. */
	if (!base) {
		if (ptr [0] == '0') {
			if (ptr [1] == 'x') {
				base = 16;
				ptr += 2;
			} else if (isascii (ptr [1]) && isdigit (ptr [1])) {
				base = 8;
				ptr += 1;
			} else {
				base = 10;
			}
		} else {
			base = 10;
		}
	}

	do {
		tval = *ptr++;
		/* XXX assumes ASCII... */
		if (tval >= 'a')
			tval = tval - 'a' + 10;
		else if (tval >= 'A')
			tval = tval - 'A' + 10;
		else if (tval >= '0')
			tval -= '0';
		else {
			parse_warn (cfile, "Bogus number: %s.", str);
			break;
		}
		if (tval >= base) {
			parse_warn (cfile,
				    "Bogus number %s: digit %d not in base %d",
				    str, tval, base);
			break;
		}
		val = val * base + tval;
	} while (*ptr);

	if (negative)
		max = (1 << (size - 1));
	else
		max = (1 << (size - 1)) + ((1 << (size - 1)) - 1);
	if (val > max) {
		switch (base) {
		      case 8:
			parse_warn (cfile,
				    "%s%lo exceeds max (%d) for precision.",
				    negative ? "-" : "",
				    (unsigned long)val, max);
			break;
		      case 16:
			parse_warn (cfile,
				    "%s%lx exceeds max (%d) for precision.",
				    negative ? "-" : "",
				    (unsigned long)val, max);
			break;
		      default:
			parse_warn (cfile,
				    "%s%lu exceeds max (%d) for precision.",
				    negative ? "-" : "",
				    (unsigned long)val, max);
			break;
		}
	}

	if (negative) {
		switch (size) {
		      case 8:
			*buf = -(unsigned long)val;
			break;
		      case 16:
			putShort (buf, -(long)val);
			break;
		      case 32:
			putLong (buf, -(long)val);
			break;
		      default:
			parse_warn (cfile,
				    "Unexpected integer size: %d\n", size);
			break;
		}
	} else {
		switch (size) {
		      case 8:
			*buf = (u_int8_t)val;
			break;
		      case 16:
			putUShort (buf, (u_int16_t)val);
			break;
		      case 32:
			putULong (buf, val);
			break;
		      default:
			parse_warn (cfile,
				    "Unexpected integer size: %d\n", size);
			break;
		}
	}
}

/*
 * date :== NUMBER NUMBER SLASH NUMBER SLASH NUMBER 
 *		NUMBER COLON NUMBER COLON NUMBER SEMI |
 *          NUMBER NUMBER SLASH NUMBER SLASH NUMBER 
 *		NUMBER COLON NUMBER COLON NUMBER NUMBER SEMI |
 *	    NEVER
 *
 * Dates are stored in GMT or with a timezone offset; first number is day
 * of week; next is year/month/day; next is hours:minutes:seconds on a
 * 24-hour clock, followed by the timezone offset in seconds, which is
 * optional.
 */

TIME parse_date (cfile)
	struct parse *cfile;
{
	struct tm tm;
	int guess;
	int tzoff, wday, year, mon, mday, hour, min, sec;
	const char *val;
	enum dhcp_token token;
	static int months [11] = { 31, 59, 90, 120, 151, 181,
					  212, 243, 273, 304, 334 };

	/* Day of week, or "never"... */
	token = next_token (&val, (unsigned *)0, cfile);
	if (token == NEVER) {
		if (!parse_semi (cfile))
			return 0;
		return MAX_TIME;
	}

	if (token != NUMBER) {
		parse_warn (cfile, "numeric day of week expected.");
		if (token != SEMI)
			skip_to_semi (cfile);
		return (TIME)0;
	}
	wday = atoi (val);

	/* Year... */
	token = next_token (&val, (unsigned *)0, cfile);
	if (token != NUMBER) {
		parse_warn (cfile, "numeric year expected.");
		if (token != SEMI)
			skip_to_semi (cfile);
		return (TIME)0;
	}

	/* Note: the following is not a Y2K bug - it's a Y1.9K bug.   Until
	   somebody invents a time machine, I think we can safely disregard
	   it.   This actually works around a stupid Y2K bug that was present
	   in a very early beta release of dhcpd. */
	year = atoi (val);
	if (year > 1900)
		year -= 1900;

	/* Slash seperating year from month... */
	token = next_token (&val, (unsigned *)0, cfile);
	if (token != SLASH) {
		parse_warn (cfile,
			    "expected slash seperating year from month.");
		if (token != SEMI)
			skip_to_semi (cfile);
		return (TIME)0;
	}

	/* Month... */
	token = next_token (&val, (unsigned *)0, cfile);
	if (token != NUMBER) {
		parse_warn (cfile, "numeric month expected.");
		if (token != SEMI)
			skip_to_semi (cfile);
		return (TIME)0;
	}
	mon = atoi (val) - 1;

	/* Slash seperating month from day... */
	token = next_token (&val, (unsigned *)0, cfile);
	if (token != SLASH) {
		parse_warn (cfile,
			    "expected slash seperating month from day.");
		if (token != SEMI)
			skip_to_semi (cfile);
		return (TIME)0;
	}

	/* Month... */
	token = next_token (&val, (unsigned *)0, cfile);
	if (token != NUMBER) {
		parse_warn (cfile, "numeric day of month expected.");
		if (token != SEMI)
			skip_to_semi (cfile);
		return (TIME)0;
	}
	mday = atoi (val);

	/* Hour... */
	token = next_token (&val, (unsigned *)0, cfile);
	if (token != NUMBER) {
		parse_warn (cfile, "numeric hour expected.");
		if (token != SEMI)
			skip_to_semi (cfile);
		return (TIME)0;
	}
	hour = atoi (val);

	/* Colon seperating hour from minute... */
	token = next_token (&val, (unsigned *)0, cfile);
	if (token != COLON) {
		parse_warn (cfile,
			    "expected colon seperating hour from minute.");
		if (token != SEMI)
			skip_to_semi (cfile);
		return (TIME)0;
	}

	/* Minute... */
	token = next_token (&val, (unsigned *)0, cfile);
	if (token != NUMBER) {
		parse_warn (cfile, "numeric minute expected.");
		if (token != SEMI)
			skip_to_semi (cfile);
		return (TIME)0;
	}
	min = atoi (val);

	/* Colon seperating minute from second... */
	token = next_token (&val, (unsigned *)0, cfile);
	if (token != COLON) {
		parse_warn (cfile,
			    "expected colon seperating hour from minute.");
		if (token != SEMI)
			skip_to_semi (cfile);
		return (TIME)0;
	}

	/* Minute... */
	token = next_token (&val, (unsigned *)0, cfile);
	if (token != NUMBER) {
		parse_warn (cfile, "numeric minute expected.");
		if (token != SEMI)
			skip_to_semi (cfile);
		return (TIME)0;
	}
	sec = atoi (val);

	token = peek_token (&val, (unsigned *)0, cfile);
	if (token == NUMBER) {
		token = next_token (&val, (unsigned *)0, cfile);
		tzoff = atoi (val);
	} else
		tzoff = 0;

	/* Make sure the date ends in a semicolon... */
	if (!parse_semi (cfile))
		return 0;

	/* Guess the time value... */
	guess = ((((((365 * (year - 70) +	/* Days in years since '70 */
		      (year - 69) / 4 +		/* Leap days since '70 */
		      (mon			/* Days in months this year */
		       ? months [mon - 1]
		       : 0) +
		      (mon > 1 &&		/* Leap day this year */
		       !((year - 72) & 3)) +
		      mday - 1) * 24) +		/* Day of month */
		    hour) * 60) +
		  min) * 60) + sec + tzoff;

	/* This guess could be wrong because of leap seconds or other
	   weirdness we don't know about that the system does.   For
	   now, we're just going to accept the guess, but at some point
	   it might be nice to do a successive approximation here to
	   get an exact value.   Even if the error is small, if the
	   server is restarted frequently (and thus the lease database
	   is reread), the error could accumulate into something
	   significant. */

	return guess;
}

/*
 * option-name :== IDENTIFIER |
 		   IDENTIFIER . IDENTIFIER
 */

struct option *parse_option_name (cfile, allocate, known)
	struct parse *cfile;
	int allocate;
	int *known;
{
	const char *val;
	enum dhcp_token token;
	char *uname;
	struct universe *universe;
	struct option *option;

	token = next_token (&val, (unsigned *)0, cfile);
	if (!is_identifier (token)) {
		parse_warn (cfile,
			    "expecting identifier after option keyword.");
		if (token != SEMI)
			skip_to_semi (cfile);
		return (struct option *)0;
	}
	uname = dmalloc (strlen (val) + 1, MDL);
	if (!uname)
		log_fatal ("no memory for uname information.");
	strcpy (uname, val);
	token = peek_token (&val, (unsigned *)0, cfile);
	if (token == DOT) {
		/* Go ahead and take the DOT token... */
		token = next_token (&val, (unsigned *)0, cfile);

		/* The next token should be an identifier... */
		token = next_token (&val, (unsigned *)0, cfile);
		if (!is_identifier (token)) {
			parse_warn (cfile, "expecting identifier after '.'");
			if (token != SEMI)
				skip_to_semi (cfile);
			return (struct option *)0;
		}

		/* Look up the option name hash table for the specified
		   uname. */
		universe = (struct universe *)0;
		if (!universe_hash_lookup (&universe, universe_hash,
					   uname, 0, MDL)) {
			parse_warn (cfile, "no option space named %s.", uname);
			skip_to_semi (cfile);
			return (struct option *)0;
		}
	} else {
		/* Use the default hash table, which contains all the
		   standard dhcp option names. */
		val = uname;
		universe = &dhcp_universe;
	}

	/* Look up the actual option info... */
	option = (struct option *)0;
	option_hash_lookup (&option, universe -> hash, val, 0, MDL);

	/* If we didn't get an option structure, it's an undefined option. */
	if (option) {
		if (known)
			*known = 1;
	} else {
		/* If we've been told to allocate, that means that this
		   (might) be an option code definition, so we'll create
		   an option structure just in case. */
		if (allocate) {
			option = new_option (MDL);
			if (val == uname)
				option -> name = val;
			else {
				char *s;
				dfree (uname, MDL);
				s = dmalloc (strlen (val) + 1, MDL);
				if (!s)
				    log_fatal ("no memory for option %s.%s",
					       universe -> name, val);
				strcpy (s, val);
				option -> name = s;
			}
			option -> universe = universe;
			option -> code = 0;
			return option;
		}
		if (val == uname)
			parse_warn (cfile, "no option named %s", val);
		else
			parse_warn (cfile, "no option named %s in space %s",
				    val, uname);
		skip_to_semi (cfile);
		return (struct option *)0;
	}

	/* Free the initial identifier token. */
	dfree (uname, MDL);
	return option;
}

/* IDENTIFIER SEMI */

void parse_option_space_decl (cfile)
	struct parse *cfile;
{
	int token;
	const char *val;
	struct universe **ua, *nu;
	char *s;

	next_token (&val, (unsigned *)0, cfile);  /* Discard the SPACE token,
						     which was checked by the
						     caller. */
	token = next_token (&val, (unsigned *)0, cfile);
	if (!is_identifier (token)) {
		parse_warn (cfile, "expecting identifier.");
		skip_to_semi (cfile);
		return;
	}
	nu = new_universe (MDL);
	if (!nu)
		log_fatal ("No memory for new option space.");

	/* Set up the server option universe... */
	s = dmalloc (strlen (val) + 1, MDL);
	if (!s)
		log_fatal ("No memory for new option space name.");
	strcpy (s, val);
	nu -> name = s;
	nu -> lookup_func = lookup_hashed_option;
	nu -> option_state_dereference =
		hashed_option_state_dereference;
	nu -> foreach = hashed_option_space_foreach;
	nu -> save_func = save_hashed_option;
	nu -> delete_func = delete_hashed_option;
	nu -> encapsulate = hashed_option_space_encapsulate;
	nu -> decode = parse_option_buffer;
	nu -> length_size = 1;
	nu -> tag_size = 1;
	nu -> store_tag = putUChar;
	nu -> store_length = putUChar;
	nu -> index = universe_count++;
	if (nu -> index >= universe_max) {
		ua = dmalloc (universe_max * 2 * sizeof *ua, MDL);
		if (!ua)
			log_fatal ("No memory to expand option space array.");
		memcpy (ua, universes, universe_max * sizeof *ua);
		universe_max *= 2;
		dfree (universes, MDL);
		universes = ua;
	}
	universes [nu -> index] = nu;
	option_new_hash (&nu -> hash, 1, MDL);
	if (!nu -> hash)
		log_fatal ("Can't allocate %s option hash table.", nu -> name);
	universe_hash_add (universe_hash, nu -> name, 0, nu, MDL);
	parse_semi (cfile);
}

/* This is faked up to look good right now.   Ideally, this should do a
   recursive parse and allow arbitrary data structure definitions, but for
   now it just allows you to specify a single type, an array of single types,
   a sequence of types, or an array of sequences of types.

   ocd :== NUMBER EQUALS ocsd SEMI

   ocsd :== ocsd_type |
	    ocsd_type_sequence |
	    ARRAY OF ocsd_simple_type_sequence

   ocsd_type_sequence :== LBRACE ocsd_types RBRACE

   ocsd_simple_type_sequence :== LBRACE ocsd_simple_types RBRACE

   ocsd_types :== ocsd_type |
		  ocsd_types ocsd_type

   ocsd_type :== ocsd_simple_type |
		 ARRAY OF ocsd_simple_type

   ocsd_simple_types :== ocsd_simple_type |
			 ocsd_simple_types ocsd_simple_type

   ocsd_simple_type :== BOOLEAN |
			INTEGER NUMBER |
			SIGNED INTEGER NUMBER |
			UNSIGNED INTEGER NUMBER |
			IP-ADDRESS |
			TEXT |
			STRING |
			ENCAPSULATE identifier */

int parse_option_code_definition (cfile, option)
	struct parse *cfile;
	struct option *option;
{
	const char *val;
	enum dhcp_token token;
	unsigned arrayp = 0;
	int recordp = 0;
	int no_more_in_record = 0;
	char tokbuf [128];
	unsigned tokix = 0;
	char type;
	int code;
	int is_signed;
	char *s;
	int has_encapsulation = 0;
	
	/* Parse the option code. */
	token = next_token (&val, (unsigned *)0, cfile);
	if (token != NUMBER) {
		parse_warn (cfile, "expecting option code number.");
		skip_to_semi (cfile);
		return 0;
	}
	option -> code = atoi (val);

	token = next_token (&val, (unsigned *)0, cfile);
	if (token != EQUAL) {
		parse_warn (cfile, "expecting \"=\"");
		skip_to_semi (cfile);
		return 0;
	}

	/* See if this is an array. */
	token = next_token (&val, (unsigned *)0, cfile);
	if (token == ARRAY) {
		token = next_token (&val, (unsigned *)0, cfile);
		if (token != OF) {
			parse_warn (cfile, "expecting \"of\".");
			skip_to_semi (cfile);
			return 0;
		}
		arrayp = 1;
		token = next_token (&val, (unsigned *)0, cfile);
	}

	if (token == LBRACE) {
		recordp = 1;
		token = next_token (&val, (unsigned *)0, cfile);
	}

	/* At this point we're expecting a data type. */
      next_type:
	if (has_encapsulation) {
		parse_warn (cfile,
			    "encapsulate must always be the last item.");
		skip_to_semi (cfile);
		return 0;
	}

	switch (token) {
	      case ARRAY:
		if (arrayp) {
			parse_warn (cfile, "no nested arrays.");
			skip_to_rbrace (cfile, recordp);
			if (recordp)
				skip_to_semi (cfile);
			return 0;
		}
		token = next_token (&val, (unsigned *)0, cfile);
		if (token != OF) {
			parse_warn (cfile, "expecting \"of\".");
			skip_to_semi (cfile);
			return 0;
		}
		arrayp = recordp + 1;
		token = next_token (&val, (unsigned *)0, cfile);
		if ((recordp) && (token == LBRACE)) {
			parse_warn (cfile,
				    "only uniform array inside record.");
			skip_to_rbrace (cfile, recordp + 1);
			skip_to_semi (cfile);
			return 0;
		}
		goto next_type;
	      case BOOLEAN:
		type = 'f';
		break;
	      case INTEGER:
		is_signed = 1;
	      parse_integer:
		token = next_token (&val, (unsigned *)0, cfile);
		if (token != NUMBER) {
			parse_warn (cfile, "expecting number.");
			skip_to_rbrace (cfile, recordp);
			if (recordp)
				skip_to_semi (cfile);
			return 0;
		}
		switch (atoi (val)) {
		      case 8:
			type = is_signed ? 'b' : 'B';
			break;
		      case 16:
			type = is_signed ? 's' : 'S';
			break;
		      case 32:
			type = is_signed ? 'l' : 'L';
			break;
		      default:
			parse_warn (cfile,
				    "%s bit precision is not supported.", val);
			skip_to_rbrace (cfile, recordp);
			if (recordp)
				skip_to_semi (cfile);
			return 0;
		}
		break;
	      case SIGNED:
		is_signed = 1;
	      parse_signed:
		token = next_token (&val, (unsigned *)0, cfile);
		if (token != INTEGER) {
			parse_warn (cfile, "expecting \"integer\" keyword.");
			skip_to_rbrace (cfile, recordp);
			if (recordp)
				skip_to_semi (cfile);
			return 0;
		}
		goto parse_integer;
	      case UNSIGNED:
		is_signed = 0;
		goto parse_signed;

	      case IP_ADDRESS:
		type = 'I';
		break;
	      case DOMAIN_NAME:
		type = 'd';
		goto no_arrays;
	      case TEXT:
		type = 't';
	      no_arrays:
		if (arrayp) {
			parse_warn (cfile, "arrays of text strings not %s",
				    "yet supported.");
			skip_to_rbrace (cfile, recordp);
			if (recordp)
				skip_to_semi (cfile);
			return 0;
		}
		no_more_in_record = 1;
		break;
	      case STRING_TOKEN:
		type = 'X';
		goto no_arrays;

	      case ENCAPSULATE:
		token = next_token (&val, (unsigned *)0, cfile);
		if (!is_identifier (token)) {
			parse_warn (cfile,
				    "expecting option space identifier");
			skip_to_semi (cfile);
			return 0;
		}
		if (strlen (val) + tokix + 2 > sizeof (tokbuf))
			goto toobig;
		tokbuf [tokix++] = 'E';
		strcpy (&tokbuf [tokix], val);
		tokix += strlen (val);
		type = '.';
		has_encapsulation = 1;
		break;

	      default:
		parse_warn (cfile, "unknown data type %s", val);
		skip_to_rbrace (cfile, recordp);
		if (recordp)
			skip_to_semi (cfile);
		return 0;
	}

	if (tokix == sizeof tokbuf) {
	      toobig:
		parse_warn (cfile, "too many types in record.");
		skip_to_rbrace (cfile, recordp);
		if (recordp)
			skip_to_semi (cfile);
		return 0;
	}
	tokbuf [tokix++] = type;

	if (recordp) {
		token = next_token (&val, (unsigned *)0, cfile);
		if (arrayp > recordp) {
			if (tokix == sizeof tokbuf) {
				parse_warn (cfile,
					    "too many types in record.");
				skip_to_rbrace (cfile, 1);
				skip_to_semi (cfile);
				return 0;
			}
			arrayp = 0;
			tokbuf[tokix++] = 'a';
		}
		if (token == COMMA) {
			if (no_more_in_record) {
				parse_warn (cfile,
					    "%s must be at end of record.",
					    type == 't' ? "text" : "string");
				skip_to_rbrace (cfile, 1);
				if (recordp)
					skip_to_semi (cfile);
				return 0;
			}
			token = next_token (&val, (unsigned *)0, cfile);
			goto next_type;
		}
		if (token != RBRACE) {
			parse_warn (cfile, "expecting right brace.");
			skip_to_rbrace (cfile, 1);
			if (recordp)
				skip_to_semi (cfile);
			return 0;
		}
	}
	if (!parse_semi (cfile)) {
		parse_warn (cfile, "semicolon expected.");
		skip_to_semi (cfile);
		if (recordp)
			skip_to_semi (cfile);
		return 0;
	}
	if (has_encapsulation && arrayp) {
		parse_warn (cfile,
			    "Arrays of encapsulations don't make sense.");
		return 0;
	}
	if (has_encapsulation && tokbuf [0] == 'E')
		has_encapsulation = 0;
	s = dmalloc (tokix +
		     (arrayp ? 1 : 0) +
		     (has_encapsulation ? 1 : 0) + 1, MDL);
	if (!s)
		log_fatal ("no memory for option format.");
	if (has_encapsulation)
		s [0] = 'e';
	memcpy (s + has_encapsulation, tokbuf, tokix);
	tokix += has_encapsulation;
	if (arrayp)
		s [tokix++] = (arrayp > recordp) ? 'a' : 'A';
	s [tokix] = 0;
	option -> format = s;
	if (option -> universe -> options [option -> code]) {
		/* XXX Free the option, but we can't do that now because they
		   XXX may start out static. */
	}
	option -> universe -> options [option -> code] = option;
	option_hash_add (option -> universe -> hash,
			 (const char *)option -> name,
			 0, option, MDL);
	return 1;
}

/*
 * base64 :== NUMBER_OR_STRING
 */

int parse_base64 (data, cfile)
	struct data_string *data;
	struct parse *cfile;
{
	enum dhcp_token token;
	const char *val;
	int i, j, k;
	unsigned acc = 0;
	static unsigned char
		from64 [] = {64, 64, 64, 64, 64, 64, 64, 64,  /*  \"#$%&' */
			     64, 64, 64, 62, 64, 64, 64, 63,  /* ()*+,-./ */
			     52, 53, 54, 55, 56, 57, 58, 59,  /* 01234567 */
			     60, 61, 64, 64, 64, 64, 64, 64,  /* 89:;<=>? */
			     64, 0, 1, 2, 3, 4, 5, 6,	      /* @ABCDEFG */
			     7, 8, 9, 10, 11, 12, 13, 14,     /* HIJKLMNO */
			     15, 16, 17, 18, 19, 20, 21, 22,  /* PQRSTUVW */
			     23, 24, 25, 64, 64, 64, 64, 64,  /* XYZ[\]^_ */
			     64, 26, 27, 28, 29, 30, 31, 32,  /* 'abcdefg */
			     33, 34, 35, 36, 37, 38, 39, 40,  /* hijklmno */
			     41, 42, 43, 44, 45, 46, 47, 48,  /* pqrstuvw */
			     49, 50, 51, 64, 64, 64, 64, 64}; /* xyz{|}~  */
	struct string_list *bufs = (struct string_list *)0,
			   *last = (struct string_list *)0,
			   *t;
	int cc = 0;
	int terminated = 0;
	
	/* It's possible for a + or a / to cause a base64 quantity to be
	   tokenized into more than one token, so we have to parse them all
	   in before decoding. */
	do {
		unsigned l;

		token = next_token (&val, &l, cfile);
		t = dmalloc (l + sizeof *t, MDL);
		if (!t)
			log_fatal ("no memory for base64 buffer.");
		memset (t, 0, (sizeof *t) - 1);
		memcpy (t -> string, val, l + 1);
		cc += l;
		if (last)
			last -> next = t;
		else
			bufs = t;
		last = t;
		token = peek_token (&val, (unsigned *)0, cfile);
	} while (token == NUMBER_OR_NAME || token == NAME || token == EQUAL ||
		 token == NUMBER || token == PLUS || token == SLASH ||
		 token == STRING);

	data -> len = cc;
	data -> len = (data -> len * 3) / 4;
	if (!buffer_allocate (&data -> buffer, data -> len, MDL)) {
		parse_warn (cfile, "can't allocate buffer for base64 data.");
		data -> len = 0;
		data -> data = (unsigned char *)0;
		return 0;
	}
		
	j = k = 0;
	for (t = bufs; t; t = t -> next) {
	    for (i = 0; t -> string [i]; i++) {
		unsigned foo = t -> string [i];
		if (terminated && foo != '=') {
			parse_warn (cfile,
				    "stuff after base64 '=' terminator: %s.",
				    &t -> string [i]);
			goto bad;
		}
		if (foo < ' ' || foo > 'z') {
		      bad64:
			parse_warn (cfile,
				    "invalid base64 character %d.",
				    t -> string [i]);
		      bad:
			data_string_forget (data, MDL);
			goto out;
		}
		if (foo == '=')
			terminated = 1;
		else {
			foo = from64 [foo - ' '];
			if (foo == 64)
				goto bad64;
			acc = (acc << 6) + foo;
			switch (k % 4) {
			      case 0:
				break;
			      case 1:
				data -> buffer -> data [j++] = (acc >> 4);
				acc = acc & 0x0f;
				break;
				
			      case 2:
				data -> buffer -> data [j++] = (acc >> 2);
				acc = acc & 0x03;
				break;
			      case 3:
				data -> buffer -> data [j++] = acc;
				acc = 0;
				break;
			}
		}
		k++;
	    }
	}
	if (k % 4) {
		if (acc) {
			parse_warn (cfile,
				    "partial base64 value left over: %d.",
				    acc);
		}
	}
	data -> len = j;
	data -> data = data -> buffer -> data;
      out:
	for (t = bufs; t; t = last) {
		last = t -> next;
		dfree (t, MDL);
	}
	if (data -> len)
		return 1;
	else
		return 0;
}


/*
 * colon-seperated-hex-list :== NUMBER |
 *				NUMBER COLON colon-seperated-hex-list
 */

int parse_cshl (data, cfile)
	struct data_string *data;
	struct parse *cfile;
{
	u_int8_t ibuf [128];
	unsigned ilen = 0;
	unsigned tlen = 0;
	struct option_tag *sl = (struct option_tag *)0;
	struct option_tag *next, **last = &sl;
	enum dhcp_token token;
	const char *val;
	unsigned char *rvp;

	do {
		token = next_token (&val, (unsigned *)0, cfile);
		if (token != NUMBER && token != NUMBER_OR_NAME) {
			parse_warn (cfile, "expecting hexadecimal number.");
			skip_to_semi (cfile);
			for (; sl; sl = next) {
				next = sl -> next;
				dfree (sl, MDL);
			}
			return 0;
		}
		if (ilen == sizeof ibuf) {
			next = (struct option_tag *)
				dmalloc (ilen - 1 +
					 sizeof (struct option_tag), MDL);
			if (!next)
				log_fatal ("no memory for string list.");
			memcpy (next -> data, ibuf, ilen);
			*last = next;
			last = &next -> next;
			tlen += ilen;
			ilen = 0;
		}
		convert_num (cfile, &ibuf [ilen++], val, 16, 8);

		token = peek_token (&val, (unsigned *)0, cfile);
		if (token != COLON)
			break;
		token = next_token (&val, (unsigned *)0, cfile);
	} while (1);

	if (!buffer_allocate (&data -> buffer, tlen + ilen, MDL))
		log_fatal ("no memory to store octet data.");
	data -> data = &data -> buffer -> data [0];
	data -> len = tlen + ilen;
	data -> terminated = 0;

	rvp = &data -> buffer -> data [0];
	while (sl) {
		next = sl -> next;
		memcpy (rvp, sl -> data, sizeof ibuf);
		rvp += sizeof ibuf;
		dfree (sl, MDL);
		sl = next;
	}
	
	memcpy (rvp, ibuf, ilen);
	return 1;
}

/*
 * executable-statements :== executable-statement executable-statements |
 *			     executable-statement
 *
 * executable-statement :==
 *	IF if-statement |
 * 	ADD class-name SEMI |
 *	BREAK SEMI |
 *	OPTION option-parameter SEMI |
 *	SUPERSEDE option-parameter SEMI |
 *	PREPEND option-parameter SEMI |
 *	APPEND option-parameter SEMI
 */

int parse_executable_statements (statements, cfile, lose, case_context)
	struct executable_statement **statements;
	struct parse *cfile;
	int *lose;
	enum expression_context case_context;
{
	struct executable_statement **next;

	next = statements;
	while (parse_executable_statement (next, cfile, lose, case_context))
		next = &((*next) -> next);
	if (!*lose)
		return 1;
	return 0;
}

int parse_executable_statement (result, cfile, lose, case_context)
	struct executable_statement **result;
	struct parse *cfile;
	int *lose;
	enum expression_context case_context;
{
	enum dhcp_token token;
	const char *val;
	struct executable_statement base;
	struct class *cta;
	struct option *option;
	struct option_cache *cache;
	int known;
	int flag;
	int i;
	struct dns_zone *zone;
	isc_result_t status;
	char *s;

	token = peek_token (&val, (unsigned *)0, cfile);
	switch (token) {
	      case IF:
		next_token (&val, (unsigned *)0, cfile);
		return parse_if_statement (result, cfile, lose);

	      case TOKEN_ADD:
		token = next_token (&val, (unsigned *)0, cfile);
		token = next_token (&val, (unsigned *)0, cfile);
		if (token != STRING) {
			parse_warn (cfile, "expecting class name.");
			skip_to_semi (cfile);
			*lose = 1;
			return 0;
		}
		cta = (struct class *)0;
		status = find_class (&cta, val, MDL);
		if (status != ISC_R_SUCCESS) {
			parse_warn (cfile, "class %s: %s",
				    val, isc_result_totext (status));
			skip_to_semi (cfile);
			*lose = 1;
			return 0;
		}
		if (!parse_semi (cfile)) {
			*lose = 1;
			return 0;
		}
		if (!executable_statement_allocate (result, MDL))
			log_fatal ("no memory for new statement.");
		(*result) -> op = add_statement;
		(*result) -> data.add = cta;
		break;

	      case BREAK:
		token = next_token (&val, (unsigned *)0, cfile);
		if (!parse_semi (cfile)) {
			*lose = 1;
			return 0;
		}
		if (!executable_statement_allocate (result, MDL))
			log_fatal ("no memory for new statement.");
		(*result) -> op = break_statement;
		break;

	      case SEND:
		token = next_token (&val, (unsigned *)0, cfile);
		known = 0;
		option = parse_option_name (cfile, 0, &known);
		if (!option) {
			*lose = 1;
			return 0;
		}
		return parse_option_statement (result, cfile, 1, option,
					       send_option_statement);

	      case SUPERSEDE:
	      case OPTION:
		token = next_token (&val, (unsigned *)0, cfile);
		known = 0;
		option = parse_option_name (cfile, 0, &known);
		if (!option) {
			*lose = 1;
			return 0;
		}
		return parse_option_statement (result, cfile, 1, option,
					       supersede_option_statement);

	      case ALLOW:
		flag = 1;
		goto pad;
	      case DENY:
		flag = 0;
		goto pad;
	      case IGNORE:
		flag = 2;
	      pad:
		token = next_token (&val, (unsigned *)0, cfile);
		cache = (struct option_cache *)0;
		if (!parse_allow_deny (&cache, cfile, flag))
			return 0;
		if (!executable_statement_allocate (result, MDL))
			log_fatal ("no memory for new statement.");
		(*result) -> op = supersede_option_statement;
		(*result) -> data.option = cache;
		break;

	      case DEFAULT:
		token = next_token (&val, (unsigned *)0, cfile);
		token = peek_token (&val, (unsigned *)0, cfile);
		if (token == COLON)
			goto switch_default;
		known = 0;
		option = parse_option_name (cfile, 0, &known);
		if (!option) {
			*lose = 1;
			return 0;
		}
		return parse_option_statement (result, cfile, 1, option,
					       default_option_statement);

	      case PREPEND:
		token = next_token (&val, (unsigned *)0, cfile);
		known = 0;
		option = parse_option_name (cfile, 0, &known);
		if (!option) {
			*lose = 1;
			return 0;
		}
		return parse_option_statement (result, cfile, 1, option,
					       prepend_option_statement);

	      case APPEND:
		token = next_token (&val, (unsigned *)0, cfile);
		known = 0;
		option = parse_option_name (cfile, 0, &known);
		if (!option) {
			*lose = 1;
			return 0;
		}
		return parse_option_statement (result, cfile, 1, option,
					       append_option_statement);

	      case ON:
		token = next_token (&val, (unsigned *)0, cfile);
		return parse_on_statement (result, cfile, lose);
			
	      case SWITCH:
		token = next_token (&val, (unsigned *)0, cfile);
		return parse_switch_statement (result, cfile, lose);

	      case CASE:
		token = next_token (&val, (unsigned *)0, cfile);
		if (case_context == context_any) {
			parse_warn (cfile,
				    "case statement in inappropriate scope.");
			*lose = 1;
			skip_to_semi (cfile);
			return 0;
		}
		return parse_case_statement (result,
					     cfile, lose, case_context);

	      switch_default:
		token = next_token (&val, (unsigned *)0, cfile);
		if (case_context == context_any) {
			parse_warn (cfile, "switch default statement in %s",
				    "inappropriate scope.");
		
			*lose = 1;
			return 0;
		} else {
			if (!executable_statement_allocate (result, MDL))
				log_fatal ("no memory for default statement.");
			(*result) -> op = default_statement;
			return 1;
		}
			
	      case DEFINE:
	      case TOKEN_SET:
		token = next_token (&val, (unsigned *)0, cfile);
		if (token == DEFINE)
			flag = 1;
		else
			flag = 0;

		token = next_token (&val, (unsigned *)0, cfile);
		if (token != NAME && token != NUMBER_OR_NAME) {
			parse_warn (cfile,
				    "%s can't be a variable name", val);
		      badset:
			skip_to_semi (cfile);
			*lose = 1;
			return 0;
		}

		if (!executable_statement_allocate (result, MDL))
			log_fatal ("no memory for set statement.");
		(*result) -> op = flag ? define_statement : set_statement;
		(*result) -> data.set.name = dmalloc (strlen (val) + 1, MDL);
		if (!(*result)->data.set.name)
			log_fatal ("can't allocate variable name");
		strcpy ((*result) -> data.set.name, val);
		token = next_token (&val, (unsigned *)0, cfile);

		if (token == LPAREN) {
			struct string_list *head, *cur, *new;
			struct expression *expr;
			head = cur = (struct string_list *)0;
			do {
				token = next_token (&val,
						    (unsigned *)0, cfile);
				if (token == RPAREN)
					break;
				if (token != NAME && token != NUMBER_OR_NAME) {
					parse_warn (cfile,
						    "expecting argument name");
					skip_to_rbrace (cfile, 0);
					*lose = 1;
					executable_statement_dereference
						(result, MDL);
					return 0;
				}
				new = ((struct string_list *)
				       dmalloc (sizeof (struct string_list) +
						strlen (val), MDL));
				if (!new)
					log_fatal ("can't allocate string.");
				memset (new, 0, sizeof *new);
				strcpy (new -> string, val);
				if (cur) {
					cur -> next = new;
					cur = new;
				} else {
					head = cur = new;
				}
				token = next_token (&val,
						    (unsigned *)0, cfile);
			} while (token == COMMA);

			if (token != RPAREN) {
				parse_warn (cfile, "expecting right paren.");
			      badx:
				skip_to_semi (cfile);
				*lose = 1;
				executable_statement_dereference (result, MDL);
				return 0;
			}

			token = next_token (&val, (unsigned *)0, cfile);
			if (token != LBRACE) {
				parse_warn (cfile, "expecting left brace.");
				goto badx;
			}

			expr = (struct expression *)0;
			if (!(expression_allocate (&expr, MDL)))
				log_fatal ("can't allocate expression.");
			expr -> op = expr_function;
			if (!fundef_allocate (&expr -> data.func, MDL))
				log_fatal ("can't allocate fundef.");
			expr -> data.func -> args = head;
			(*result) -> data.set.expr = expr;

			if (!(parse_executable_statements
			      (&expr -> data.func -> statements, cfile, lose,
			       case_context))) {
				if (*lose)
					goto badx;
			}

			token = next_token (&val, (unsigned *)0, cfile);
			if (token != RBRACE) {
				parse_warn (cfile, "expecting rigt brace.");
				goto badx;
			}
		} else {
			if (token != EQUAL) {
				parse_warn (cfile,
					    "expecting '=' in %s statement.",
					    flag ? "define" : "set");
				goto badset;
			}

			if (!parse_expression (&(*result) -> data.set.expr,
					       cfile, lose, context_any,
					       (struct expression **)0,
					       expr_none)) {
				if (!*lose)
					parse_warn (cfile,
						    "expecting expression.");
				else
					*lose = 1;
				skip_to_semi (cfile);
				executable_statement_dereference (result, MDL);
				return 0;
			}
			if (!parse_semi (cfile)) {
				*lose = 1;
				executable_statement_dereference (result, MDL);
				return 0;
			}
		}
		break;

	      case UNSET:
		token = next_token (&val, (unsigned *)0, cfile);

		token = next_token (&val, (unsigned *)0, cfile);
		if (token != NAME && token != NUMBER_OR_NAME) {
			parse_warn (cfile,
				    "%s can't be a variable name", val);
		      badunset:
			skip_to_semi (cfile);
			*lose = 1;
			return 0;
		}

		if (!executable_statement_allocate (result, MDL))
			log_fatal ("no memory for set statement.");
		(*result) -> op = unset_statement;
		(*result) -> data.unset = dmalloc (strlen (val) + 1, MDL);
		if (!(*result)->data.unset)
			log_fatal ("can't allocate variable name");
		strcpy ((*result) -> data.unset, val);
		if (!parse_semi (cfile)) {
			*lose = 1;
			executable_statement_dereference (result, MDL);
			return 0;
		}
		break;

	      case EVAL:
		token = next_token (&val, (unsigned *)0, cfile);

		if (!executable_statement_allocate (result, MDL))
			log_fatal ("no memory for eval statement.");
		(*result) -> op = eval_statement;

		if (!parse_expression (&(*result) -> data.eval,
				       cfile, lose, context_data, /* XXX */
				       (struct expression **)0, expr_none)) {
			if (!*lose)
				parse_warn (cfile,
					    "expecting data expression.");
			else
				*lose = 1;
			skip_to_semi (cfile);
			executable_statement_dereference (result, MDL);
			return 0;
		}
		if (!parse_semi (cfile)) {
			*lose = 1;
			executable_statement_dereference (result, MDL);
		}
		break;

	      case RETURN:
		token = next_token (&val, (unsigned *)0, cfile);

		if (!executable_statement_allocate (result, MDL))
			log_fatal ("no memory for return statement.");
		(*result) -> op = return_statement;

		if (!parse_expression (&(*result) -> data.retval,
				       cfile, lose, context_data,
				       (struct expression **)0, expr_none)) {
			if (!*lose)
				parse_warn (cfile,
					    "expecting data expression.");
			else
				*lose = 1;
			skip_to_semi (cfile);
			executable_statement_dereference (result, MDL);
			return 0;
		}
		if (!parse_semi (cfile)) {
			*lose = 1;
			executable_statement_dereference (result, MDL);
			return 0;
		}
		break;

	      case LOG:
		token = next_token (&val, (unsigned *)0, cfile);

		if (!executable_statement_allocate (result, MDL))
			log_fatal ("no memory for log statement.");
		(*result) -> op = log_statement;

		token = next_token (&val, (unsigned *)0, cfile);
		if (token != LPAREN) {
			parse_warn (cfile, "left parenthesis expected.");
			skip_to_semi (cfile);
			*lose = 1;
			return 0;
		}

		token = peek_token (&val, (unsigned *)0, cfile);
		i = 1;
		if (token == FATAL) {
			(*result) -> data.log.priority = log_priority_fatal;
		} else if (token == ERROR) {
			(*result) -> data.log.priority = log_priority_error;
		} else if (token == TOKEN_DEBUG) {
			(*result) -> data.log.priority = log_priority_debug;
		} else if (token == INFO) {
			(*result) -> data.log.priority = log_priority_info;
		} else {
			(*result) -> data.log.priority = log_priority_debug;
			i = 0;
		}
		if (i) {
			token = next_token (&val, (unsigned *)0, cfile);
			token = next_token (&val, (unsigned *)0, cfile);
			if (token != COMMA) {
				parse_warn (cfile, "comma expected.");
				skip_to_semi (cfile);
				*lose = 1;
				return 0;
			}
		}

		if (!(parse_data_expression
		      (&(*result) -> data.log.expr, cfile, lose))) {
			skip_to_semi (cfile);
			*lose = 1;
			return 0;
		}

		token = next_token (&val, (unsigned *)0, cfile);
		if (token != RPAREN) {
			parse_warn (cfile, "right parenthesis expected.");
			skip_to_semi (cfile);
			*lose = 1;
			return 0;
		}

		token = next_token (&val, (unsigned *)0, cfile);
		if (token != SEMI) {
			parse_warn (cfile, "semicolon expected.");
			skip_to_semi (cfile);
			*lose = 1;
			return 0;
		}
		break;
			
		/* Not really a statement, but we parse it here anyway
		   because it's appropriate for all DHCP agents with
		   parsers. */
	      case ZONE:
		token = next_token (&val, (unsigned *)0, cfile);
		zone = (struct dns_zone *)0;
		if (!dns_zone_allocate (&zone, MDL))
			log_fatal ("no memory for new zone.");
		zone -> name = parse_host_name (cfile);
		if (!zone -> name) {
			parse_warn (cfile, "expecting hostname.");
		      badzone:
			*lose = 1;
			skip_to_semi (cfile);
			dns_zone_dereference (&zone, MDL);
			return 0;
		}
		i = strlen (zone -> name);
		if (zone -> name [i - 1] != '.') {
			s = dmalloc ((unsigned)i + 2, MDL);
			if (!s) {
				parse_warn (cfile, "no trailing '.' on zone");
				goto badzone;
			}
			strcpy (s, zone -> name);
			s [i] = '.';
			s [i + 1] = 0;
			dfree (zone -> name, MDL);
			zone -> name = s;
		}
		if (!parse_zone (zone, cfile))
			goto badzone;
		status = enter_dns_zone (zone);
		if (status != ISC_R_SUCCESS) {
			parse_warn (cfile, "dns zone key %s: %s",
				    zone -> name, isc_result_totext (status));
			dns_zone_dereference (&zone, MDL);
			return 0;
		}
		dns_zone_dereference (&zone, MDL);
		return 1;
		
		/* Also not really a statement, but same idea as above. */
	      case KEY:
		token = next_token (&val, (unsigned *)0, cfile);
		if (!parse_key (cfile)) {
			*lose = 1;
			return 0;
		}
		return 1;

	      default:
		if (config_universe && is_identifier (token)) {
			option = (struct option *)0;
			option_hash_lookup (&option, config_universe -> hash,
					    val, 0, MDL);
			if (option) {
				token = next_token (&val,
						    (unsigned *)0, cfile);
				return parse_option_statement
					(result, cfile, 1, option,
					 supersede_option_statement);
			}
		}

		if (token == NUMBER_OR_NAME || token == NAME) {
			/* This is rather ugly.  Since function calls are
			   data expressions, fake up an eval statement. */
			if (!executable_statement_allocate (result, MDL))
				log_fatal ("no memory for eval statement.");
			(*result) -> op = eval_statement;

			if (!parse_expression (&(*result) -> data.eval,
					       cfile, lose, context_data,
					       (struct expression **)0,
					       expr_none)) {
				if (!*lose)
					parse_warn (cfile, "expecting "
						    "function call.");
				else
					*lose = 1;
				skip_to_semi (cfile);
				executable_statement_dereference (result, MDL);
				return 0;
			}
			if (!parse_semi (cfile)) {
				*lose = 1;
				executable_statement_dereference (result, MDL);
				return 0;
			}
			break;
		}

		*lose = 0;
		return 0;
	}

	return 1;
}

/* zone-statements :== zone-statement |
		       zone-statement zone-statements
   zone-statement :==
	PRIMARY ip-addresses SEMI |
	SECONDARY ip-addresses SEMI |
	key-reference SEMI
   ip-addresses :== ip-addr-or-hostname |
		  ip-addr-or-hostname COMMA ip-addresses
   key-reference :== KEY STRING |
		    KEY identifier */

int parse_zone (struct dns_zone *zone, struct parse *cfile)
{
	int token;
	const char *val;
	char *key_name;
	struct option_cache *oc;
	int done = 0;

	token = next_token (&val, (unsigned *)0, cfile);
	if (token != LBRACE) {
		parse_warn (cfile, "expecting left brace");
		return 0;
	}

	do {
	    token = peek_token (&val, (unsigned *)0, cfile);
	    switch (token) {
		  case PRIMARY:
		    if (zone -> primary) {
			    parse_warn (cfile,
					"more than one primary.");
			    skip_to_semi (cfile);
			    return 0;
		    }
		    if (!option_cache_allocate (&zone -> primary, MDL))
			    log_fatal ("can't allocate primary option cache.");
		    oc = zone -> primary;
		    goto consemup;
		    
		  case SECONDARY:
		    if (zone -> secondary) {
			    parse_warn (cfile, "more than one secondary.");
			skip_to_semi (cfile);
			return 0;
		    }
		    if (!option_cache_allocate (&zone -> secondary, MDL))
			    log_fatal ("can't allocate secondary.");
		    oc = zone -> secondary;
		  consemup:
		    token = next_token (&val, (unsigned *)0, cfile);
		    do {
			    struct expression *expr = (struct expression *)0;
			    if (!parse_ip_addr_or_hostname (&expr, cfile, 0)) {
				parse_warn (cfile,
					    "expecting IP addr or hostname.");
				skip_to_semi (cfile);
				return 0;
			    }
			    if (oc -> expression) {
				    struct expression *old =
					    (struct expression *)0;
				    expression_reference (&old,
							  oc -> expression,
							  MDL);
				    expression_dereference (&oc -> expression,
							    MDL);
				    if (!make_concat (&oc -> expression,
						      old, expr))
					log_fatal ("no memory for concat.");
				    expression_dereference (&expr, MDL);
				    expression_dereference (&old, MDL);
			    } else {
				    expression_reference (&oc -> expression,
							  expr, MDL);
				    expression_dereference (&expr, MDL);
			    }
			    token = next_token (&val, (unsigned *)0, cfile);
		    } while (token == COMMA);
		    if (token != SEMI) {
			    parse_warn (cfile, "expecting semicolon.");
			    skip_to_semi (cfile);
			    return 0;
		    }
		    break;

		  case KEY:
		    token = next_token (&val, (unsigned *)0, cfile);
		    token = peek_token (&val, (unsigned *)0, cfile);
		    if (token == STRING) {
			    token = next_token (&val, (unsigned *)0, cfile);
			    key_name = (char *)0;
		    } else {
			    key_name = parse_host_name (cfile);
			    if (!key_name) {
				    parse_warn (cfile, "expecting key name.");
				    skip_to_semi (cfile);
				    return 0;
			    }
			    val = key_name;
		    }
		    if (omapi_auth_key_lookup_name (&zone -> key, val) !=
			ISC_R_SUCCESS)
			    parse_warn (cfile, "unknown key %s", val);
		    if (key_name)
			    dfree (key_name, MDL);
		    if (!parse_semi (cfile))
			    return 0;
		    break;
		    
		  default:
		    done = 1;
		    break;
	    }
	} while (!done);

	token = next_token (&val, (unsigned *)0, cfile);
	if (token != RBRACE) {
		parse_warn (cfile, "expecting right brace.");
		return 0;
	}
	return 1;
}

/* key-statements :== key-statement |
		      key-statement key-statements
   key-statement :==
	ALGORITHM host-name SEMI |
	secret-definition SEMI
   secret-definition :== SECRET base64val |
			 SECRET STRING */

int parse_key (struct parse *cfile)
{
	int token;
	const char *val;
	int done = 0;
	struct auth_key *key;
	struct data_string ds;
	isc_result_t status;
	char *s;

	key = (struct auth_key *)0;
	if (omapi_auth_key_new (&key, MDL) != ISC_R_SUCCESS)
		log_fatal ("no memory for key");

	token = peek_token (&val, (unsigned *)0, cfile);
	if (token == STRING) {
		token = next_token (&val, (unsigned *)0, cfile);
		key -> name = dmalloc (strlen (val) + 1, MDL);
		if (!key -> name)
			log_fatal ("no memory for key name.");
		strcpy (key -> name, val);

	} else {
		key -> name = parse_host_name (cfile);
		if (!key -> name) {
			parse_warn (cfile, "expecting key name.");
			skip_to_semi (cfile);
			goto bad;
		}
	}

	token = next_token (&val, (unsigned *)0, cfile);
	if (token != LBRACE) {
		parse_warn (cfile, "expecting left brace");
		goto bad;
	}

	do {
		token = next_token (&val, (unsigned *)0, cfile);
		switch (token) {
		      case ALGORITHM:
			if (key -> algorithm) {
				parse_warn (cfile,
					    "key %s: too many algorithms",
					    key -> name);
				goto rbad;
			}
			key -> algorithm = parse_host_name (cfile);
			if (!key -> algorithm) {
				parse_warn (cfile,
					    "expecting key algorithm name.");
				goto rbad;
			}
			if (!parse_semi (cfile))
				goto rbad;
			/* If the algorithm name isn't an FQDN, tack on
			   the .SIG-ALG.REG.NET. domain. */
			s = strrchr (key -> algorithm, '.');
			if (!s) {
			    static char add [] = ".SIG-ALG.REG.INT.";
			    s = dmalloc (strlen (key -> algorithm) +
					 sizeof (add), MDL);
			    if (!s) {
				log_error ("no memory for key %s.",
					   "algorithm");
				goto rbad;
			    }
			    strcpy (s, key -> algorithm);
			    strcat (s, add);
			    dfree (key -> algorithm, MDL);
			    key -> algorithm = s;
			} else if (s [1]) {
			    /* If there is no trailing '.', hack one in. */
			    s = dmalloc (strlen (key -> algorithm) + 2, MDL);
			    if (!s) {
				    log_error ("no memory for key %s.",
					       key -> algorithm);
				    goto rbad;
			    }
			    strcpy (s, key -> algorithm);
			    strcat (s, ".");
			    dfree (key -> algorithm, MDL);
			    key -> algorithm = s;
			}
			break;

		      case SECRET:
			if (key -> key) {
				parse_warn (cfile, "key %s: too many secrets",
					    key -> name);
				goto rbad;
			}

			memset (&ds, 0, sizeof(ds));
			if (!parse_base64 (&ds, cfile))
				goto rbad;
			status = omapi_data_string_new (&key -> key, ds.len,
							MDL);
			if (status != ISC_R_SUCCESS)
				goto rbad;
			memcpy (key -> key -> value,
				ds.buffer -> data, ds.len);
			data_string_forget (&ds, MDL);

			if (!parse_semi (cfile))
				goto rbad;
			break;

		      default:
			done = 1;
			break;
		}
	} while (!done);
	if (token != RBRACE) {
		parse_warn (cfile, "expecting right brace.");
		goto rbad;
	}
	/* Allow the BIND 8 syntax, which has a semicolon after each
	   closing brace. */
	token = peek_token (&val, (unsigned *)0, cfile);
	if (token == SEMI)
		token = next_token (&val, (unsigned *)0, cfile);

	/* Remember the key. */
	status = omapi_auth_key_enter (key);
	if (status != ISC_R_SUCCESS) {
		parse_warn (cfile, "tsig key %s: %s",
			    key -> name, isc_result_totext (status));
		goto bad;
	}
	omapi_auth_key_dereference (&key, MDL);
	return 1;

      rbad:
	skip_to_rbrace (cfile, 1);
      bad:
	omapi_auth_key_dereference (&key, MDL);
	return 0;
}

/*
 * on-statement :== event-types LBRACE executable-statements RBRACE
 * event-types :== event-type OR event-types |
 *		   event-type
 * event-type :== EXPIRY | COMMIT | RELEASE
 */

int parse_on_statement (result, cfile, lose)
	struct executable_statement **result;
	struct parse *cfile;
	int *lose;
{
	enum dhcp_token token;
	const char *val;

	if (!executable_statement_allocate (result, MDL))
		log_fatal ("no memory for new statement.");
	(*result) -> op = on_statement;

	do {
		token = next_token (&val, (unsigned *)0, cfile);
		switch (token) {
		      case EXPIRY:
			(*result) -> data.on.evtypes |= ON_EXPIRY;
			break;
		
		      case COMMIT:
			(*result) -> data.on.evtypes |= ON_COMMIT;
			break;
			
		      case RELEASE:
			(*result) -> data.on.evtypes |= ON_RELEASE;
			break;
			
		      case TRANSMISSION:
			(*result) -> data.on.evtypes |= ON_TRANSMISSION;
			break;

		      default:
			parse_warn (cfile, "expecting a lease event type");
			skip_to_semi (cfile);
			*lose = 1;
			executable_statement_dereference (result, MDL);
			return 0;
		}
		token = next_token (&val, (unsigned *)0, cfile);
	} while (token == OR);
		
	/* Semicolon means no statements. */
	if (token == SEMI)
		return 1;

	if (token != LBRACE) {
		parse_warn (cfile, "left brace expected.");
		skip_to_semi (cfile);
		*lose = 1;
		executable_statement_dereference (result, MDL);
		return 0;
	}
	if (!parse_executable_statements (&(*result) -> data.on.statements,
					  cfile, lose, context_any)) {
		if (*lose) {
			/* Try to even things up. */
			do {
				token = next_token (&val,
						    (unsigned *)0, cfile);
			} while (token != END_OF_FILE && token != RBRACE);
			executable_statement_dereference (result, MDL);
			return 0;
		}
	}
	token = next_token (&val, (unsigned *)0, cfile);
	if (token != RBRACE) {
		parse_warn (cfile, "right brace expected.");
		skip_to_semi (cfile);
		*lose = 1;
		executable_statement_dereference (result, MDL);
		return 0;
	}
	return 1;
}

/*
 * switch-statement :== LPAREN expr RPAREN LBRACE executable-statements RBRACE
 *
 */

int parse_switch_statement (result, cfile, lose)
	struct executable_statement **result;
	struct parse *cfile;
	int *lose;
{
	enum dhcp_token token;
	const char *val;

	if (!executable_statement_allocate (result, MDL))
		log_fatal ("no memory for new statement.");
	(*result) -> op = switch_statement;

	token = next_token (&val, (unsigned *)0, cfile);
	if (token != LPAREN) {
		parse_warn (cfile, "expecting left brace.");
	      pfui:
		*lose = 1;
		skip_to_semi (cfile);
	      gnorf:
		executable_statement_dereference (result, MDL);
		return 0;
	}

	if (!parse_expression (&(*result) -> data.s_switch.expr,
			       cfile, lose, context_data_or_numeric,
			       (struct expression **)0, expr_none)) {
		if (!*lose) {
			parse_warn (cfile,
				    "expecting data or numeric expression.");
			goto pfui;
		}
		goto gnorf;
	}

	token = next_token (&val, (unsigned *)0, cfile);
	if (token != RPAREN) {
		parse_warn (cfile, "right paren expected.");
		goto pfui;
	}

	token = next_token (&val, (unsigned *)0, cfile);
	if (token != LBRACE) {
		parse_warn (cfile, "left brace expected.");
		goto pfui;
	}
	if (!(parse_executable_statements
	      (&(*result) -> data.s_switch.statements, cfile, lose,
	       (is_data_expression ((*result) -> data.s_switch.expr)
		? context_data : context_numeric)))) {
		if (*lose) {
			skip_to_rbrace (cfile, 1);
			executable_statement_dereference (result, MDL);
			return 0;
		}
	}
	token = next_token (&val, (unsigned *)0, cfile);
	if (token != RBRACE) {
		parse_warn (cfile, "right brace expected.");
		goto pfui;
	}
	return 1;
}

/*
 * case-statement :== CASE expr COLON
 *
 */

int parse_case_statement (result, cfile, lose, case_context)
	struct executable_statement **result;
	struct parse *cfile;
	int *lose;
	enum expression_context case_context;
{
	enum dhcp_token token;
	const char *val;

	if (!executable_statement_allocate (result, MDL))
		log_fatal ("no memory for new statement.");
	(*result) -> op = case_statement;

	if (!parse_expression (&(*result) -> data.c_case,
			       cfile, lose, case_context,
			       (struct expression **)0, expr_none))
	{
		if (!*lose) {
			parse_warn (cfile, "expecting %s expression.",
				    (case_context == context_data
				     ? "data" : "numeric"));
		}
	      pfui:
		*lose = 1;
		skip_to_semi (cfile);
		executable_statement_dereference (result, MDL);
		return 0;
	}

	token = next_token (&val, (unsigned *)0, cfile);
	if (token != COLON) {
		parse_warn (cfile, "colon expected.");
		goto pfui;
	}
	return 1;
}

/*
 * if-statement :== boolean-expression LBRACE executable-statements RBRACE
 *						else-statement
 *
 * else-statement :== <null> |
 *		      ELSE LBRACE executable-statements RBRACE |
 *		      ELSE IF if-statement |
 *		      ELSIF if-statement
 */

int parse_if_statement (result, cfile, lose)
	struct executable_statement **result;
	struct parse *cfile;
	int *lose;
{
	enum dhcp_token token;
	const char *val;
	int parenp;

	if (!executable_statement_allocate (result, MDL))
		log_fatal ("no memory for if statement.");

	(*result) -> op = if_statement;

	token = peek_token (&val, (unsigned *)0, cfile);
	if (token == LPAREN) {
		parenp = 1;
		next_token (&val, (unsigned *)0, cfile);
	} else
		parenp = 0;


	if (!parse_boolean_expression (&(*result) -> data.ie.expr,
				       cfile, lose)) {
		if (!*lose)
			parse_warn (cfile, "boolean expression expected.");
		executable_statement_dereference (result, MDL);
		*lose = 1;
		return 0;
	}
#if defined (DEBUG_EXPRESSION_PARSE)
	print_expression ("if condition", (*result) -> data.ie.expr);
#endif
	if (parenp) {
		token = next_token (&val, (unsigned *)0, cfile);
		if (token != RPAREN) {
			parse_warn (cfile, "expecting right paren.");
			*lose = 1;
			executable_statement_dereference (result, MDL);
			return 0;
		}
	}
	token = next_token (&val, (unsigned *)0, cfile);
	if (token != LBRACE) {
		parse_warn (cfile, "left brace expected.");
		skip_to_semi (cfile);
		*lose = 1;
		executable_statement_dereference (result, MDL);
		return 0;
	}
	if (!parse_executable_statements (&(*result) -> data.ie.tc,
					  cfile, lose, context_any)) {
		if (*lose) {
			/* Try to even things up. */
			do {
				token = next_token (&val,
						    (unsigned *)0, cfile);
			} while (token != END_OF_FILE && token != RBRACE);
			executable_statement_dereference (result, MDL);
			return 0;
		}
	}
	token = next_token (&val, (unsigned *)0, cfile);
	if (token != RBRACE) {
		parse_warn (cfile, "right brace expected.");
		skip_to_semi (cfile);
		*lose = 1;
		executable_statement_dereference (result, MDL);
		return 0;
	}
	token = peek_token (&val, (unsigned *)0, cfile);
	if (token == ELSE) {
		token = next_token (&val, (unsigned *)0, cfile);
		token = peek_token (&val, (unsigned *)0, cfile);
		if (token == IF) {
			token = next_token (&val, (unsigned *)0, cfile);
			if (!parse_if_statement (&(*result) -> data.ie.fc,
						 cfile, lose)) {
				if (!*lose)
					parse_warn (cfile,
						    "expecting if statement");
				executable_statement_dereference (result, MDL);
				*lose = 1;
				return 0;
			}
		} else if (token != LBRACE) {
			parse_warn (cfile, "left brace or if expected.");
			skip_to_semi (cfile);
			*lose = 1;
			executable_statement_dereference (result, MDL);
			return 0;
		} else {
			token = next_token (&val, (unsigned *)0, cfile);
			if (!(parse_executable_statements
			      (&(*result) -> data.ie.fc,
			       cfile, lose, context_any))) {
				executable_statement_dereference (result, MDL);
				return 0;
			}
			token = next_token (&val, (unsigned *)0, cfile);
			if (token != RBRACE) {
				parse_warn (cfile, "right brace expected.");
				skip_to_semi (cfile);
				*lose = 1;
				executable_statement_dereference (result, MDL);
				return 0;
			}
		}
	} else if (token == ELSIF) {
		token = next_token (&val, (unsigned *)0, cfile);
		if (!parse_if_statement (&(*result) -> data.ie.fc,
					 cfile, lose)) {
			if (!*lose)
				parse_warn (cfile,
					    "expecting conditional.");
			executable_statement_dereference (result, MDL);
			*lose = 1;
			return 0;
		}
	} else
		(*result) -> data.ie.fc = (struct executable_statement *)0;
	
	return 1;
}

/*
 * boolean_expression :== CHECK STRING |
 *  			  NOT boolean-expression |
 *			  data-expression EQUAL data-expression |
 *			  data-expression BANG EQUAL data-expression |
 *			  boolean-expression AND boolean-expression |
 *			  boolean-expression OR boolean-expression
 *			  EXISTS OPTION-NAME
 */
   			  
int parse_boolean_expression (expr, cfile, lose)
	struct expression **expr;
	struct parse *cfile;
	int *lose;
{
	/* Parse an expression... */
	if (!parse_expression (expr, cfile, lose, context_boolean,
			       (struct expression **)0, expr_none))
		return 0;

	if (!is_boolean_expression (*expr) &&
	    (*expr) -> op != expr_variable_reference &&
	    (*expr) -> op != expr_funcall) {
		parse_warn (cfile, "Expecting a boolean expression.");
		*lose = 1;
		expression_dereference (expr, MDL);
		return 0;
	}
	return 1;
}

/*
 * data_expression :== SUBSTRING LPAREN data-expression COMMA
 *					numeric-expression COMMA
 *					numeric-expression RPAREN |
 *		       CONCAT LPAREN data-expression COMMA 
					data-expression RPAREN
 *		       SUFFIX LPAREN data_expression COMMA
 *		       		     numeric-expression RPAREN |
 *		       OPTION option_name |
 *		       HARDWARE |
 *		       PACKET LPAREN numeric-expression COMMA
 *				     numeric-expression RPAREN |
 *		       STRING |
 *		       colon_seperated_hex_list
 */

int parse_data_expression (expr, cfile, lose)
	struct expression **expr;
	struct parse *cfile;
	int *lose;
{
	/* Parse an expression... */
	if (!parse_expression (expr, cfile, lose, context_data,
			       (struct expression **)0, expr_none))
		return 0;

	if (!is_data_expression (*expr) &&
	    (*expr) -> op != expr_variable_reference &&
	    (*expr) -> op != expr_funcall) {
		parse_warn (cfile, "Expecting a data expression.");
		*lose = 1;
		return 0;
	}
	return 1;
}

/*
 * numeric-expression :== EXTRACT_INT LPAREN data-expression
 *					     COMMA number RPAREN |
 *			  NUMBER
 */

int parse_numeric_expression (expr, cfile, lose)
	struct expression **expr;
	struct parse *cfile;
	int *lose;
{
	/* Parse an expression... */
	if (!parse_expression (expr, cfile, lose, context_numeric,
			       (struct expression **)0, expr_none))
		return 0;

	if (!is_numeric_expression (*expr) &&
	    (*expr) -> op != expr_variable_reference &&
	    (*expr) -> op != expr_funcall) {
		parse_warn (cfile, "Expecting a numeric expression.");
		*lose = 1;
		return 0;
	}
	return 1;
}

/*
 * dns-expression :==
 *	UPDATE LPAREN ns-class COMMA ns-type COMMA data-expression COMMA
 *				data-expression COMMA numeric-expression RPAREN
 *	DELETE LPAREN ns-class COMMA ns-type COMMA data-expression COMMA
 *				data-expression RPAREN
 *	EXISTS LPAREN ns-class COMMA ns-type COMMA data-expression COMMA
 *				data-expression RPAREN
 *	NOT EXISTS LPAREN ns-class COMMA ns-type COMMA data-expression COMMA
 *				data-expression RPAREN
 * ns-class :== IN | CHAOS | HS | NUMBER
 * ns-type :== A | PTR | MX | TXT | NUMBER
 */

int parse_dns_expression (expr, cfile, lose)
	struct expression **expr;
	struct parse *cfile;
	int *lose;
{
	/* Parse an expression... */
	if (!parse_expression (expr, cfile, lose, context_dns,
			       (struct expression **)0, expr_none))
		return 0;

	if (!is_dns_expression (*expr) &&
	    (*expr) -> op != expr_variable_reference &&
	    (*expr) -> op != expr_funcall) {
		parse_warn (cfile, "Expecting a dns update subexpression.");
		*lose = 1;
		return 0;
	}
	return 1;
}

/* Parse a subexpression that does not contain a binary operator. */

int parse_non_binary (expr, cfile, lose, context)
	struct expression **expr;
	struct parse *cfile;
	int *lose;
	enum expression_context context;
{
	enum dhcp_token token;
	const char *val;
	struct collection *col;
	struct option *option;
	struct expression *nexp, **ep;
	int known;
	enum expr_op opcode;
	const char *s;
	char *cptr;
	struct executable_statement *stmt;
	int i;
	unsigned long u;
	isc_result_t status, code;
	unsigned len;

	token = peek_token (&val, (unsigned *)0, cfile);

	/* Check for unary operators... */
	switch (token) {
	      case CHECK:
		token = next_token (&val, (unsigned *)0, cfile);
		token = next_token (&val, (unsigned *)0, cfile);
		if (token != STRING) {
			parse_warn (cfile, "string expected.");
			skip_to_semi (cfile);
			*lose = 1;
			return 0;
		}
		for (col = collections; col; col = col -> next)
			if (!strcmp (col -> name, val))
				break;
		if (!col) {
			parse_warn (cfile, "unknown collection.");
			*lose = 1;
			return 0;
		}
		if (!expression_allocate (expr, MDL))
			log_fatal ("can't allocate expression");
		(*expr) -> op = expr_check;
		(*expr) -> data.check = col;
		break;

	      case TOKEN_NOT:
		token = next_token (&val, (unsigned *)0, cfile);
		if (context == context_dns) {
			token = peek_token (&val, (unsigned *)0, cfile);
			goto not_exists;
		}
		if (!expression_allocate (expr, MDL))
			log_fatal ("can't allocate expression");
		(*expr) -> op = expr_not;
		if (!parse_non_binary (&(*expr) -> data.not,
				       cfile, lose, context_boolean)) {
			if (!*lose) {
				parse_warn (cfile, "expression expected");
				skip_to_semi (cfile);
			}
			*lose = 1;
			expression_dereference (expr, MDL);
			return 0;
		}
		if (!is_boolean_expression ((*expr) -> data.not)) {
			*lose = 1;
			parse_warn (cfile, "boolean expression expected");
			skip_to_semi (cfile);
			expression_dereference (expr, MDL);
			return 0;
		}
		break;

	      case LPAREN:
		token = next_token (&val, (unsigned *)0, cfile);
		if (!parse_expression (expr, cfile, lose, context,
				       (struct expression **)0, expr_none)) {
			if (!*lose) {
				parse_warn (cfile, "expression expected");
				skip_to_semi (cfile);
			}
			*lose = 1;
			return 0;
		}
		token = next_token (&val, (unsigned *)0, cfile);
		if (token != RPAREN) {
			*lose = 1;
			parse_warn (cfile, "right paren expected");
			skip_to_semi (cfile);
			return 0;
		}
		break;

	      case EXISTS:
		if (context == context_dns)
			goto ns_exists;
		token = next_token (&val, (unsigned *)0, cfile);
		if (!expression_allocate (expr, MDL))
			log_fatal ("can't allocate expression");
		(*expr) -> op = expr_exists;
		known = 0;
		(*expr) -> data.option = parse_option_name (cfile, 0, &known);
		if (!(*expr) -> data.option) {
			*lose = 1;
			expression_dereference (expr, MDL);
			return 0;
		}
		break;

	      case STATIC:
		token = next_token (&val, (unsigned *)0, cfile);
		if (!expression_allocate (expr, MDL))
			log_fatal ("can't allocate expression");
		(*expr) -> op = expr_static;
		break;

	      case KNOWN:
		token = next_token (&val, (unsigned *)0, cfile);
		if (!expression_allocate (expr, MDL))
			log_fatal ("can't allocate expression");
		(*expr) -> op = expr_known;
		break;

	      case SUBSTRING:
		token = next_token (&val, (unsigned *)0, cfile);
		if (!expression_allocate (expr, MDL))
			log_fatal ("can't allocate expression");
		(*expr) -> op = expr_substring;

		token = next_token (&val, (unsigned *)0, cfile);
		if (token != LPAREN) {
		      nolparen:
			expression_dereference (expr, MDL);
			parse_warn (cfile, "left parenthesis expected.");
			*lose = 1;
			return 0;
		}

		if (!parse_data_expression (&(*expr) -> data.substring.expr,
					    cfile, lose)) {
		      nodata:
			expression_dereference (expr, MDL);
			if (!*lose) {
				parse_warn (cfile,
					    "expecting data expression.");
				skip_to_semi (cfile);
				*lose = 1;
			}
			return 0;
		}

		token = next_token (&val, (unsigned *)0, cfile);
		if (token != COMMA) {
		      nocomma:
			expression_dereference (expr, MDL);
			parse_warn (cfile, "comma expected.");
			*lose = 1;

			return 0;
		}

		if (!parse_numeric_expression
		    (&(*expr) -> data.substring.offset,cfile, lose)) {
		      nonum:
			if (!*lose) {
				parse_warn (cfile,
					    "expecting numeric expression.");
				skip_to_semi (cfile);
				*lose = 1;
			}
			expression_dereference (expr, MDL);
			return 0;
		}

		token = next_token (&val, (unsigned *)0, cfile);
		if (token != COMMA)
			goto nocomma;

		if (!parse_numeric_expression
		    (&(*expr) -> data.substring.len, cfile, lose))
			goto nonum;

		token = next_token (&val, (unsigned *)0, cfile);
		if (token != RPAREN) {
		      norparen:
			parse_warn (cfile, "right parenthesis expected.");
			*lose = 1;
			expression_dereference (expr, MDL);
			return 0;
		}
		break;

	      case SUFFIX:
		token = next_token (&val, (unsigned *)0, cfile);
		if (!expression_allocate (expr, MDL))
			log_fatal ("can't allocate expression");
		(*expr) -> op = expr_suffix;

		token = next_token (&val, (unsigned *)0, cfile);
		if (token != LPAREN)
			goto nolparen;

		if (!parse_data_expression (&(*expr) -> data.suffix.expr,
					    cfile, lose))
			goto nodata;

		token = next_token (&val, (unsigned *)0, cfile);
		if (token != COMMA)
			goto nocomma;

		if (!parse_numeric_expression (&(*expr) -> data.suffix.len,
					       cfile, lose))
			goto nonum;

		token = next_token (&val, (unsigned *)0, cfile);
		if (token != RPAREN)
			goto norparen;
		break;

	      case CONCAT:
		token = next_token (&val, (unsigned *)0, cfile);
		if (!expression_allocate (expr, MDL))
			log_fatal ("can't allocate expression");
		(*expr) -> op = expr_concat;

		token = next_token (&val, (unsigned *)0, cfile);
		if (token != LPAREN)
			goto nolparen;

		if (!parse_data_expression (&(*expr) -> data.concat [0],
					    cfile, lose))
			goto nodata;

		token = next_token (&val, (unsigned *)0, cfile);
		if (token != COMMA)
			goto nocomma;

	      concat_another:
		if (!parse_data_expression (&(*expr) -> data.concat [1],
					    cfile, lose))
			goto nodata;

		token = next_token (&val, (unsigned *)0, cfile);

		if (token == COMMA) {
			nexp = (struct expression *)0;
			if (!expression_allocate (&nexp, MDL))
				log_fatal ("can't allocate at CONCAT2");
			nexp -> op = expr_concat;
			expression_reference (&nexp -> data.concat [0],
					      *expr, MDL);
			expression_dereference (expr, MDL);
			expression_reference (expr, nexp, MDL);
			expression_dereference (&nexp, MDL);
			goto concat_another;
		}

		if (token != RPAREN)
			goto norparen;
		break;

	      case BINARY_TO_ASCII:
		token = next_token (&val, (unsigned *)0, cfile);
		if (!expression_allocate (expr, MDL))
			log_fatal ("can't allocate expression");
		(*expr) -> op = expr_binary_to_ascii;

		token = next_token (&val, (unsigned *)0, cfile);
		if (token != LPAREN)
			goto nolparen;

		if (!parse_numeric_expression (&(*expr) -> data.b2a.base,
					       cfile, lose))
			goto nodata;

		token = next_token (&val, (unsigned *)0, cfile);
		if (token != COMMA)
			goto nocomma;

		if (!parse_numeric_expression (&(*expr) -> data.b2a.width,
					       cfile, lose))
			goto nodata;

		token = next_token (&val, (unsigned *)0, cfile);
		if (token != COMMA)
			goto nocomma;

		if (!parse_data_expression (&(*expr) -> data.b2a.seperator,
					    cfile, lose))
			goto nodata;

		token = next_token (&val, (unsigned *)0, cfile);
		if (token != COMMA)
			goto nocomma;

		if (!parse_data_expression (&(*expr) -> data.b2a.buffer,
					    cfile, lose))
			goto nodata;

		token = next_token (&val, (unsigned *)0, cfile);
		if (token != RPAREN)
			goto norparen;
		break;

	      case REVERSE:
		token = next_token (&val, (unsigned *)0, cfile);
		if (!expression_allocate (expr, MDL))
			log_fatal ("can't allocate expression");
		(*expr) -> op = expr_reverse;

		token = next_token (&val, (unsigned *)0, cfile);
		if (token != LPAREN)
			goto nolparen;

		if (!(parse_numeric_expression
		      (&(*expr) -> data.reverse.width, cfile, lose)))
			goto nodata;

		token = next_token (&val, (unsigned *)0, cfile);
		if (token != COMMA)
			goto nocomma;

		if (!(parse_data_expression
		      (&(*expr) -> data.reverse.buffer, cfile, lose)))
			goto nodata;

		token = next_token (&val, (unsigned *)0, cfile);
		if (token != RPAREN)
			goto norparen;
		break;

	      case PICK:
		/* pick (a, b, c) actually produces an internal representation
		   that looks like pick (a, pick (b, pick (c, nil))). */
		token = next_token (&val, (unsigned *)0, cfile);
		if (!(expression_allocate (expr, MDL)))
			log_fatal ("can't allocate expression");

		token = next_token (&val, (unsigned *)0, cfile);
		if (token != LPAREN)
			goto nolparen;

		nexp = (struct expression *)0;
		expression_reference (&nexp, *expr, MDL);
		do {
		    nexp -> op = expr_pick_first_value;
		    if (!(parse_data_expression
			  (&nexp -> data.pick_first_value.car,
			   cfile, lose)))
			goto nodata;

		    token = next_token (&val, (unsigned *)0, cfile);
		    if (token == COMMA) {
			struct expression *foo = (struct expression *)0;
			if (!expression_allocate (&foo, MDL))
			    log_fatal ("can't allocate expr");
			expression_reference
				(&nexp -> data.pick_first_value.cdr, foo, MDL);
			expression_dereference (&nexp, MDL);
			expression_reference (&nexp, foo, MDL);
			expression_dereference (&foo, MDL);
		    }
		} while (token == COMMA);
		expression_dereference (&nexp, MDL);

		if (token != RPAREN)
			goto norparen;
		break;

		/* dns-update and dns-delete are present for historical
		   purposes, but are deprecated in favor of ns-update
		   in combination with update, delete, exists and not
		   exists. */
	      case DNS_UPDATE:
	      case DNS_DELETE:
#if !defined (NSUPDATE)
		parse_warn (cfile,
			    "Please rebuild dhcpd with --with-nsupdate.");
#endif
		token = next_token (&val, (unsigned *)0, cfile);
		if (token == DNS_UPDATE)
			opcode = expr_ns_add;
		else
			opcode = expr_ns_delete;

		token = next_token (&val, (unsigned *)0, cfile);
		if (token != LPAREN)
			goto nolparen;

		token = next_token (&val, (unsigned *)0, cfile);
		if (token != STRING) {
			parse_warn (cfile,
				    "parse_expression: expecting string.");
		      badnsupdate:
			skip_to_semi (cfile);
			*lose = 1;
			return 0;
		}
			
		if (!strcasecmp (val, "a"))
			u = T_A;
		else if (!strcasecmp (val, "ptr"))
			u = T_PTR;
		else if (!strcasecmp (val, "mx"))
			u = T_MX;
		else if (!strcasecmp (val, "cname"))
			u = T_CNAME;
		else if (!strcasecmp (val, "TXT"))
			u = T_TXT;
		else {
			parse_warn (cfile, "unexpected rrtype: %s", val);
			goto badnsupdate;
		}

		s = (opcode == expr_ns_add
		     ? "old-dns-update"
		     : "old-dns-delete");
		cptr = dmalloc (strlen (s) + 1, MDL);
		if (!cptr)
			log_fatal ("can't allocate name for %s", s);
		strcpy (cptr, s);
		if (!expression_allocate (expr, MDL))
			log_fatal ("can't allocate expression");
		(*expr) -> op = expr_funcall;
		(*expr) -> data.funcall.name = cptr;

		/* Fake up a function call. */
		ep = &(*expr) -> data.funcall.arglist;
		if (!expression_allocate (ep, MDL))
			log_fatal ("can't allocate expression");
		(*ep) -> op = expr_arg;
		if (!make_const_int (&(*ep) -> data.arg.val, u))
			log_fatal ("can't allocate rrtype value.");

		token = next_token (&val, (unsigned *)0, cfile);
		if (token != COMMA)
			goto nocomma;
		ep = &((*ep) -> data.arg.next);
		if (!expression_allocate (ep, MDL))
			log_fatal ("can't allocate expression");
		(*ep) -> op = expr_arg;
		if (!(parse_data_expression (&(*ep) -> data.arg.val,
					     cfile, lose)))
			goto nodata;

		token = next_token (&val, (unsigned *)0, cfile);
		if (token != COMMA)
			goto nocomma;

		ep = &((*ep) -> data.arg.next);
		if (!expression_allocate (ep, MDL))
			log_fatal ("can't allocate expression");
		(*ep) -> op = expr_arg;
		if (!(parse_data_expression (&(*ep) -> data.arg.val,
					     cfile, lose)))
			goto nodata;

		if (opcode == expr_ns_add) {
			token = next_token (&val, (unsigned *)0, cfile);
			if (token != COMMA)
				goto nocomma;
			
			ep = &((*ep) -> data.arg.next);
			if (!expression_allocate (ep, MDL))
				log_fatal ("can't allocate expression");
			(*ep) -> op = expr_arg;
			if (!(parse_numeric_expression (&(*ep) -> data.arg.val,
							cfile, lose))) {
				parse_warn (cfile,
					    "expecting numeric expression.");
				goto badnsupdate;
			}
		}

		token = next_token (&val, (unsigned *)0, cfile);
		if (token != RPAREN)
			goto norparen;
		break;

	      case NS_UPDATE:
#if !defined (NSUPDATE)
		parse_warn (cfile,
			    "Please rebuild dhcpd with --with-nsupdate.");
#endif
		token = next_token (&val, (unsigned *)0, cfile);
		if (!expression_allocate (expr, MDL))
			log_fatal ("can't allocate expression");

		token = next_token (&val, (unsigned *)0, cfile);
		if (token != LPAREN)
			goto nolparen;

		nexp = *expr;
		do {
			nexp -> op = expr_dns_transaction;
			if (!(parse_dns_expression
			      (&nexp -> data.dns_transaction.car,
			       cfile, lose)))
			{
				if (!*lose)
					parse_warn
						(cfile,
						 "expecting dns expression.");
			      badnstrans:
				expression_dereference (expr, MDL);
				*lose = 1;
				return 0;
			}

			token = next_token (&val, (unsigned *)0, cfile);
			
			if (token == COMMA) {
				if (!(expression_allocate
				      (&nexp -> data.dns_transaction.cdr,
				       MDL)))
					log_fatal
						("can't allocate expression");
				nexp = nexp -> data.dns_transaction.cdr;
			}
		} while (token == COMMA);

		if (token != RPAREN)
			goto norparen;
		break;

		/* NOT EXISTS is special cased above... */
	      not_exists:
		token = peek_token (&val, (unsigned *)0, cfile);
		if (token != EXISTS) {
			parse_warn (cfile, "expecting DNS prerequisite.");
			*lose = 1;
			return 0;
		}
		opcode = expr_ns_not_exists;
		goto nsupdatecode;
	      case TOKEN_ADD:
		opcode = expr_ns_add;
		goto nsupdatecode;
	      case TOKEN_DELETE:
		opcode = expr_ns_delete;
		goto nsupdatecode;
	      ns_exists:
		opcode = expr_ns_exists;
	      nsupdatecode:
		token = next_token (&val, (unsigned *)0, cfile);

#if !defined (NSUPDATE)
		parse_warn (cfile,
			    "Please rebuild dhcpd with --with-nsupdate.");
#endif
		if (!expression_allocate (expr, MDL))
			log_fatal ("can't allocate expression");
		(*expr) -> op = opcode;

		token = next_token (&val, (unsigned *)0, cfile);
		if (token != LPAREN)
			goto nolparen;

		token = next_token (&val, (unsigned *)0, cfile);
		if (!is_identifier (token) && token != NUMBER) {
			parse_warn (cfile, "expecting identifier or number.");
		      badnsop:
			expression_dereference (expr, MDL);
			skip_to_semi (cfile);
			*lose = 1;
			return 0;
		}
			
		if (token == NUMBER)
			(*expr) -> data.ns_add.rrclass = atoi (val);
		else if (!strcasecmp (val, "in"))
			(*expr) -> data.ns_add.rrclass = C_IN;
		else if (!strcasecmp (val, "chaos"))
			(*expr) -> data.ns_add.rrclass = C_CHAOS;
		else if (!strcasecmp (val, "hs"))
			(*expr) -> data.ns_add.rrclass = C_HS;
		else {
			parse_warn (cfile, "unexpected rrclass: %s", val);
			goto badnsop;
		}
		
		token = next_token (&val, (unsigned *)0, cfile);
		if (token != COMMA)
			goto nocomma;

		token = next_token (&val, (unsigned *)0, cfile);
		if (!is_identifier (token) && token != NUMBER) {
			parse_warn (cfile, "expecting identifier or number.");
			goto badnsop;
		}
			
		if (token == NUMBER)
			(*expr) -> data.ns_add.rrtype = atoi (val);
		else if (!strcasecmp (val, "a"))
			(*expr) -> data.ns_add.rrtype = T_A;
		else if (!strcasecmp (val, "ptr"))
			(*expr) -> data.ns_add.rrtype = T_PTR;
		else if (!strcasecmp (val, "mx"))
			(*expr) -> data.ns_add.rrtype = T_MX;
		else if (!strcasecmp (val, "cname"))
			(*expr) -> data.ns_add.rrtype = T_CNAME;
		else if (!strcasecmp (val, "TXT"))
			(*expr) -> data.ns_add.rrtype = T_TXT;
		else {
			parse_warn (cfile, "unexpected rrtype: %s", val);
			goto badnsop;
		}

		token = next_token (&val, (unsigned *)0, cfile);
		if (token != COMMA)
			goto nocomma;

		if (!(parse_data_expression
		      (&(*expr) -> data.ns_add.rrname, cfile, lose)))
			goto nodata;

		token = next_token (&val, (unsigned *)0, cfile);
		if (token != COMMA)
			goto nocomma;

		if (!(parse_data_expression
		      (&(*expr) -> data.ns_add.rrdata, cfile, lose)))
			goto nodata;

		if (opcode == expr_ns_add) {
			token = next_token (&val, (unsigned *)0, cfile);
			if (token != COMMA)
				goto nocomma;
			
			if (!(parse_numeric_expression
			      (&(*expr) -> data.ns_add.ttl, cfile,
			       lose))) {
			    if (!*lose)
				parse_warn (cfile,
					    "expecting numeric expression.");
			    goto badnsupdate;
			}
		}

		token = next_token (&val, (unsigned *)0, cfile);
		if (token != RPAREN)
			goto norparen;
		break;

	      case OPTION:
	      case CONFIG_OPTION:
		if (!expression_allocate (expr, MDL))
			log_fatal ("can't allocate expression");
		(*expr) -> op = (token == OPTION
				 ? expr_option
				 : expr_config_option);
		token = next_token (&val, (unsigned *)0, cfile);
		known = 0;
		(*expr) -> data.option = parse_option_name (cfile, 0, &known);
		if (!(*expr) -> data.option) {
			*lose = 1;
			expression_dereference (expr, MDL);
			return 0;
		}
		break;

	      case HARDWARE:
		token = next_token (&val, (unsigned *)0, cfile);
		if (!expression_allocate (expr, MDL))
			log_fatal ("can't allocate expression");
		(*expr) -> op = expr_hardware;
		break;

	      case LEASED_ADDRESS:
		token = next_token (&val, (unsigned *)0, cfile);
		if (!expression_allocate (expr, MDL))
			log_fatal ("can't allocate expression");
		(*expr) -> op = expr_leased_address;
		break;

	      case CLIENT_STATE:
		token = next_token (&val, (unsigned *)0, cfile);
		if (!expression_allocate (expr, MDL))
			log_fatal ("can't allocate expression");
		(*expr) -> op = expr_client_state;
		break;

	      case FILENAME:
		token = next_token (&val, (unsigned *)0, cfile);
		if (!expression_allocate (expr, MDL))
			log_fatal ("can't allocate expression");
		(*expr) -> op = expr_filename;
		break;

	      case SERVER_NAME:
		token = next_token (&val, (unsigned *)0, cfile);
		if (!expression_allocate (expr, MDL))
			log_fatal ("can't allocate expression");
		(*expr) -> op = expr_sname;
		break;

	      case LEASE_TIME:
		token = next_token (&val, (unsigned *)0, cfile);
		if (!expression_allocate (expr, MDL))
			log_fatal ("can't allocate expression");
		(*expr) -> op = expr_lease_time;
		break;

	      case TOKEN_NULL:
		token = next_token (&val, (unsigned *)0, cfile);
		if (!expression_allocate (expr, MDL))
			log_fatal ("can't allocate expression");
		(*expr) -> op = expr_null;
		break;

	      case HOST_DECL_NAME:
		token = next_token (&val, (unsigned *)0, cfile);
		if (!expression_allocate (expr, MDL))
			log_fatal ("can't allocate expression");
		(*expr) -> op = expr_host_decl_name;
		break;

	      case UPDATED_DNS_RR:
		token = next_token (&val, (unsigned *)0, cfile);

		token = next_token (&val, (unsigned *)0, cfile);
		if (token != LPAREN)
			goto nolparen;

		token = next_token (&val, (unsigned *)0, cfile);
		if (token != STRING) {
			parse_warn (cfile, "expecting string.");
		      bad_rrtype:
			*lose = 1;
			return 0;
		}
		if (!strcasecmp (val, "a"))
			s = "ddns-fwd-name";
		else if (!strcasecmp (val, "ptr"))
			s = "ddns-rev-name";
		else {
			parse_warn (cfile, "invalid DNS rrtype: %s", val);
			goto bad_rrtype;
		}

		token = next_token (&val, (unsigned *)0, cfile);
		if (token != RPAREN)
			goto norparen;

		if (!expression_allocate (expr, MDL))
			log_fatal ("can't allocate expression");
		(*expr) -> op = expr_variable_reference;
		(*expr) -> data.variable =
			dmalloc (strlen (s) + 1, MDL);
		if (!(*expr) -> data.variable)
			log_fatal ("can't allocate variable name.");
		strcpy ((*expr) -> data.variable, s);
		break;

	      case PACKET:
		token = next_token (&val, (unsigned *)0, cfile);
		if (!expression_allocate (expr, MDL))
			log_fatal ("can't allocate expression");
		(*expr) -> op = expr_packet;

		token = next_token (&val, (unsigned *)0, cfile);
		if (token != LPAREN)
			goto nolparen;

		if (!parse_numeric_expression (&(*expr) -> data.packet.offset,
					       cfile, lose))
			goto nonum;

		token = next_token (&val, (unsigned *)0, cfile);
		if (token != COMMA)
			goto nocomma;

		if (!parse_numeric_expression (&(*expr) -> data.packet.len,
					       cfile, lose))
			goto nonum;

		token = next_token (&val, (unsigned *)0, cfile);
		if (token != RPAREN)
			goto norparen;
		break;
		
	      case STRING:
		token = next_token (&val, &len, cfile);
		if (!make_const_data (expr, (const unsigned char *)val,
				      len, 1, 1, MDL))
			log_fatal ("can't make constant string expression.");
		break;

	      case EXTRACT_INT:
		token = next_token (&val, (unsigned *)0, cfile);	
		token = next_token (&val, (unsigned *)0, cfile);
		if (token != LPAREN) {
			parse_warn (cfile, "left parenthesis expected.");
			*lose = 1;
			return 0;
		}

		if (!expression_allocate (expr, MDL))
			log_fatal ("can't allocate expression");

		if (!parse_data_expression (&(*expr) -> data.extract_int,
					    cfile, lose)) {
			if (!*lose) {
				parse_warn (cfile,
					    "expecting data expression.");
				skip_to_semi (cfile);
				*lose = 1;
			}
			expression_dereference (expr, MDL);
			return 0;
		}

		token = next_token (&val, (unsigned *)0, cfile);
		if (token != COMMA) {
			parse_warn (cfile, "comma expected.");
			*lose = 1;
			expression_dereference (expr, MDL);
			return 0;
		}

		token = next_token (&val, (unsigned *)0, cfile);
		if (token != NUMBER) {
			parse_warn (cfile, "number expected.");
			*lose = 1;
			expression_dereference (expr, MDL);
			return 0;
		}
		switch (atoi (val)) {
		      case 8:
			(*expr) -> op = expr_extract_int8;
			break;

		      case 16:
			(*expr) -> op = expr_extract_int16;
			break;

		      case 32:
			(*expr) -> op = expr_extract_int32;
			break;

		      default:
			parse_warn (cfile,
				    "unsupported integer size %d", atoi (val));
			*lose = 1;
			skip_to_semi (cfile);
			expression_dereference (expr, MDL);
			return 0;
		}

		token = next_token (&val, (unsigned *)0, cfile);
		if (token != RPAREN) {
			parse_warn (cfile, "right parenthesis expected.");
			*lose = 1;
			expression_dereference (expr, MDL);
			return 0;
		}
		break;
	
	      case ENCODE_INT:
		token = next_token (&val, (unsigned *)0, cfile);	
		token = next_token (&val, (unsigned *)0, cfile);
		if (token != LPAREN) {
			parse_warn (cfile, "left parenthesis expected.");
			*lose = 1;
			return 0;
		}

		if (!expression_allocate (expr, MDL))
			log_fatal ("can't allocate expression");

		if (!parse_numeric_expression (&(*expr) -> data.encode_int,
					       cfile, lose)) {
			parse_warn (cfile, "expecting numeric expression.");
			skip_to_semi (cfile);
			*lose = 1;
			expression_dereference (expr, MDL);
			return 0;
		}

		token = next_token (&val, (unsigned *)0, cfile);
		if (token != COMMA) {
			parse_warn (cfile, "comma expected.");
			*lose = 1;
			expression_dereference (expr, MDL);
			return 0;
		}

		token = next_token (&val, (unsigned *)0, cfile);
		if (token != NUMBER) {
			parse_warn (cfile, "number expected.");
			*lose = 1;
			expression_dereference (expr, MDL);
			return 0;
		}
		switch (atoi (val)) {
		      case 8:
			(*expr) -> op = expr_encode_int8;
			break;

		      case 16:
			(*expr) -> op = expr_encode_int16;
			break;

		      case 32:
			(*expr) -> op = expr_encode_int32;
			break;

		      default:
			parse_warn (cfile,
				    "unsupported integer size %d", atoi (val));
			*lose = 1;
			skip_to_semi (cfile);
			expression_dereference (expr, MDL);
			return 0;
		}

		token = next_token (&val, (unsigned *)0, cfile);
		if (token != RPAREN) {
			parse_warn (cfile, "right parenthesis expected.");
			*lose = 1;
			expression_dereference (expr, MDL);
			return 0;
		}
		break;
	
	      case NUMBER:
		/* If we're in a numeric context, this should just be a
		   number, by itself. */
		if (context == context_numeric ||
		    context == context_data_or_numeric) {
			next_token (&val, (unsigned *)0, cfile);
			if (!expression_allocate (expr, MDL))
				log_fatal ("can't allocate expression");
			(*expr) -> op = expr_const_int;
			(*expr) -> data.const_int = atoi (val);
			break;
		}

	      case NUMBER_OR_NAME:
		if (!expression_allocate (expr, MDL))
			log_fatal ("can't allocate expression");

		(*expr) -> op = expr_const_data;
		if (!parse_cshl (&(*expr) -> data.const_data, cfile)) {
			expression_dereference (expr, MDL);
			return 0;
		}
		break;

	      case NS_FORMERR:
		known = FORMERR;
		goto ns_const;
	      ns_const:
		token = next_token (&val, (unsigned *)0, cfile);
		if (!expression_allocate (expr, MDL))
			log_fatal ("can't allocate expression");
		(*expr) -> op = expr_const_int;
		(*expr) -> data.const_int = known;
		break;
		
	      case NS_NOERROR:
		known = ISC_R_SUCCESS;
		goto ns_const;

	      case NS_NOTAUTH:
		known = ISC_R_NOTAUTH;
		goto ns_const;

	      case NS_NOTIMP:
		known = ISC_R_NOTIMPLEMENTED;
		goto ns_const;

	      case NS_NOTZONE:
		known = ISC_R_NOTZONE;
		goto ns_const;

	      case NS_NXDOMAIN:
		known = ISC_R_NXDOMAIN;
		goto ns_const;

	      case NS_NXRRSET:
		known = ISC_R_NXRRSET;
		goto ns_const;

	      case NS_REFUSED:
		known = ISC_R_REFUSED;
		goto ns_const;

	      case NS_SERVFAIL:
		known = ISC_R_SERVFAIL;
		goto ns_const;

	      case NS_YXDOMAIN:
		known = ISC_R_YXDOMAIN;
		goto ns_const;

	      case NS_YXRRSET:
		known = ISC_R_YXRRSET;
		goto ns_const;

	      case BOOTING:
		known = S_INIT;
		goto ns_const;

	      case REBOOT:
		known = S_REBOOTING;
		goto ns_const;

	      case SELECT:
		known = S_SELECTING;
		goto ns_const;

	      case REQUEST:
		known = S_REQUESTING;
		goto ns_const;

	      case BOUND:
		known = S_BOUND;
		goto ns_const;

	      case RENEW:
		known = S_RENEWING;
		goto ns_const;

	      case REBIND:
		known = S_REBINDING;
		goto ns_const;

	      case DEFINED:
		token = next_token (&val, (unsigned *)0, cfile);
		token = next_token (&val, (unsigned *)0, cfile);
		if (token != LPAREN)
			goto nolparen;

		token = next_token (&val, (unsigned *)0, cfile);
		if (token != NAME && token != NUMBER_OR_NAME) {
			parse_warn (cfile, "%s can't be a variable name", val);
			skip_to_semi (cfile);
			*lose = 1;
			return 0;
		}

		if (!expression_allocate (expr, MDL))
			log_fatal ("can't allocate expression");
		(*expr) -> op = expr_variable_exists;
		(*expr) -> data.variable = dmalloc (strlen (val) + 1, MDL);
		if (!(*expr)->data.variable)
			log_fatal ("can't allocate variable name");
		strcpy ((*expr) -> data.variable, val);
		token = next_token (&val, (unsigned *)0, cfile);
		if (token != RPAREN)
			goto norparen;
		break;

		/* Not a valid start to an expression... */
	      default:
		if (token != NAME && token != NUMBER_OR_NAME)
			return 0;

		token = next_token (&val, (unsigned *)0, cfile);

		/* Save the name of the variable being referenced. */
		cptr = dmalloc (strlen (val) + 1, MDL);
		if (!cptr)
			log_fatal ("can't allocate variable name");
		strcpy (cptr, val);

		/* Simple variable reference, as far as we can tell. */
		token = peek_token (&val, (unsigned *)0, cfile);
		if (token != LPAREN) {
			if (!expression_allocate (expr, MDL))
				log_fatal ("can't allocate expression");
			(*expr) -> op = expr_variable_reference;
			(*expr) -> data.variable = cptr;
			break;
		}

		token = next_token (&val, (unsigned *)0, cfile);
		if (!expression_allocate (expr, MDL))
			log_fatal ("can't allocate expression");
		(*expr) -> op = expr_funcall;
		(*expr) -> data.funcall.name = cptr;

		/* Now parse the argument list. */
		ep = &(*expr) -> data.funcall.arglist;
		do {
			if (!expression_allocate (ep, MDL))
				log_fatal ("can't allocate expression");
			(*ep) -> op = expr_arg;
			if (!parse_expression (&(*ep) -> data.arg.val,
					       cfile, lose, context_any,
					       (struct expression **)0,
					       expr_none)) {
				if (!*lose) {
					parse_warn (cfile,
						    "expecting expression.");
					*lose = 1;
				}
				skip_to_semi (cfile);
				expression_dereference (expr, MDL);
				return 0;
			}
			ep = &((*ep) -> data.arg.next);
			token = next_token (&val, (unsigned *)0, cfile);
		} while (token == COMMA);
		if (token != RPAREN) {
			parse_warn (cfile, "Right parenthesis expected.");
			skip_to_semi (cfile);
			*lose = 1;
			expression_dereference (expr, MDL);
			return 0;
		}
		break;
	}
	return 1;
}

/* Parse an expression. */

int parse_expression (expr, cfile, lose, context, plhs, binop)
	struct expression **expr;
	struct parse *cfile;
	int *lose;
	enum expression_context context;
	struct expression **plhs;
	enum expr_op binop;
{
	enum dhcp_token token;
	const char *val;
	struct expression *rhs = (struct expression *)0, *tmp;
	struct expression *lhs = (struct expression *)0;
	enum expr_op next_op;
	enum expression_context
		lhs_context = context_any,
		rhs_context = context_any;

	/* Consume the left hand side we were passed. */
	if (plhs) {
		expression_reference (&lhs, *plhs, MDL);
		expression_dereference (plhs, MDL);
	}

      new_rhs:
	if (!parse_non_binary (&rhs, cfile, lose, context)) {
		/* If we already have a left-hand side, then it's not
		   okay for there not to be a right-hand side here, so
		   we need to flag it as an error. */
		if (lhs) {
			if (!*lose) {
				parse_warn (cfile,
					    "expecting right-hand side.");
				*lose = 1;
				skip_to_semi (cfile);
			}
			expression_dereference (&lhs, MDL);
		}
		return 0;
	}

	/* At this point, rhs contains either an entire subexpression,
	   or at least a left-hand-side.   If we do not see a binary token
	   as the next token, we're done with the expression. */

	token = peek_token (&val, (unsigned *)0, cfile);
	switch (token) {
	      case BANG:
		token = next_token (&val, (unsigned *)0, cfile);
		token = peek_token (&val, (unsigned *)0, cfile);
		if (token != EQUAL) {
			parse_warn (cfile, "! in boolean context without =");
			*lose = 1;
			skip_to_semi (cfile);
			if (lhs)
				expression_dereference (&lhs, MDL);
			return 0;
		}
		next_op = expr_not_equal;
		context = expression_context (rhs);
		break;

	      case EQUAL:
		next_op = expr_equal;
		context = expression_context (rhs);
		break;

	      case AND:
		next_op = expr_and;
		context = expression_context (rhs);
		if (context != context_boolean) {
		      needbool:
			parse_warn (cfile, "expecting boolean expressions");
			skip_to_semi (cfile);
			expression_dereference (&rhs, MDL);
			*lose = 1;
			return 0;
		}
		break;

	      case OR:
		next_op = expr_or;
		context = expression_context (rhs);
		if (context != context_boolean)
			goto needbool;
		break;

	      case PLUS:
		next_op = expr_add;
		context = expression_context (rhs);
		if (context != context_numeric) {
		      neednum:
			parse_warn (cfile, "expecting numeric expressions");
			skip_to_semi (cfile);
			expression_dereference (&rhs, MDL);
			*lose = 1;
			return 0;
		}
		break;

	      case MINUS:
		next_op = expr_subtract;
		context = expression_context (rhs);
		if (context != context_numeric)
			goto neednum;
		break;

	      case SLASH:
		next_op = expr_divide;
		context = expression_context (rhs);
		if (context != context_numeric)
			goto neednum;
		break;

	      case ASTERISK:
		next_op = expr_multiply;
		context = expression_context (rhs);
		if (context != context_numeric)
			goto neednum;
		break;

	      case PERCENT:
		next_op = expr_remainder;
		context = expression_context (rhs);
		if (context != context_numeric)
			goto neednum;
		break;

	      case AMPERSAND:
		next_op = expr_binary_and;
		context = expression_context (rhs);
		if (context != context_numeric)
			goto neednum;
		break;

	      case PIPE:
		next_op = expr_binary_or;
		context = expression_context (rhs);
		if (context != context_numeric)
			goto neednum;
		break;

	      case CARET:
		next_op = expr_binary_xor;
		context = expression_context (rhs);
		if (context != context_numeric)
			goto neednum;
		break;

	      default:
		next_op = expr_none;
	}

	/* If we have no lhs yet, we just parsed it. */
	if (!lhs) {
		/* If there was no operator following what we just parsed,
		   then we're done - return it. */
		if (next_op == expr_none) {
			*expr = rhs;
			return 1;
		}
		lhs = rhs;
		rhs = (struct expression *)0;
		binop = next_op;
		next_token (&val, (unsigned *)0, cfile);
		goto new_rhs;
	}

	/* Now, if we didn't find a binary operator, we're done parsing
	   this subexpression, so combine it with the preceding binary
	   operator and return the result. */
	if (next_op == expr_none) {
		if (!expression_allocate (expr, MDL))
			log_fatal ("Can't allocate expression!");

		(*expr) -> op = binop;
		/* All the binary operators' data union members
		   are the same, so we'll cheat and use the member
		   for the equals operator. */
		(*expr) -> data.equal [0] = lhs;
		(*expr) -> data.equal [1] = rhs;
		return 1;
	}

	/* Eat the operator token - we now know it was a binary operator... */
	token = next_token (&val, (unsigned *)0, cfile);

	/* If the binary operator we saw previously has a lower precedence
	   than the next operator, then the rhs we just parsed for that
	   operator is actually the lhs of the operator with the higher
	   precedence - to get the real rhs, we need to recurse on the
	   new operator. */
 	if (binop != expr_none &&
	    op_precedence (binop, next_op) < 0) {
		tmp = rhs;
		rhs = (struct expression *)0;
		if (!parse_expression (&rhs, cfile, lose, op_context (next_op),
				       &tmp, next_op)) {
			if (!*lose) {
				parse_warn (cfile,
					    "expecting a subexpression");
				*lose = 1;
			}
			return 0;
		}
		next_op = expr_none;
	}

	/* Now combine the LHS and the RHS using binop. */
	tmp = (struct expression *)0;
	if (!expression_allocate (&tmp, MDL))
		log_fatal ("No memory for equal precedence combination.");
	
	/* Store the LHS and RHS. */
	tmp -> data.equal [0] = lhs;
	tmp -> data.equal [1] = rhs;
	tmp -> op = binop;
	
	lhs = tmp;
	tmp = (struct expression *)0;
	rhs = (struct expression *)0;

	/* Recursions don't return until we have parsed the end of the
	   expression, so if we recursed earlier, we can now return what
	   we got. */
	if (next_op == expr_none) {
		*expr = lhs;
		return 1;
	}

	binop = next_op;
	goto new_rhs;
}	

/* option-statement :== identifier DOT identifier <syntax> SEMI
		      | identifier <syntax> SEMI

   Option syntax is handled specially through format strings, so it
   would be painful to come up with BNF for it.   However, it always
   starts as above and ends in a SEMI. */

int parse_option_statement (result, cfile, lookups, option, op)
	struct executable_statement **result;
	struct parse *cfile;
	int lookups;
	struct option *option;
	enum statement_op op;
{
	const char *val;
	enum dhcp_token token;
	const char *fmt = NULL;
	struct expression *expr = (struct expression *)0;
	struct expression *tmp;
	int lose;
	struct executable_statement *stmt;
	int ftt = 1;

	token = peek_token (&val, (unsigned *)0, cfile);
	if (token == SEMI) {
		/* Eat the semicolon... */
		token = next_token (&val, (unsigned *)0, cfile);
		goto done;
	}

	if (token == EQUAL) {
		/* Eat the equals sign. */
		token = next_token (&val, (unsigned *)0, cfile);

		/* Parse a data expression and use its value for the data. */
		if (!parse_data_expression (&expr, cfile, &lose)) {
			/* In this context, we must have an executable
			   statement, so if we found something else, it's
			   still an error. */
			if (!lose) {
				parse_warn (cfile,
					    "expecting a data expression.");
				skip_to_semi (cfile);
			}
			return 0;
		}

		/* We got a valid expression, so use it. */
		goto done;
	}

	/* Parse the option data... */
	do {
		/* Set a flag if this is an array of a simple type (i.e.,
		   not an array of pairs of IP addresses, or something
		   like that. */
		int uniform = option -> format [1] == 'A';

	      and_again:
		/* Set fmt to start of format for 'A' and one char back
		   for 'a' */
                if ((fmt != NULL) &&
		    (fmt != option -> format) && (*fmt == 'a'))
			fmt -= 1;
		else
			fmt = ((fmt == NULL) ||
			       (*fmt == 'A')) ? option -> format : fmt;

		/* 'a' means always uniform */
		uniform |= (fmt [1] == 'a');

		for ( ; *fmt; fmt++) {
			if ((*fmt == 'A') || (*fmt == 'a'))
				break;
			if (*fmt == 'o')
				continue;
			tmp = expr;
			expr = (struct expression *)0;
			if (!parse_option_token (&expr, cfile, &fmt,
						 tmp, uniform, lookups)) {
				if (fmt [1] != 'o') {
					if (tmp)
						expression_dereference (&tmp,
									MDL);
					return 0;
				}
				expr = tmp;
				tmp = (struct expression *)0;
			}
			if (tmp)
				expression_dereference (&tmp, MDL);
		}
		if ((*fmt == 'A') || (*fmt == 'a')) {
			token = peek_token (&val, (unsigned *)0, cfile);
			/* Comma means: continue with next element in array */
			if (token == COMMA) {
				token = next_token (&val,
						    (unsigned *)0, cfile);
				continue;
			}
			/* no comma: end of array.
			   'A' or end of string means: leave the loop */
			if ((*fmt == 'A') || (fmt[1] == '\0'))
				break;
			/* 'a' means: go on with next char */
			if (*fmt == 'a') {
				fmt++;
				goto and_again;
			}
		}
	} while ((*fmt == 'A') || (*fmt == 'a'));

      done:
	if (!parse_semi (cfile))
		return 0;
	if (!executable_statement_allocate (result, MDL))
		log_fatal ("no memory for option statement.");
	(*result) -> op = op;
	if (expr && !option_cache (&(*result) -> data.option,
				   (struct data_string *)0, expr, option, MDL))
		log_fatal ("no memory for option cache");
	if (expr)
		expression_dereference (&expr, MDL);
	return 1;
}

int parse_option_token (rv, cfile, fmt, expr, uniform, lookups)
	struct expression **rv;
	struct parse *cfile;
	const char **fmt;
	struct expression *expr;
	int uniform;
	int lookups;
{
	const char *val;
	enum dhcp_token token;
	struct expression *t = (struct expression *)0;
	unsigned char buf [4];
	unsigned len;
	unsigned char *ob;
	struct iaddr addr;
	int num;
	const char *f, *g;
	struct enumeration_value *e;

	switch (**fmt) {
	      case 'U':
		token = peek_token (&val, (unsigned *)0, cfile);
		if (!is_identifier (token)) {
			if ((*fmt) [1] != 'o') {
				parse_warn (cfile, "expecting identifier.");
				skip_to_semi (cfile);
			}
			return 0;
		}
		token = next_token (&val, &len, cfile);
		if (!make_const_data (&t, (const unsigned char *)val,
				      len, 1, 1, MDL))
			log_fatal ("No memory for %s", val);
		break;

	      case 'E':
		g = strchr (*fmt, '.');
		if (!g) {
			parse_warn (cfile,
				    "malformed encapsulation format (bug!)");
			skip_to_semi (cfile);
			return 0;
		}
		*fmt = g;
	      case 'X':
		token = peek_token (&val, (unsigned *)0, cfile);
		if (token == NUMBER_OR_NAME || token == NUMBER) {
			if (!expression_allocate (&t, MDL))
				return 0;
			if (!parse_cshl (&t -> data.const_data, cfile)) {
				expression_dereference (&t, MDL);
				return 0;
			}
			t -> op = expr_const_data;
		} else if (token == STRING) {
			token = next_token (&val, &len, cfile);
			if (!make_const_data (&t, (const unsigned char *)val,
					      len, 1, 1, MDL))
				log_fatal ("No memory for \"%s\"", val);
		} else {
			if ((*fmt) [1] != 'o') {
				parse_warn (cfile, "expecting string %s.",
					    "or hexadecimal data");
				skip_to_semi (cfile);
			}
			return 0;
		}
		break;
		
	      case 'd': /* Domain name... */
		val = parse_host_name (cfile);
		if (!val) {
			parse_warn (cfile, "not a valid domain name.");
			skip_to_semi (cfile);
			return 0;
		}
		len = strlen (val);
		goto make_string;

	      case 't': /* Text string... */
		token = peek_token (&val, (unsigned *)0, cfile);
		if (token != STRING && !is_identifier (token)) {
			if ((*fmt) [1] != 'o') {
				parse_warn (cfile, "expecting string.");
				if (token != SEMI)
					skip_to_semi (cfile);
			}
			return 0;
		}
		token = next_token (&val, &len, cfile);
	      make_string:
		if (!make_const_data (&t, (const unsigned char *)val,
				      len, 1, 1, MDL))
			log_fatal ("No memory for concatenation");
		break;
		
	      case 'N':
		f = (*fmt) + 1;
		g = strchr (*fmt, '.');
		if (!g) {
			parse_warn (cfile, "malformed %s (bug!)",
				    "enumeration format");
		      foo:
			skip_to_semi (cfile);
			return 0;
		}
		*fmt = g;
		token = next_token (&val, (unsigned *)0, cfile);
		if (!is_identifier (token)) {
			parse_warn (cfile,
				    "identifier expected");
			goto foo;
		}
		e = find_enumeration_value (f, (*fmt) - f, val);
		if (!e) {
			parse_warn (cfile, "unknown value");
			goto foo;
		}
		if (!make_const_data (&t, &e -> value, 1, 0, 1, MDL))
			return 0;
		break;

	      case 'I': /* IP address or hostname. */
		if (lookups) {
			if (!parse_ip_addr_or_hostname (&t, cfile, uniform))
				return 0;
		} else {
			if (!parse_ip_addr (cfile, &addr))
				return 0;
			if (!make_const_data (&t, addr.iabuf, addr.len,
					      0, 1, MDL))
				return 0;
		}
		break;
		
	      case 'T':	/* Lease interval. */
		token = peek_token (&val, (unsigned *)0, cfile);
		if (token != INFINITE)
			goto check_number;
		token = next_token (&val, (unsigned *)0, cfile);
		putLong (buf, -1);
		if (!make_const_data (&t, buf, 4, 0, 1, MDL))
			return 0;
		break;

	      case 'L': /* Unsigned 32-bit integer... */
	      case 'l':	/* Signed 32-bit integer... */
		token = peek_token (&val, (unsigned *)0, cfile);
	      check_number:
		if (token != NUMBER) {
		      need_number:
			if ((*fmt) [1] != 'o') {
				parse_warn (cfile, "expecting number.");
				if (token != SEMI)
					skip_to_semi (cfile);
			}
			return 0;
		}
		token = next_token (&val, (unsigned *)0, cfile);
		convert_num (cfile, buf, val, 0, 32);
		if (!make_const_data (&t, buf, 4, 0, 1, MDL))
			return 0;
		break;

	      case 's':	/* Signed 16-bit integer. */
	      case 'S':	/* Unsigned 16-bit integer. */
		token = peek_token (&val, (unsigned *)0, cfile);
		if (token != NUMBER)
			goto need_number;
		token = next_token (&val, (unsigned *)0, cfile);
		convert_num (cfile, buf, val, 0, 16);
		if (!make_const_data (&t, buf, 2, 0, 1, MDL))
			return 0;
		break;

	      case 'b':	/* Signed 8-bit integer. */
	      case 'B':	/* Unsigned 8-bit integer. */
		token = peek_token (&val, (unsigned *)0, cfile);
		if (token != NUMBER)
			goto need_number;
		token = next_token (&val, (unsigned *)0, cfile);
		convert_num (cfile, buf, val, 0, 8);
		if (!make_const_data (&t, buf, 1, 0, 1, MDL))
			return 0;
		break;

	      case 'f': /* Boolean flag. */
		token = peek_token (&val, (unsigned *)0, cfile);
		if (!is_identifier (token)) {
			if ((*fmt) [1] != 'o')
				parse_warn (cfile, "expecting identifier.");
		      bad_flag:
			if ((*fmt) [1] != 'o') {
				if (token != SEMI)
					skip_to_semi (cfile);
			}
			return 0;
		}
		if (!strcasecmp (val, "true")
		    || !strcasecmp (val, "on"))
			buf [0] = 1;
		else if (!strcasecmp (val, "false")
			 || !strcasecmp (val, "off"))
			buf [0] = 0;
		else if (!strcasecmp (val, "ignore"))
			buf [0] = 2;
		else {
			if ((*fmt) [1] != 'o')
				parse_warn (cfile, "expecting boolean.");
			goto bad_flag;
		}
		token = next_token (&val, (unsigned *)0, cfile);
		if (!make_const_data (&t, buf, 1, 0, 1, MDL))
			return 0;
		break;

	      default:
		parse_warn (cfile, "Bad format %c in parse_option_token.",
			    **fmt);
		skip_to_semi (cfile);
		return 0;
	}
	if (expr) {
		if (!make_concat (rv, expr, t))
			return 0;
	} else
		expression_reference (rv, t, MDL);
	expression_dereference (&t, MDL);
	return 1;
}

int parse_option_decl (oc, cfile)
	struct option_cache **oc;
	struct parse *cfile;
{
	const char *val;
	int token;
	u_int8_t buf [4];
	u_int8_t hunkbuf [1024];
	unsigned hunkix = 0;
	const char *fmt, *f;
	struct option *option;
	struct iaddr ip_addr;
	u_int8_t *dp;
	unsigned len;
	int nul_term = 0;
	struct buffer *bp;
	int known = 0;
	struct enumeration_value *e;

	option = parse_option_name (cfile, 0, &known);
	if (!option)
		return 0;

	/* Parse the option data... */
	do {
		/* Set a flag if this is an array of a simple type (i.e.,
		   not an array of pairs of IP addresses, or something
		   like that. */
		int uniform = option -> format [1] == 'A';

		for (fmt = option -> format; *fmt; fmt++) {
			if (*fmt == 'A')
				break;
			switch (*fmt) {
			      case 'E':
				fmt = strchr (fmt, '.');
				if (!fmt) {
					parse_warn (cfile,
						    "malformed %s (bug!)",
						    "encapsulation format");
					skip_to_semi (cfile);
					return 0;
				}
			      case 'X':
				len = parse_X (cfile, &hunkbuf [hunkix],
					       sizeof hunkbuf - hunkix);
				hunkix += len;
				break;
					
			      case 't': /* Text string... */
				token = next_token (&val,
						    &len, cfile);
				if (token != STRING) {
					parse_warn (cfile,
						    "expecting string.");
					skip_to_semi (cfile);
					return 0;
				}
				if (hunkix + len + 1 > sizeof hunkbuf) {
					parse_warn (cfile,
						    "option data buffer %s",
						    "overflow");
					skip_to_semi (cfile);
					return 0;
				}
				memcpy (&hunkbuf [hunkix], val, len + 1);
				nul_term = 1;
				hunkix += len;
				break;

			      case 'N':
				f = fmt;
				fmt = strchr (fmt, '.');
				if (!fmt) {
					parse_warn (cfile,
						    "malformed %s (bug!)",
						    "enumeration format");
				      foo:
					skip_to_semi (cfile);
					return 0;
				}
				token = next_token (&val,
						    (unsigned *)0, cfile);
				if (!is_identifier (token)) {
					parse_warn (cfile,
						    "identifier expected");
					goto foo;
				}
				e = find_enumeration_value (f, fmt - f, val);
				if (!e) {
					parse_warn (cfile,
						    "unknown value");
					goto foo;
				}
				len = 1;
				dp = &e -> value;
				goto alloc;

			      case 'I': /* IP address. */
				if (!parse_ip_addr (cfile, &ip_addr))
					return 0;
				len = ip_addr.len;
				dp = ip_addr.iabuf;

			      alloc:
				if (hunkix + len > sizeof hunkbuf) {
					parse_warn (cfile,
						    "option data buffer %s",
						    "overflow");
					skip_to_semi (cfile);
					return 0;
				}
				memcpy (&hunkbuf [hunkix], dp, len);
				hunkix += len;
				break;

			      case 'L': /* Unsigned 32-bit integer... */
			      case 'l':	/* Signed 32-bit integer... */
				token = next_token (&val,
						    (unsigned *)0, cfile);
				if (token != NUMBER) {
				      need_number:
					parse_warn (cfile,
						    "expecting number.");
					if (token != SEMI)
						skip_to_semi (cfile);
					return 0;
				}
				convert_num (cfile, buf, val, 0, 32);
				len = 4;
				dp = buf;
				goto alloc;

			      case 's':	/* Signed 16-bit integer. */
			      case 'S':	/* Unsigned 16-bit integer. */
				token = next_token (&val,
						    (unsigned *)0, cfile);
				if (token != NUMBER)
					goto need_number;
				convert_num (cfile, buf, val, 0, 16);
				len = 2;
				dp = buf;
				goto alloc;

			      case 'b':	/* Signed 8-bit integer. */
			      case 'B':	/* Unsigned 8-bit integer. */
				token = next_token (&val,
						    (unsigned *)0, cfile);
				if (token != NUMBER)
					goto need_number;
				convert_num (cfile, buf, val, 0, 8);
				len = 1;
				dp = buf;
				goto alloc;

			      case 'f': /* Boolean flag. */
				token = next_token (&val,
						    (unsigned *)0, cfile);
				if (!is_identifier (token)) {
					parse_warn (cfile,
						    "expecting identifier.");
				      bad_flag:
					if (token != SEMI)
						skip_to_semi (cfile);
					return 0;
				}
				if (!strcasecmp (val, "true")
				    || !strcasecmp (val, "on"))
					buf [0] = 1;
				else if (!strcasecmp (val, "false")
					 || !strcasecmp (val, "off"))
					buf [0] = 0;
				else {
					parse_warn (cfile,
						    "expecting boolean.");
					goto bad_flag;
				}
				len = 1;
				dp = buf;
				goto alloc;

			      default:
				log_error ("parse_option_param: Bad format %c",
				      *fmt);
				skip_to_semi (cfile);
				return 0;
			}
		}
		token = next_token (&val, (unsigned *)0, cfile);
	} while (*fmt == 'A' && token == COMMA);

	if (token != SEMI) {
		parse_warn (cfile, "semicolon expected.");
		skip_to_semi (cfile);
		return 0;
	}

	bp = (struct buffer *)0;
	if (!buffer_allocate (&bp, hunkix + nul_term, MDL))
		log_fatal ("no memory to store option declaration.");
	if (!bp -> data)
		log_fatal ("out of memory allocating option data.");
	memcpy (bp -> data, hunkbuf, hunkix + nul_term);
	
	if (!option_cache_allocate (oc, MDL))
		log_fatal ("out of memory allocating option cache.");

	(*oc) -> data.buffer = bp;
	(*oc) -> data.data = &bp -> data [0];
	(*oc) -> data.terminated = nul_term;
	(*oc) -> data.len = hunkix;
	(*oc) -> option = option;
	return 1;
}

/* Consider merging parse_cshl into this. */

int parse_X (cfile, buf, max)
	struct parse *cfile;
	u_int8_t *buf;
	unsigned max;
{
	int token;
	const char *val;
	unsigned len;
	u_int8_t *s;

	token = peek_token (&val, (unsigned *)0, cfile);
	if (token == NUMBER_OR_NAME || token == NUMBER) {
		len = 0;
		do {
			token = next_token (&val, (unsigned *)0, cfile);
			if (token != NUMBER && token != NUMBER_OR_NAME) {
				parse_warn (cfile,
					    "expecting hexadecimal constant.");
				skip_to_semi (cfile);
				return 0;
			}
			convert_num (cfile, &buf [len], val, 16, 8);
			if (len++ > max) {
				parse_warn (cfile,
					    "hexadecimal constant too long.");
				skip_to_semi (cfile);
				return 0;
			}
			token = peek_token (&val, (unsigned *)0, cfile);
			if (token == COLON)
				token = next_token (&val,
						    (unsigned *)0, cfile);
		} while (token == COLON);
		val = (char *)buf;
	} else if (token == STRING) {
		token = next_token (&val, &len, cfile);
		if (len + 1 > max) {
			parse_warn (cfile, "string constant too long.");
			skip_to_semi (cfile);
			return 0;
		}
		memcpy (buf, val, len + 1);
	} else {
		parse_warn (cfile, "expecting string or hexadecimal data");
		skip_to_semi (cfile);
		return 0;
	}
	return len;
}

int parse_warn (struct parse *cfile, const char *fmt, ...)
{
	va_list list;
	char lexbuf [256];
	char mbuf [1024];
	char fbuf [1024];
	unsigned i, lix;
	
	do_percentm (mbuf, fmt);
#ifndef NO_SNPRINTF
	snprintf (fbuf, sizeof fbuf, "%s line %d: %s",
		  cfile -> tlname, cfile -> lexline, mbuf);
#else
	sprintf (fbuf, "%s line %d: %s",
		 cfile -> tlname, cfile -> lexline, mbuf);
#endif
	
	va_start (list, fmt);
	vsnprintf (mbuf, sizeof mbuf, fbuf, list);
	va_end (list);

	lix = 0;
	for (i = 0;
	     cfile -> token_line [i] && i < (cfile -> lexchar - 1); i++) {
		if (lix < (sizeof lexbuf) - 1)
			lexbuf [lix++] = ' ';
		if (cfile -> token_line [i] == '\t') {
			for (lix;
			     lix < (sizeof lexbuf) - 1 && (lix & 7); lix++)
				lexbuf [lix] = ' ';
		}
	}
	lexbuf [lix] = 0;

#ifndef DEBUG
	syslog (log_priority | LOG_ERR, "%s", mbuf);
	syslog (log_priority | LOG_ERR, "%s", cfile -> token_line);
	if (cfile -> lexchar < 81)
		syslog (log_priority | LOG_ERR, "%s^", lexbuf);
#endif

	if (log_perror) {
		write (2, mbuf, strlen (mbuf));
		write (2, "\n", 1);
		write (2, cfile -> token_line, strlen (cfile -> token_line));
		write (2, "\n", 1);
		if (cfile -> lexchar < 81)
			write (2, lexbuf, lix);
		write (2, "^\n", 2);
	}

	cfile -> warnings_occurred = 1;

	return 0;
}
