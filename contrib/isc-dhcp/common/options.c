/* options.c

   DHCP options parsing and reassembly. */

/*
 * Copyright (c) 2004 by Internet Systems Consortium, Inc. ("ISC")
 * Copyright (c) 1995-2003 by Internet Software Consortium
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 *   Internet Systems Consortium, Inc.
 *   950 Charter Street
 *   Redwood City, CA 94063
 *   <info@isc.org>
 *   http://www.isc.org/
 *
 * This software has been written for Internet Systems Consortium
 * by Ted Lemon in cooperation with Vixie Enterprises and Nominum, Inc.
 * To learn more about Internet Systems Consortium, see
 * ``http://www.isc.org/''.  To learn more about Vixie Enterprises,
 * see ``http://www.vix.com''.   To learn more about Nominum, Inc., see
 * ``http://www.nominum.com''.
 */

#ifndef lint
static char copyright[] =
"$Id: options.c,v 1.85.2.13 2004/06/10 17:59:19 dhankins Exp $ Copyright (c) 2004 Internet Systems Consortium.  All rights reserved.\n";
#endif /* not lint */

#define DHCP_OPTION_DATA
#include "dhcpd.h"
#include <omapip/omapip_p.h>

struct option *vendor_cfg_option;

static void do_option_set PROTO ((pair *,
				  struct option_cache *,
				  enum statement_op));

/* Parse all available options out of the specified packet. */

int parse_options (packet)
	struct packet *packet;
{
	int i;
	struct option_cache *op = (struct option_cache *)0;

	/* Allocate a new option state. */
	if (!option_state_allocate (&packet -> options, MDL)) {
		packet -> options_valid = 0;
		return 0;
	}

	/* If we don't see the magic cookie, there's nothing to parse. */
	if (memcmp (packet -> raw -> options, DHCP_OPTIONS_COOKIE, 4)) {
		packet -> options_valid = 0;
		return 1;
	}

	/* Go through the options field, up to the end of the packet
	   or the End field. */
	if (!parse_option_buffer (packet -> options,
				  &packet -> raw -> options [4],
				  (packet -> packet_length -
				   DHCP_FIXED_NON_UDP - 4),
				  &dhcp_universe))
		return 0;

	/* If we parsed a DHCP Option Overload option, parse more
	   options out of the buffer(s) containing them. */
	if (packet -> options_valid &&
	    (op = lookup_option (&dhcp_universe, packet -> options,
				 DHO_DHCP_OPTION_OVERLOAD))) {
		if (op -> data.data [0] & 1) {
			if (!parse_option_buffer
			    (packet -> options,
			     (unsigned char *)packet -> raw -> file,
			     sizeof packet -> raw -> file,
			     &dhcp_universe))
				return 0;
		}
		if (op -> data.data [0] & 2) {
			if (!parse_option_buffer
			    (packet -> options,
			     (unsigned char *)packet -> raw -> sname,
			     sizeof packet -> raw -> sname,
			     &dhcp_universe))
				return 0;
		}
	}
	packet -> options_valid = 1;
	return 1;
}

/* Parse options out of the specified buffer, storing addresses of option
   values in packet -> options and setting packet -> options_valid if no
   errors are encountered. */

int parse_option_buffer (options, buffer, length, universe)
	struct option_state *options;
	const unsigned char *buffer;
	unsigned length;
	struct universe *universe;
{
	unsigned char *t;
	const unsigned char *end = buffer + length;
	unsigned len, offset;
	int code;
	struct option_cache *op = (struct option_cache *)0;
	struct buffer *bp = (struct buffer *)0;

	if (!buffer_allocate (&bp, length, MDL)) {
		log_error ("no memory for option buffer.");
		return 0;
	}
	memcpy (bp -> data, buffer, length);
	
	for (offset = 0; buffer [offset] != DHO_END && offset < length; ) {
		code = buffer [offset];
		/* Pad options don't have a length - just skip them. */
		if (code == DHO_PAD) {
			++offset;
			continue;
		}

		/* Don't look for length if the buffer isn't that big. */
		if (offset + 2 > length) {
			len = 65536;
			goto bogus;
		}

		/* All other fields (except end, see above) have a
		   one-byte length. */
		len = buffer [offset + 1];

		/* If the length is outrageous, the options are bad. */
		if (offset + len + 2 > length) {
		      bogus:
			log_error ("parse_option_buffer: option %s (%d) %s.",
				   dhcp_options [code].name, len,
				   "larger than buffer");
			buffer_dereference (&bp, MDL);
			return 0;
		}

		/* If the option contains an encapsulation, parse it.   If
		   the parse fails, or the option isn't an encapsulation (by
		   far the most common case), or the option isn't entirely
		   an encapsulation, keep the raw data as well. */
		if (universe -> options [code] &&
		    !((universe -> options [code] -> format [0] == 'e' ||
		       universe -> options [code] -> format [0] == 'E') &&
		      (parse_encapsulated_suboptions
		       (options, universe -> options [code],
			buffer + offset + 2, len,
			universe, (const char *)0)))) {
		    op = lookup_option (universe, options, code);
		    if (op) {
			struct data_string new;
			memset (&new, 0, sizeof new);
			if (!buffer_allocate (&new.buffer, op -> data.len + len,
					      MDL)) {
			    log_error ("parse_option_buffer: No memory.");
			    return 0;
			}
			memcpy (new.buffer -> data, op -> data.data,
				op -> data.len);
			memcpy (&new.buffer -> data [op -> data.len],
				&bp -> data [offset + 2], len);
			new.len = op -> data.len + len;
			new.data = new.buffer -> data;
			data_string_forget (&op -> data, MDL);
			data_string_copy (&op -> data, &new, MDL);
			data_string_forget (&new, MDL);
		    } else {
			save_option_buffer (universe, options, bp,
					    &bp -> data [offset + 2], len,
					    universe -> options [code], 1);
		    }
		}
		offset += len + 2;
	}
	buffer_dereference (&bp, MDL);
	return 1;
}

/* If an option in an option buffer turns out to be an encapsulation,
   figure out what to do.   If we don't know how to de-encapsulate it,
   or it's not well-formed, return zero; otherwise, return 1, indicating
   that we succeeded in de-encapsulating it. */

struct universe *find_option_universe (struct option *eopt, const char *uname)
{
	int i;
	char *s, *t;
	struct universe *universe = (struct universe *)0;

	/* Look for the E option in the option format. */
	s = strchr (eopt -> format, 'E');
	if (!s) {
		log_error ("internal encapsulation format error 1.");
		return 0;
	}
	/* Look for the universe name in the option format. */
	t = strchr (++s, '.');
	/* If there was no trailing '.', or there's something after the
	   trailing '.', the option is bogus and we can't use it. */
	if (!t || t [1]) {
		log_error ("internal encapsulation format error 2.");
		return 0;
	}
	if (t == s && uname) {
		for (i = 0; i < universe_count; i++) {
			if (!strcmp (universes [i] -> name, uname)) {
				universe = universes [i];
				break;
			}
		}
	} else if (t != s) {
		for (i = 0; i < universe_count; i++) {
			if (strlen (universes [i] -> name) == t - s &&
			    !memcmp (universes [i] -> name,
				     s, (unsigned)(t - s))) {
				universe = universes [i];
				break;
			}
		}
	}
	return universe;
}

/* If an option in an option buffer turns out to be an encapsulation,
   figure out what to do.   If we don't know how to de-encapsulate it,
   or it's not well-formed, return zero; otherwise, return 1, indicating
   that we succeeded in de-encapsulating it. */

int parse_encapsulated_suboptions (struct option_state *options,
				   struct option *eopt,
				   const unsigned char *buffer,
				   unsigned len, struct universe *eu,
				   const char *uname)
{
	int i;
	struct universe *universe = find_option_universe (eopt, uname);

	/* If we didn't find the universe, we can't do anything with it
	   right now (e.g., we can't decode vendor options until we've
	   decoded the packet and executed the scopes that it matches). */
	if (!universe)
		return 0;
		
	/* If we don't have a decoding function for it, we can't decode
	   it. */
	if (!universe -> decode)
		return 0;

	i = (*universe -> decode) (options, buffer, len, universe);

	/* If there is stuff before the suboptions, we have to keep it. */
	if (eopt -> format [0] != 'E')
		return 0;
	/* Otherwise, return the status of the decode function. */
	return i;
}

int fqdn_universe_decode (struct option_state *options,
			  const unsigned char *buffer,
			  unsigned length, struct universe *u)
{
	char *name;
	struct buffer *bp = (struct buffer *)0;

	/* FQDN options have to be at least four bytes long. */
	if (length < 3)
		return 0;

	/* Save the contents of the option in a buffer. */
	if (!buffer_allocate (&bp, length + 4, MDL)) {
		log_error ("no memory for option buffer.");
		return 0;
	}
	memcpy (&bp -> data [3], buffer + 1, length - 1);

	if (buffer [0] & 4)	/* encoded */
		bp -> data [0] = 1;
	else
		bp -> data [0] = 0;
	if (!save_option_buffer (&fqdn_universe, options, bp,
				 &bp -> data [0], 1,
				 &fqdn_options [FQDN_ENCODED], 0)) {
	      bad:
		buffer_dereference (&bp, MDL);
		return 0;
	}

	if (buffer [0] & 1)	/* server-update */
		bp -> data [2] = 1;
	else
		bp -> data [2] = 0;
	if (buffer [0] & 2)	/* no-client-update */
		bp -> data [1] = 1;
	else
		bp -> data [1] = 0;

	/* XXX Ideally we should store the name in DNS format, so if the
	   XXX label isn't in DNS format, we convert it to DNS format,
	   XXX rather than converting labels specified in DNS format to
	   XXX the plain ASCII representation.   But that's hard, so
	   XXX not now. */

	/* Not encoded using DNS format? */
	if (!bp -> data [0]) {
		unsigned i;

		/* Some broken clients NUL-terminate this option. */
		if (buffer [length - 1] == 0) {
			--length;
			bp -> data [1] = 1;
		}

		/* Determine the length of the hostname component of the
		   name.  If the name contains no '.' character, it
		   represents a non-qualified label. */
		for (i = 3; i < length && buffer [i] != '.'; i++);
		i -= 3;

		/* Note: If the client sends a FQDN, the first '.' will
		   be used as a NUL terminator for the hostname. */
		if (i)
			if (!save_option_buffer (&fqdn_universe, options, bp,
						 &bp -> data[5], i,
						 &fqdn_options [FQDN_HOSTNAME],
						 0))
			goto bad;
		/* Note: If the client sends a single label, the
		   FQDN_DOMAINNAME option won't be set. */
		if (length > 4 + i &&
		    !save_option_buffer (&fqdn_universe, options, bp,
					 &bp -> data[6 + i], length - 4 - i,
					 &fqdn_options [FQDN_DOMAINNAME], 1))
			goto bad;
		/* Also save the whole name. */
		if (length > 3)
			if (!save_option_buffer (&fqdn_universe, options, bp,
						 &bp -> data [5], length - 3,
						 &fqdn_options [FQDN_FQDN], 1))
				goto bad;
	} else {
		unsigned len;
		unsigned total_len = 0;
		unsigned first_len = 0;
		int terminated = 0;
		unsigned char *s;

		s = &bp -> data[5];

		while (s < &bp -> data[0] + length + 2) {
			len = *s;
			if (len > 63) {
				log_info ("fancy bits in fqdn option");
				return 0;
			}	
			if (len == 0) {
				terminated = 1;
				break;
			}
			if (s + len > &bp -> data [0] + length + 3) {
				log_info ("fqdn tag longer than buffer");
				return 0;
			}

			if (first_len == 0) {
				first_len = len;
			}

			*s = '.';
			s += len + 1;
			total_len += len + 1;
		}

		/* We wind up with a length that's one too many because
		   we shouldn't increment for the last label, but there's
		   no way to tell we're at the last label until we exit
		   the loop.   :'*/
		if (total_len > 0)
			total_len--;

		if (!terminated) {
			first_len = total_len;
		}

		if (first_len > 0 &&
		    !save_option_buffer (&fqdn_universe, options, bp,
					 &bp -> data[6], first_len,
					 &fqdn_options [FQDN_HOSTNAME], 0))
			goto bad;
		if (total_len > 0 && first_len != total_len) {
			if (!save_option_buffer
			    (&fqdn_universe, options, bp,
			     &bp -> data[6 + first_len], total_len - first_len,
			     &fqdn_options [FQDN_DOMAINNAME], 1))
				goto bad;
		}
		if (total_len > 0)
			if (!save_option_buffer (&fqdn_universe, options, bp,
						 &bp -> data [6], total_len,
						 &fqdn_options [FQDN_FQDN], 1))
				goto bad;
	}

	if (!save_option_buffer (&fqdn_universe, options, bp,
				 &bp -> data [1], 1,
				 &fqdn_options [FQDN_NO_CLIENT_UPDATE], 0))
	    goto bad;
	if (!save_option_buffer (&fqdn_universe, options, bp,
				 &bp -> data [2], 1,
				 &fqdn_options [FQDN_SERVER_UPDATE], 0))
		goto bad;

	if (!save_option_buffer (&fqdn_universe, options, bp,
				 &bp -> data [3], 1,
				 &fqdn_options [FQDN_RCODE1], 0))
		goto bad;
	if (!save_option_buffer (&fqdn_universe, options, bp,
				 &bp -> data [4], 1,
				 &fqdn_options [FQDN_RCODE2], 0))
		goto bad;

	buffer_dereference (&bp, MDL);
	return 1;
}

/* cons options into a big buffer, and then split them out into the
   three seperate buffers if needed.  This allows us to cons up a set
   of vendor options using the same routine. */

int cons_options (inpacket, outpacket, lease, client_state,
		  mms, in_options, cfg_options,
		  scope, overload, terminate, bootpp, prl, vuname)
	struct packet *inpacket;
	struct dhcp_packet *outpacket;
	struct lease *lease;
	struct client_state *client_state;
	int mms;
	struct option_state *in_options;
	struct option_state *cfg_options;
	struct binding_scope **scope;
	int overload;	/* Overload flags that may be set. */
	int terminate;
	int bootpp;
	struct data_string *prl;
	const char *vuname;
{
#define PRIORITY_COUNT 300
	unsigned priority_list [PRIORITY_COUNT];
	int priority_len;
	unsigned char buffer [4096];	/* Really big buffer... */
	unsigned main_buffer_size;
	unsigned mainbufix, bufix, agentix;
	unsigned option_size;
	unsigned length;
	int i;
	struct option_cache *op;
	struct data_string ds;
	pair pp, *hash;
	int need_endopt = 0;
	int have_sso = 0;

	memset (&ds, 0, sizeof ds);

	/* If there's a Maximum Message Size option in the incoming packet
	   and no alternate maximum message size has been specified, take the
	   one in the packet. */

	if (inpacket &&
	    (op = lookup_option (&dhcp_universe, inpacket -> options,
				 DHO_DHCP_MAX_MESSAGE_SIZE))) {
		evaluate_option_cache (&ds, inpacket,
				       lease, client_state, in_options,
				       cfg_options, scope, op, MDL);
		if (ds.len >= sizeof (u_int16_t)) {
			i = getUShort (ds.data);

			if(!mms || (i < mms))
				mms = i;
		}
		data_string_forget (&ds, MDL);
	}

	/* If the client has provided a maximum DHCP message size,
	   use that; otherwise, if it's BOOTP, only 64 bytes; otherwise
	   use up to the minimum IP MTU size (576 bytes). */
	/* XXX if a BOOTP client specifies a max message size, we will
	   honor it. */

	if (mms) {
		main_buffer_size = mms - DHCP_FIXED_LEN;

		/* Enforce a minimum packet size... */
		if (main_buffer_size < (576 - DHCP_FIXED_LEN))
			main_buffer_size = 576 - DHCP_FIXED_LEN;
	} else if (bootpp) {
		if (inpacket) {
			main_buffer_size =
				inpacket -> packet_length - DHCP_FIXED_LEN;
			if (main_buffer_size < 64)
				main_buffer_size = 64;
		} else
			main_buffer_size = 64;
	} else
		main_buffer_size = 576 - DHCP_FIXED_LEN;

	/* Set a hard limit at the size of the output buffer. */
	if (main_buffer_size > sizeof buffer)
		main_buffer_size = sizeof buffer;

	/* Preload the option priority list with mandatory options. */
	priority_len = 0;
	priority_list [priority_len++] = DHO_DHCP_MESSAGE_TYPE;
	priority_list [priority_len++] = DHO_DHCP_SERVER_IDENTIFIER;
	priority_list [priority_len++] = DHO_DHCP_LEASE_TIME;
	priority_list [priority_len++] = DHO_DHCP_MESSAGE;
	priority_list [priority_len++] = DHO_DHCP_REQUESTED_ADDRESS;
	priority_list [priority_len++] = DHO_FQDN;

	if (prl && prl -> len > 0) {
		if ((op = lookup_option (&dhcp_universe, cfg_options,
					 DHO_SUBNET_SELECTION))) {
			if (priority_len < PRIORITY_COUNT)
				priority_list [priority_len++] =
					DHO_SUBNET_SELECTION;
		}
			    
		data_string_truncate (prl, (PRIORITY_COUNT - priority_len));

		for (i = 0; i < prl -> len; i++) {
			/* Prevent client from changing order of delivery
			   of relay agent information option. */
			if (prl -> data [i] != DHO_DHCP_AGENT_OPTIONS)
				priority_list [priority_len++] =
					prl -> data [i];
		}
	} else {
		/* First, hardcode some more options that ought to be
		   sent first... */
		priority_list [priority_len++] = DHO_SUBNET_MASK;
		priority_list [priority_len++] = DHO_ROUTERS;
		priority_list [priority_len++] = DHO_DOMAIN_NAME_SERVERS;
		priority_list [priority_len++] = DHO_HOST_NAME;

		/* Append a list of the standard DHCP options from the
		   standard DHCP option space.  Actually, if a site
		   option space hasn't been specified, we wind up
		   treating the dhcp option space as the site option
		   space, and the first for loop is skipped, because
		   it's slightly more general to do it this way,
		   taking the 1Q99 DHCP futures work into account. */
		if (cfg_options -> site_code_min) {
		    for (i = 0; i < OPTION_HASH_SIZE; i++) {
			hash = cfg_options -> universes [dhcp_universe.index];
			if (hash) {
			    for (pp = hash [i]; pp; pp = pp -> cdr) {
				op = (struct option_cache *)(pp -> car);
				if (op -> option -> code <
				    cfg_options -> site_code_min &&
				    priority_len < PRIORITY_COUNT &&
				    (op -> option -> code !=
				     DHO_DHCP_AGENT_OPTIONS))
					priority_list [priority_len++] =
						op -> option -> code;
			    }
			}
		    }
		}

		/* Now cycle through the site option space, or if there
		   is no site option space, we'll be cycling through the
		   dhcp option space. */
		for (i = 0; i < OPTION_HASH_SIZE; i++) {
		    hash = (cfg_options -> universes
			    [cfg_options -> site_universe]);
		    if (hash)
			for (pp = hash [i]; pp; pp = pp -> cdr) {
				op = (struct option_cache *)(pp -> car);
				if (op -> option -> code >=
				    cfg_options -> site_code_min &&
				    priority_len < PRIORITY_COUNT &&
				    (op -> option -> code !=
				     DHO_DHCP_AGENT_OPTIONS))
					priority_list [priority_len++] =
						op -> option -> code;
			}
		}

		/* Now go through all the universes for which options
		   were set and see if there are encapsulations for
		   them; if there are, put the encapsulation options
		   on the priority list as well. */
		for (i = 0; i < cfg_options -> universe_count; i++) {
		    if (cfg_options -> universes [i] &&
			universes [i] -> enc_opt &&
			priority_len < PRIORITY_COUNT &&
			universes [i] -> enc_opt -> universe == &dhcp_universe)
		    {
			    if (universes [i] -> enc_opt -> code !=
				DHO_DHCP_AGENT_OPTIONS)
				    priority_list [priority_len++] =
					    universes [i] -> enc_opt -> code;
		    }
		}

		/* The vendor option space can't stand on its own, so always
		   add it to the list. */
		if (priority_len < PRIORITY_COUNT)
			priority_list [priority_len++] =
				DHO_VENDOR_ENCAPSULATED_OPTIONS;
	}

	/* Copy the options into the big buffer... */
	option_size = store_options (buffer,
				     (main_buffer_size - 7 +
				      ((overload & 1) ? DHCP_FILE_LEN : 0) +
				      ((overload & 2) ? DHCP_SNAME_LEN : 0)),
				     inpacket, lease, client_state,
				     in_options, cfg_options, scope,
				     priority_list, priority_len,
				     main_buffer_size,
				     (main_buffer_size +
				      ((overload & 1) ? DHCP_FILE_LEN : 0)),
				     terminate, vuname);

	/* Put the cookie up front... */
	memcpy (outpacket -> options, DHCP_OPTIONS_COOKIE, 4);
	mainbufix = 4;

	/* If we're going to have to overload, store the overload
	   option at the beginning.  If we can, though, just store the
	   whole thing in the packet's option buffer and leave it at
	   that. */
	if (option_size <= main_buffer_size - mainbufix) {
		memcpy (&outpacket -> options [mainbufix],
			buffer, option_size);
		mainbufix += option_size;
		agentix = mainbufix;
		if (mainbufix < main_buffer_size)
			need_endopt = 1;
		length = DHCP_FIXED_NON_UDP + mainbufix;
	} else {
		outpacket -> options [mainbufix++] = DHO_DHCP_OPTION_OVERLOAD;
		outpacket -> options [mainbufix++] = 1;
		if (option_size > main_buffer_size - mainbufix + DHCP_FILE_LEN)
			outpacket -> options [mainbufix++] = 3;
		else
			outpacket -> options [mainbufix++] = 1;

		memcpy (&outpacket -> options [mainbufix],
			buffer, main_buffer_size - mainbufix);
		length = DHCP_FIXED_NON_UDP + main_buffer_size;
		agentix = main_buffer_size;

		bufix = main_buffer_size - mainbufix;
		if (overload & 1) {
			if (option_size - bufix <= DHCP_FILE_LEN) {
				memcpy (outpacket -> file,
					&buffer [bufix], option_size - bufix);
				mainbufix = option_size - bufix;
				if (mainbufix < DHCP_FILE_LEN)
					outpacket -> file [mainbufix++]
						= DHO_END;
				while (mainbufix < DHCP_FILE_LEN)
					outpacket -> file [mainbufix++]
						= DHO_PAD;
			} else {
				memcpy (outpacket -> file,
					&buffer [bufix], DHCP_FILE_LEN);
				bufix += DHCP_FILE_LEN;
			}
		}
		if ((overload & 2) && option_size < bufix) {
			memcpy (outpacket -> sname,
				&buffer [bufix], option_size - bufix);

			mainbufix = option_size - bufix;
			if (mainbufix < DHCP_SNAME_LEN)
				outpacket -> file [mainbufix++]
					= DHO_END;
			while (mainbufix < DHCP_SNAME_LEN)
				outpacket -> file [mainbufix++]
					= DHO_PAD;
		}
	}

	/* Now hack in the agent options if there are any. */
	priority_list [0] = DHO_DHCP_AGENT_OPTIONS;
	priority_len = 1;
	agentix +=
		store_options (&outpacket -> options [agentix],
			       1500 - DHCP_FIXED_LEN - agentix,
			       inpacket, lease, client_state,
			       in_options, cfg_options, scope,
			       priority_list, priority_len,
			       1500 - DHCP_FIXED_LEN - agentix,
			       1500 - DHCP_FIXED_LEN - agentix, 0, (char *)0);

	/* Tack a DHO_END option onto the packet if we need to. */
	if (agentix < 1500 - DHCP_FIXED_LEN && need_endopt)
		outpacket -> options [agentix++] = DHO_END;

	/* Figure out the length. */
	length = DHCP_FIXED_NON_UDP + agentix;
	return length;
}

/* Store all the requested options into the requested buffer. */

int store_options (buffer, buflen, packet, lease, client_state,
		   in_options, cfg_options, scope, priority_list, priority_len,
		   first_cutoff, second_cutoff, terminate, vuname)
	unsigned char *buffer;
	unsigned buflen;
	struct packet *packet;
	struct lease *lease;
	struct client_state *client_state;
	struct option_state *in_options;
	struct option_state *cfg_options;
	struct binding_scope **scope;
	unsigned *priority_list;
	int priority_len;
	unsigned first_cutoff, second_cutoff;
	int terminate;
	const char *vuname;
{
	int bufix = 0;
	int i;
	int ix;
	int tto;
	struct data_string od;
	struct option_cache *oc;
	unsigned code;
	int optstart;

	memset (&od, 0, sizeof od);

	/* Eliminate duplicate options in the parameter request list.
	   There's got to be some clever knuthian way to do this:
	   Eliminate all but the first occurance of a value in an array
	   of values without otherwise disturbing the order of the array. */
	for (i = 0; i < priority_len - 1; i++) {
		tto = 0;
		for (ix = i + 1; ix < priority_len + tto; ix++) {
			if (tto)
				priority_list [ix - tto] =
					priority_list [ix];
			if (priority_list [i] == priority_list [ix]) {
				tto++;
				priority_len--;
			}
		}
	}

	/* Copy out the options in the order that they appear in the
	   priority list... */
	for (i = 0; i < priority_len; i++) {
	    /* Number of bytes left to store (some may already
	       have been stored by a previous pass). */
	    unsigned length;
	    int optstart;
	    struct universe *u;
	    int have_encapsulation = 0;
	    struct data_string encapsulation;

	    memset (&encapsulation, 0, sizeof encapsulation);

	    /* Code for next option to try to store. */
	    code = priority_list [i];
	    
	    /* Look up the option in the site option space if the code
	       is above the cutoff, otherwise in the DHCP option space. */
	    if (code >= cfg_options -> site_code_min)
		    u = universes [cfg_options -> site_universe];
	    else
		    u = &dhcp_universe;

	    oc = lookup_option (u, cfg_options, code);

	    /* It's an encapsulation, try to find the universe
	       to be encapsulated first, except that if it's a straight
	       encapsulation and the user has provided a value for the
	       encapsulation option, use the user-provided value. */
	    if (u -> options [code] &&
		((u -> options [code] -> format [0] == 'E' && !oc) ||
		 u -> options [code] -> format [0] == 'e')) {
		int uix;
		static char *s, *t;
		struct option_cache *tmp;
		struct data_string name;

		s = strchr (u -> options [code] -> format, 'E');
		if (s)
		    t = strchr (++s, '.');
		if (s && t) {
		    memset (&name, 0, sizeof name);

		    /* A zero-length universe name means the vendor
		       option space, if one is defined. */
		    if (t == s) {
			if (vendor_cfg_option) {
			    tmp = lookup_option (vendor_cfg_option -> universe,
						 cfg_options,
						 vendor_cfg_option -> code);
			    if (tmp)
				evaluate_option_cache (&name, packet, lease,
						       client_state,
						       in_options,
						       cfg_options,
						       scope, tmp, MDL);
			} else if (vuname) {
			    name.data = (unsigned char *)s;
			    name.len = strlen (s);
			}
		    } else {
			name.data = (unsigned char *)s;
			name.len = t - s;
		    }
			
		    /* If we found a universe, and there are options configured
		       for that universe, try to encapsulate it. */
		    if (name.len) {
			have_encapsulation =
				(option_space_encapsulate
				 (&encapsulation, packet, lease, client_state,
				  in_options, cfg_options, scope, &name));
			data_string_forget (&name, MDL);
		    }
		}
	    }

	    /* In order to avoid memory leaks, we have to get to here
	       with any option cache that we allocated in tmp not being
	       referenced by tmp, and whatever option cache is referenced
	       by oc being an actual reference.   lookup_option doesn't
	       generate a reference (this needs to be fixed), so the
	       preceding goop ensures that if we *didn't* generate a new
	       option cache, oc still winds up holding an actual reference. */

	    /* If no data is available for this option, skip it. */
	    if (!oc && !have_encapsulation) {
		    continue;
	    }
	    
	    /* Find the value of the option... */
	    if (oc) {
		evaluate_option_cache (&od, packet,
				       lease, client_state, in_options,
				       cfg_options, scope, oc, MDL);
		if (!od.len) {
		    data_string_forget (&encapsulation, MDL);
		    data_string_forget (&od, MDL);
		    have_encapsulation = 0;
		    continue;
		}
	    }

	    /* We should now have a constant length for the option. */
	    length = od.len;
	    if (have_encapsulation) {
		    length += encapsulation.len;
		    if (!od.len) {
			    data_string_copy (&od, &encapsulation, MDL);
			    data_string_forget (&encapsulation, MDL);
		    } else {
			    struct buffer *bp = (struct buffer *)0;
			    if (!buffer_allocate (&bp, length, MDL)) {
				    option_cache_dereference (&oc, MDL);
				    data_string_forget (&od, MDL);
				    data_string_forget (&encapsulation, MDL);
				    continue;
			    }
			    memcpy (&bp -> data [0], od.data, od.len);
			    memcpy (&bp -> data [od.len], encapsulation.data,
				    encapsulation.len);
			    data_string_forget (&od, MDL);
			    data_string_forget (&encapsulation, MDL);
			    od.data = &bp -> data [0];
			    buffer_reference (&od.buffer, bp, MDL);
			    buffer_dereference (&bp, MDL);
			    od.len = length;
			    od.terminated = 0;
		    }
	    }

	    /* Do we add a NUL? */
	    if (terminate && dhcp_options [code].format [0] == 't') {
		    length++;
		    tto = 1;
	    } else {
		    tto = 0;
	    }

	    /* Try to store the option. */
	    
	    /* If the option's length is more than 255, we must store it
	       in multiple hunks.   Store 255-byte hunks first.  However,
	       in any case, if the option data will cross a buffer
	       boundary, split it across that boundary. */

	    ix = 0;
	    optstart = bufix;
	    while (length) {
		    unsigned char incr = length > 255 ? 255 : length;
		    int consumed = 0;
		    
		    /* If this hunk of the buffer will cross a
		       boundary, only go up to the boundary in this
		       pass. */
		    if (bufix < first_cutoff &&
			bufix + incr > first_cutoff)
			    incr = first_cutoff - bufix;
		    else if (bufix < second_cutoff &&
			     bufix + incr > second_cutoff)
			    incr = second_cutoff - bufix;
		    
		    /* If this option is going to overflow the buffer,
		       skip it. */
		    if (bufix + 2 + incr > buflen) {
			    bufix = optstart;
			    break;
		    }
		    
		    /* Everything looks good - copy it in! */
		    buffer [bufix] = code;
		    buffer [bufix + 1] = incr;
		    if (tto && incr == length) {
			    memcpy (buffer + bufix + 2,
				    od.data + ix, (unsigned)(incr - 1));
			    buffer [bufix + 2 + incr - 1] = 0;
		    } else {
			    memcpy (buffer + bufix + 2,
				    od.data + ix, (unsigned)incr);
		    }
		    length -= incr;
		    ix += incr;
		    bufix += 2 + incr;
	    }
	    data_string_forget (&od, MDL);
	}

	return bufix;
}

/* Format the specified option so that a human can easily read it. */

const char *pretty_print_option (option, data, len, emit_commas, emit_quotes)
	struct option *option;
	const unsigned char *data;
	unsigned len;
	int emit_commas;
	int emit_quotes;
{
	static char optbuf [32768]; /* XXX */
	int hunksize = 0;
	int opthunk = 0;
	int hunkinc = 0;
	int numhunk = -1;
	int numelem = 0;
	char fmtbuf [32];
	struct enumeration *enumbuf [32];
	int i, j, k, l;
	char *op = optbuf;
	const unsigned char *dp = data;
	struct in_addr foo;
	char comma;
	unsigned long tval;

	if (emit_commas)
		comma = ',';
	else
		comma = ' ';
	
	memset (enumbuf, 0, sizeof enumbuf);

	/* Figure out the size of the data. */
	for (l = i = 0; option -> format [i]; i++, l++) {
		if (!numhunk) {
			log_error ("%s: Extra codes in format string: %s",
				   option -> name,
				   &(option -> format [i]));
			break;
		}
		numelem++;
		fmtbuf [l] = option -> format [i];
		switch (option -> format [i]) {
		      case 'a':
			--numelem;
			fmtbuf [l] = 0;
			numhunk = 0;
			break;
		      case 'A':
			--numelem;
			fmtbuf [l] = 0;
			numhunk = 0;
			break;
		      case 'E':
			/* Skip the universe name. */
			while (option -> format [i] &&
			       option -> format [i] != '.')
				i++;
		      case 'X':
			for (k = 0; k < len; k++) {
				if (!isascii (data [k]) ||
				    !isprint (data [k]))
					break;
			}
			/* If we found no bogus characters, or the bogus
			   character we found is a trailing NUL, it's
			   okay to print this option as text. */
			if (k == len || (k + 1 == len && data [k] == 0)) {
				fmtbuf [l] = 't';
				numhunk = -2;
			} else {
				fmtbuf [l] = 'x';
				hunksize++;
				comma = ':';
				numhunk = 0;
			}
			fmtbuf [l + 1] = 0;
			break;
		      case 'd':
		      case 't':
			fmtbuf [l] = 't';
			fmtbuf [l + 1] = 0;
			numhunk = -2;
			break;
		      case 'N':
			k = i;
			while (option -> format [i] &&
			       option -> format [i] != '.')
				i++;
			enumbuf [l] =
				find_enumeration (&option -> format [k] + 1,
						  i - k - 1);
			hunksize += 1;
			hunkinc = 1;
			break;
		      case 'I':
		      case 'l':
		      case 'L':
		      case 'T':
			hunksize += 4;
			hunkinc = 4;
			break;
		      case 's':
		      case 'S':
			hunksize += 2;
			hunkinc = 2;
			break;
		      case 'b':
		      case 'B':
		      case 'f':
			hunksize++;
			hunkinc = 1;
			break;
		      case 'e':
			break;
		      case 'o':
			opthunk += hunkinc;
			break;
		      default:
			log_error ("%s: garbage in format string: %s",
			      option -> name,
			      &(option -> format [i]));
			break;
		} 
	}

	/* Check for too few bytes... */
	if (hunksize - opthunk > len) {
		log_error ("%s: expecting at least %d bytes; got %d",
		      option -> name,
		      hunksize, len);
		return "<error>";
	}
	/* Check for too many bytes... */
	if (numhunk == -1 && hunksize < len)
		log_error ("%s: %d extra bytes",
		      option -> name,
		      len - hunksize);

	/* If this is an array, compute its size. */
	if (!numhunk)
		numhunk = len / hunksize;
	/* See if we got an exact number of hunks. */
	if (numhunk > 0 && numhunk * hunksize < len)
		log_error ("%s: %d extra bytes at end of array\n",
		      option -> name,
		      len - numhunk * hunksize);

	/* A one-hunk array prints the same as a single hunk. */
	if (numhunk < 0)
		numhunk = 1;

	/* Cycle through the array (or hunk) printing the data. */
	for (i = 0; i < numhunk; i++) {
		for (j = 0; j < numelem; j++) {
			switch (fmtbuf [j]) {
			      case 't':
				if (emit_quotes)
					*op++ = '"';
				for (; dp < data + len; dp++) {
					if (!isascii (*dp) ||
					    !isprint (*dp)) {
						/* Skip trailing NUL. */
					    if (dp + 1 != data + len ||
						*dp != 0) {
						    sprintf (op, "\\%03o",
							     *dp);
						    op += 4;
					    }
					} else if (*dp == '"' ||
						   *dp == '\'' ||
						   *dp == '$' ||
						   *dp == '`' ||
						   *dp == '\\') {
						*op++ = '\\';
						*op++ = *dp;
					} else
						*op++ = *dp;
				}
				if (emit_quotes)
					*op++ = '"';
				*op = 0;
				break;
				/* pretty-printing an array of enums is
				   going to get ugly. */
			      case 'N':
				if (!enumbuf [j])
					goto enum_as_num;
				for (i = 0; ;i++) {
					if (!enumbuf [j] -> values [i].name)
						goto enum_as_num;
					if (enumbuf [j] -> values [i].value ==
					    *dp)
						break;
				}
				strcpy (op, enumbuf [j] -> values [i].name);
				op += strlen (op);
				break;
			      case 'I':
				foo.s_addr = htonl (getULong (dp));
				strcpy (op, inet_ntoa (foo));
				dp += 4;
				break;
			      case 'l':
				sprintf (op, "%ld", (long)getLong (dp));
				dp += 4;
				break;
			      case 'T':
				tval = getULong (dp);
				if (tval == -1)
					sprintf (op, "%s", "infinite");
				else
					sprintf (op, "%ld", tval);
				break;
			      case 'L':
				sprintf (op, "%ld",
					 (unsigned long)getULong (dp));
				dp += 4;
				break;
			      case 's':
				sprintf (op, "%d", (int)getShort (dp));
				dp += 2;
				break;
			      case 'S':
				sprintf (op, "%d", (unsigned)getUShort (dp));
				dp += 2;
				break;
			      case 'b':
				sprintf (op, "%d", *(const char *)dp++);
				break;
			      case 'B':
			      enum_as_num:
				sprintf (op, "%d", *dp++);
				break;
			      case 'x':
				sprintf (op, "%x", *dp++);
				break;
			      case 'f':
				strcpy (op, *dp++ ? "true" : "false");
				break;
			      default:
				log_error ("Unexpected format code %c",
					   fmtbuf [j]);
			}
			op += strlen (op);
			if (dp == data + len)
				break;
			if (j + 1 < numelem && comma != ':')
				*op++ = ' ';
		}
		if (i + 1 < numhunk) {
			*op++ = comma;
		}
		if (dp == data + len)
			break;
	}
	return optbuf;
}

int get_option (result, universe, packet, lease, client_state,
		in_options, cfg_options, options, scope, code, file, line)
	struct data_string *result;
	struct universe *universe;
	struct packet *packet;
	struct lease *lease;
	struct client_state *client_state;
	struct option_state *in_options;
	struct option_state *cfg_options;
	struct option_state *options;
	struct binding_scope **scope;
	unsigned code;
	const char *file;
	int line;
{
	struct option_cache *oc;

	if (!universe -> lookup_func)
		return 0;
	oc = ((*universe -> lookup_func) (universe, options, code));
	if (!oc)
		return 0;
	if (!evaluate_option_cache (result, packet, lease, client_state,
				    in_options, cfg_options, scope, oc,
				    file, line))
		return 0;
	return 1;
}

void set_option (universe, options, option, op)
	struct universe *universe;
	struct option_state *options;
	struct option_cache *option;
	enum statement_op op;
{
	struct option_cache *oc, *noc;

	switch (op) {
	      case if_statement:
	      case add_statement:
	      case eval_statement:
	      case break_statement:
	      default:
		log_error ("bogus statement type in do_option_set.");
		break;

	      case default_option_statement:
		oc = lookup_option (universe, options,
				    option -> option -> code);
		if (oc)
			break;
		save_option (universe, options, option);
		break;

	      case supersede_option_statement:
	      case send_option_statement:
		/* Install the option, replacing any existing version. */
		save_option (universe, options, option);
		break;

	      case append_option_statement:
	      case prepend_option_statement:
		oc = lookup_option (universe, options,
				    option -> option -> code);
		if (!oc) {
			save_option (universe, options, option);
			break;
		}
		/* If it's not an expression, make it into one. */
		if (!oc -> expression && oc -> data.len) {
			if (!expression_allocate (&oc -> expression, MDL)) {
				log_error ("Can't allocate const expression.");
				break;
			}
			oc -> expression -> op = expr_const_data;
			data_string_copy
				(&oc -> expression -> data.const_data,
				 &oc -> data, MDL);
			data_string_forget (&oc -> data, MDL);
		}
		noc = (struct option_cache *)0;
		if (!option_cache_allocate (&noc, MDL))
			break;
		if (op == append_option_statement) {
			if (!make_concat (&noc -> expression,
					  oc -> expression,
					  option -> expression)) {
				option_cache_dereference (&noc, MDL);
				break;
			}
		} else {
			if (!make_concat (&noc -> expression,
					  option -> expression,
					  oc -> expression)) {
				option_cache_dereference (&noc, MDL);
				break;
			}
		}
		noc -> option = oc -> option;
		save_option (universe, options, noc);
		option_cache_dereference (&noc, MDL);
		break;
	}
}

struct option_cache *lookup_option (universe, options, code)
	struct universe *universe;
	struct option_state *options;
	unsigned code;
{
	if (!options)
		return (struct option_cache *)0;
	if (universe -> lookup_func)
		return (*universe -> lookup_func) (universe, options, code);
	else
		log_error ("can't look up options in %s space.",
			   universe -> name);
	return (struct option_cache *)0;
}

struct option_cache *lookup_hashed_option (universe, options, code)
	struct universe *universe;
	struct option_state *options;
	unsigned code;
{
	int hashix;
	pair bptr;
	pair *hash;

	/* Make sure there's a hash table. */
	if (universe -> index >= options -> universe_count ||
	    !(options -> universes [universe -> index]))
		return (struct option_cache *)0;

	hash = options -> universes [universe -> index];

	hashix = compute_option_hash (code);
	for (bptr = hash [hashix]; bptr; bptr = bptr -> cdr) {
		if (((struct option_cache *)(bptr -> car)) -> option -> code ==
		    code)
			return (struct option_cache *)(bptr -> car);
	}
	return (struct option_cache *)0;
}

int save_option_buffer (struct universe *universe,
			struct option_state *options,
			struct buffer *bp,
			unsigned char *buffer, unsigned length,
			struct option *option, int tp)
{
	struct buffer *lbp = (struct buffer *)0;
	struct option_cache *op = (struct option_cache *)0;

	if (!option_cache_allocate (&op, MDL)) {
		log_error ("No memory for option %s.%s.",
			   universe -> name,
			   option -> name);
		return 0;
	}

	/* If we weren't passed a buffer in which the data are saved and
	   refcounted, allocate one now. */
	if (!bp) {
		if (!buffer_allocate (&lbp, length, MDL)) {
			log_error ("no memory for option buffer.");

			option_cache_dereference (&op, MDL);
			return 0;
		}
		memcpy (lbp -> data, buffer, length + tp);
		bp = lbp;
		buffer = &bp -> data [0]; /* Refer to saved buffer. */
	}

	/* Reference buffer copy to option cache. */
	op -> data.buffer = (struct buffer *)0;
	buffer_reference (&op -> data.buffer, bp, MDL);
		
	/* Point option cache into buffer. */
	op -> data.data = buffer;
	op -> data.len = length;
			
	if (tp) {
		/* NUL terminate (we can get away with this because we (or
		   the caller!) allocated one more than the buffer size, and
		   because the byte following the end of an option is always
		   the code of the next option, which the caller is getting
		   out of the *original* buffer. */
		buffer [length] = 0;
		op -> data.terminated = 1;
	} else
		op -> data.terminated = 0;
	
	op -> option = option;

	/* Now store the option. */
	save_option (universe, options, op);

	/* And let go of our reference. */
	option_cache_dereference (&op, MDL);

	return 1;
}

void save_option (struct universe *universe,
		  struct option_state *options, struct option_cache *oc)
{
	if (universe -> save_func)
		(*universe -> save_func) (universe, options, oc);
	else
		log_error ("can't store options in %s space.",
			   universe -> name);
}

void save_hashed_option (universe, options, oc)
	struct universe *universe;
	struct option_state *options;
	struct option_cache *oc;
{
	int hashix;
	pair bptr;
	pair *hash = options -> universes [universe -> index];

	if (oc -> refcnt == 0)
		abort ();

	/* Compute the hash. */
	hashix = compute_option_hash (oc -> option -> code);

	/* If there's no hash table, make one. */
	if (!hash) {
		hash = (pair *)dmalloc (OPTION_HASH_SIZE * sizeof *hash, MDL);
		if (!hash) {
			log_error ("no memory to store %s.%s",
				   universe -> name, oc -> option -> name);
			return;
		}
		memset (hash, 0, OPTION_HASH_SIZE * sizeof *hash);
		options -> universes [universe -> index] = (VOIDPTR)hash;
	} else {
		/* Try to find an existing option matching the new one. */
		for (bptr = hash [hashix]; bptr; bptr = bptr -> cdr) {
			if (((struct option_cache *)
			     (bptr -> car)) -> option -> code ==
			    oc -> option -> code)
				break;
		}

		/* If we find one, dereference it and put the new one
		   in its place. */
		if (bptr) {
			option_cache_dereference
				((struct option_cache **)&bptr -> car, MDL);
			option_cache_reference
				((struct option_cache **)&bptr -> car,
				 oc, MDL);
			return;
		}
	}

	/* Otherwise, just put the new one at the head of the list. */
	bptr = new_pair (MDL);
	if (!bptr) {
		log_error ("No memory for option_cache reference.");
		return;
	}
	bptr -> cdr = hash [hashix];
	bptr -> car = 0;
	option_cache_reference ((struct option_cache **)&bptr -> car, oc, MDL);
	hash [hashix] = bptr;
}

void delete_option (universe, options, code)
	struct universe *universe;
	struct option_state *options;
	int code;
{
	if (universe -> delete_func)
		(*universe -> delete_func) (universe, options, code);
	else
		log_error ("can't delete options from %s space.",
			   universe -> name);
}

void delete_hashed_option (universe, options, code)
	struct universe *universe;
	struct option_state *options;
	int code;
{
	int hashix;
	pair bptr, prev = (pair)0;
	pair *hash = options -> universes [universe -> index];

	/* There may not be any options in this space. */
	if (!hash)
		return;

	/* Try to find an existing option matching the new one. */
	hashix = compute_option_hash (code);
	for (bptr = hash [hashix]; bptr; bptr = bptr -> cdr) {
		if (((struct option_cache *)(bptr -> car)) -> option -> code
		    == code)
			break;
		prev = bptr;
	}
	/* If we found one, wipe it out... */
	if (bptr) {
		if (prev)
			prev -> cdr = bptr -> cdr;
		else
			hash [hashix] = bptr -> cdr;
		option_cache_dereference
			((struct option_cache **)(&bptr -> car), MDL);
		free_pair (bptr, MDL);
	}
}

extern struct option_cache *free_option_caches; /* XXX */

int option_cache_dereference (ptr, file, line)
	struct option_cache **ptr;
	const char *file;
	int line;
{
	if (!ptr || !*ptr) {
		log_error ("Null pointer in option_cache_dereference: %s(%d)",
			   file, line);
#if defined (POINTER_DEBUG)
		abort ();
#else
		return 0;
#endif
	}

	(*ptr) -> refcnt--;
	rc_register (file, line, ptr, *ptr, (*ptr) -> refcnt, 1, RC_MISC);
	if (!(*ptr) -> refcnt) {
		if ((*ptr) -> data.buffer)
			data_string_forget (&(*ptr) -> data, file, line);
		if ((*ptr) -> expression)
			expression_dereference (&(*ptr) -> expression,
						file, line);
		if ((*ptr) -> next)
			option_cache_dereference (&((*ptr) -> next),
						  file, line);
		/* Put it back on the free list... */
		(*ptr) -> expression = (struct expression *)free_option_caches;
		free_option_caches = *ptr;
		dmalloc_reuse (free_option_caches, (char *)0, 0, 0);
	}
	if ((*ptr) -> refcnt < 0) {
		log_error ("%s(%d): negative refcnt!", file, line);
#if defined (DEBUG_RC_HISTORY)
		dump_rc_history (*ptr);
#endif
#if defined (POINTER_DEBUG)
		abort ();
#else
		*ptr = (struct option_cache *)0;
		return 0;
#endif
	}
	*ptr = (struct option_cache *)0;
	return 1;

}

int hashed_option_state_dereference (universe, state, file, line)
	struct universe *universe;
	struct option_state *state;
	const char *file;
	int line;
{
	pair *heads;
	pair cp, next;
	int i;

	/* Get the pointer to the array of hash table bucket heads. */
	heads = (pair *)(state -> universes [universe -> index]);
	if (!heads)
		return 0;

	/* For each non-null head, loop through all the buckets dereferencing
	   the attached option cache structures and freeing the buckets. */
	for (i = 0; i < OPTION_HASH_SIZE; i++) {
		for (cp = heads [i]; cp; cp = next) {
			next = cp -> cdr;
			option_cache_dereference
				((struct option_cache **)&cp -> car,
				 file, line);
			free_pair (cp, file, line);
		}
	}

	dfree (heads, file, line);
	state -> universes [universe -> index] = (void *)0;
	return 1;
}

int store_option (result, universe, packet, lease, client_state,
		  in_options, cfg_options, scope, oc)
	struct data_string *result;
	struct universe *universe;
	struct packet *packet;
	struct lease *lease;
	struct client_state *client_state;
	struct option_state *in_options;
	struct option_state *cfg_options;
	struct binding_scope **scope;
	struct option_cache *oc;
{
	struct data_string d1, d2;

	memset (&d1, 0, sizeof d1);
	memset (&d2, 0, sizeof d2);

	if (evaluate_option_cache (&d2, packet, lease, client_state,
				   in_options, cfg_options, scope, oc, MDL)) {
		if (!buffer_allocate (&d1.buffer,
				      (result -> len +
				       universe -> length_size +
				       universe -> tag_size + d2.len), MDL)) {
			data_string_forget (result, MDL);
			data_string_forget (&d2, MDL);
			return 0;
		}
		d1.data = &d1.buffer -> data [0];
		if (result -> len)
			memcpy (d1.buffer -> data,
				result -> data, result -> len);
		d1.len = result -> len;
		(*universe -> store_tag) (&d1.buffer -> data [d1.len],
					  oc -> option -> code);
		d1.len += universe -> tag_size;
		(*universe -> store_length) (&d1.buffer -> data [d1.len],
					     d2.len);
		d1.len += universe -> length_size;
		memcpy (&d1.buffer -> data [d1.len], d2.data, d2.len);
		d1.len += d2.len;
		data_string_forget (&d2, MDL);
		data_string_forget (result, MDL);
		data_string_copy (result, &d1, MDL);
		data_string_forget (&d1, MDL);
		return 1;
	}
	return 0;
}
	
int option_space_encapsulate (result, packet, lease, client_state,
			      in_options, cfg_options, scope, name)
	struct data_string *result;
	struct packet *packet;
	struct lease *lease;
	struct client_state *client_state;
	struct option_state *in_options;
	struct option_state *cfg_options;
	struct binding_scope **scope;
	struct data_string *name;
{
	struct universe *u;

	u = (struct universe *)0;
	universe_hash_lookup (&u, universe_hash,
			      (const char *)name -> data, name -> len, MDL);
	if (!u)
		return 0;

	if (u -> encapsulate)
		return (*u -> encapsulate) (result, packet, lease,
					    client_state,
					    in_options, cfg_options, scope, u);
	log_error ("encapsulation requested for %s with no support.",
		   name -> data);
	return 0;
}

int hashed_option_space_encapsulate (result, packet, lease, client_state,
				     in_options, cfg_options, scope, universe)
	struct data_string *result;
	struct packet *packet;
	struct lease *lease;
	struct client_state *client_state;
	struct option_state *in_options;
	struct option_state *cfg_options;
	struct binding_scope **scope;
	struct universe *universe;
{
	pair p, *hash;
	int status;
	int i;

	if (universe -> index >= cfg_options -> universe_count)
		return 0;

	hash = cfg_options -> universes [universe -> index];
	if (!hash)
		return 0;

	status = 0;
	for (i = 0; i < OPTION_HASH_SIZE; i++) {
		for (p = hash [i]; p; p = p -> cdr) {
			if (store_option (result, universe, packet,
					  lease, client_state, in_options,
					  cfg_options, scope,
					  (struct option_cache *)p -> car))
				status = 1;
		}
	}

	return status;
}

int nwip_option_space_encapsulate (result, packet, lease, client_state,
				   in_options, cfg_options, scope, universe)
	struct data_string *result;
	struct packet *packet;
	struct lease *lease;
	struct client_state *client_state;
	struct option_state *in_options;
	struct option_state *cfg_options;
	struct binding_scope **scope;
	struct universe *universe;
{
	pair ocp;
	int status;
	int i;
	static struct option_cache *no_nwip;
	struct data_string ds;
	struct option_chain_head *head;

	if (universe -> index >= cfg_options -> universe_count)
		return 0;
	head = ((struct option_chain_head *)
		cfg_options -> universes [fqdn_universe.index]);
	if (!head)
		return 0;

	status = 0;
	for (ocp = head -> first; ocp; ocp = ocp -> cdr) {
		struct option_cache *oc = (struct option_cache *)(ocp -> car);
		if (store_option (result, universe, packet,
				  lease, client_state, in_options,
				  cfg_options, scope,
				  (struct option_cache *)ocp -> car))
			status = 1;
	}

	/* If there's no data, the nwip suboption is supposed to contain
	   a suboption saying there's no data. */
	if (!status) {
		if (!no_nwip) {
			static unsigned char nni [] = { 1, 0 };
			memset (&ds, 0, sizeof ds);
			ds.data = nni;
			ds.len = 2;
			if (option_cache_allocate (&no_nwip, MDL))
				data_string_copy (&no_nwip -> data, &ds, MDL);
			no_nwip -> option = nwip_universe.options [1];
		}
		if (no_nwip) {
			if (store_option (result, universe, packet, lease,
					  client_state, in_options,
					  cfg_options, scope, no_nwip))
				status = 1;
		}
	} else {
		memset (&ds, 0, sizeof ds);

		/* If we have nwip options, the first one has to be the
		   nwip-exists-in-option-area option. */
		if (!buffer_allocate (&ds.buffer, result -> len + 2, MDL)) {
			data_string_forget (result, MDL);
			return 0;
		}
		ds.data = &ds.buffer -> data [0];
		ds.buffer -> data [0] = 2;
		ds.buffer -> data [1] = 0;
		memcpy (&ds.buffer -> data [2], result -> data, result -> len);
		data_string_forget (result, MDL);
		data_string_copy (result, &ds, MDL);
		data_string_forget (&ds, MDL);
	}

	return status;
}

int fqdn_option_space_encapsulate (result, packet, lease, client_state,
				   in_options, cfg_options, scope, universe)
	struct data_string *result;
	struct packet *packet;
	struct lease *lease;
	struct client_state *client_state;
	struct option_state *in_options;
	struct option_state *cfg_options;
	struct binding_scope **scope;
	struct universe *universe;
{
	pair ocp;
	struct data_string results [FQDN_SUBOPTION_COUNT + 1];
	unsigned i;
	unsigned len;
	struct buffer *bp = (struct buffer *)0;
	struct option_chain_head *head;

	/* If there's no FQDN universe, don't encapsulate. */
	if (fqdn_universe.index >= cfg_options -> universe_count)
		return 0;
	head = ((struct option_chain_head *)
		cfg_options -> universes [fqdn_universe.index]);
	if (!head)
		return 0;

	/* Figure out the values of all the suboptions. */
	memset (results, 0, sizeof results);
	for (ocp = head -> first; ocp; ocp = ocp -> cdr) {
		struct option_cache *oc = (struct option_cache *)(ocp -> car);
		if (oc -> option -> code > FQDN_SUBOPTION_COUNT)
			continue;
		evaluate_option_cache (&results [oc -> option -> code],
				       packet, lease, client_state, in_options,
				       cfg_options, scope,  oc, MDL);
	}
	len = 4 + results [FQDN_FQDN].len;
	/* Save the contents of the option in a buffer. */
	if (!buffer_allocate (&bp, len, MDL)) {
		log_error ("no memory for option buffer.");
		return 0;
	}
	buffer_reference (&result -> buffer, bp, MDL);
	result -> len = 3;
	result -> data = &bp -> data [0];

	memset (&bp -> data [0], 0, len);
	if (results [FQDN_NO_CLIENT_UPDATE].len &&
	    results [FQDN_NO_CLIENT_UPDATE].data [0])
		bp -> data [0] |= 2;
	if (results [FQDN_SERVER_UPDATE].len &&
	    results [FQDN_SERVER_UPDATE].data [0])
		bp -> data [0] |= 1;
	if (results [FQDN_RCODE1].len)
		bp -> data [1] = results [FQDN_RCODE1].data [0];
	if (results [FQDN_RCODE2].len)
		bp -> data [2] = results [FQDN_RCODE2].data [0];

	if (results [FQDN_ENCODED].len &&
	    results [FQDN_ENCODED].data [0]) {
		unsigned char *out;
		int i;
		bp -> data [0] |= 4;
		out = &bp -> data [3];
		if (results [FQDN_FQDN].len) {
			i = 0;
			while (i < results [FQDN_FQDN].len) {
				int j;
				for (j = i; ('.' !=
					     results [FQDN_FQDN].data [j]) &&
					     j < results [FQDN_FQDN].len; j++)
					;
				*out++ = j - i;
				memcpy (out, &results [FQDN_FQDN].data [i],
					(unsigned)(j - i));
				out += j - i;
				i = j;
				if (results [FQDN_FQDN].data [j] == '.')
					i++;
			}
			if ((results [FQDN_FQDN].data
			     [results [FQDN_FQDN].len - 1] == '.'))
				*out++ = 0;
			result -> len = out - result -> data;
			result -> terminated = 0;
		}
	} else {
		if (results [FQDN_FQDN].len) {
			memcpy (&bp -> data [3], results [FQDN_FQDN].data,
				results [FQDN_FQDN].len);
			result -> len += results [FQDN_FQDN].len;
			result -> terminated = 0;
		}
	}
	for (i = 1; i <= FQDN_SUBOPTION_COUNT; i++) {
		if (results [i].len)
			data_string_forget (&results [i], MDL);
	}
	buffer_dereference (&bp, MDL);
	return 1;
}

void option_space_foreach (struct packet *packet, struct lease *lease,
			   struct client_state *client_state,
			   struct option_state *in_options,
			   struct option_state *cfg_options,
			   struct binding_scope **scope,
			   struct universe *u, void *stuff,
			   void (*func) (struct option_cache *,
					 struct packet *,
					 struct lease *, struct client_state *,
					 struct option_state *,
					 struct option_state *,
					 struct binding_scope **,
					 struct universe *, void *))
{
	if (u -> foreach)
		(*u -> foreach) (packet, lease, client_state, in_options,
				 cfg_options, scope, u, stuff, func);
}

void suboption_foreach (struct packet *packet, struct lease *lease,
			struct client_state *client_state,
			struct option_state *in_options,
			struct option_state *cfg_options,
			struct binding_scope **scope,
			struct universe *u, void *stuff,
			void (*func) (struct option_cache *,
				      struct packet *,
				      struct lease *, struct client_state *,
				      struct option_state *,
				      struct option_state *,
				      struct binding_scope **,
				      struct universe *, void *),
			struct option_cache *oc,
			const char *vsname)
{
	struct universe *universe = find_option_universe (oc -> option,
							  vsname);
	int i;

	if (universe -> foreach)
		(*universe -> foreach) (packet, lease, client_state,
					in_options, cfg_options,
					scope, universe, stuff, func);
}

void hashed_option_space_foreach (struct packet *packet, struct lease *lease,
				  struct client_state *client_state,
				  struct option_state *in_options,
				  struct option_state *cfg_options,
				  struct binding_scope **scope,
				  struct universe *u, void *stuff,
				  void (*func) (struct option_cache *,
						struct packet *,
						struct lease *,
						struct client_state *,
						struct option_state *,
						struct option_state *,
						struct binding_scope **,
						struct universe *, void *))
{
	pair *hash;
	int i;
	struct option_cache *oc;

	if (cfg_options -> universe_count <= u -> index)
		return;

	hash = cfg_options -> universes [u -> index];
	if (!hash)
		return;
	for (i = 0; i < OPTION_HASH_SIZE; i++) {
		pair p;
		/* XXX save _all_ options! XXX */
		for (p = hash [i]; p; p = p -> cdr) {
			oc = (struct option_cache *)p -> car;
			(*func) (oc, packet, lease, client_state,
				 in_options, cfg_options, scope, u, stuff);
		}
	}
}

void save_linked_option (universe, options, oc)
	struct universe *universe;
	struct option_state *options;
	struct option_cache *oc;
{
	pair *tail;
	pair np = (pair )0;
	struct option_chain_head *head;

	if (universe -> index >= options -> universe_count)
		return;
	head = ((struct option_chain_head *)
		options -> universes [universe -> index]);
	if (!head) {
		if (!option_chain_head_allocate (((struct option_chain_head **)
						  &options -> universes
						  [universe -> index]), MDL))
			return;
		head = ((struct option_chain_head *)
			options -> universes [universe -> index]);
	}

	/* Find the tail of the list. */
	for (tail = &head -> first; *tail; tail = &((*tail) -> cdr)) {
		if (oc -> option ==
		    ((struct option_cache *)((*tail) -> car)) -> option) {
			option_cache_dereference ((struct option_cache **)
						  (&(*tail) -> car), MDL);
			option_cache_reference ((struct option_cache **)
						(&(*tail) -> car), oc, MDL);
			return;
		}
	}

	*tail = cons (0, 0);
	if (*tail) {
		option_cache_reference ((struct option_cache **)
					(&(*tail) -> car), oc, MDL);
	}
}

int linked_option_space_encapsulate (result, packet, lease, client_state,
				    in_options, cfg_options, scope, universe)
	struct data_string *result;
	struct packet *packet;
	struct lease *lease;
	struct client_state *client_state;
	struct option_state *in_options;
	struct option_state *cfg_options;
	struct binding_scope **scope;
	struct universe *universe;
{
	int status;
	pair oc;
	struct option_chain_head *head;

	if (universe -> index >= cfg_options -> universe_count)
		return 0;
	head = ((struct option_chain_head *)
		cfg_options -> universes [universe -> index]);
	if (!head)
		return 0;

	status = 0;
	for (oc = head -> first; oc; oc = oc -> cdr) {
		if (store_option (result, universe, packet,
				  lease, client_state, in_options, cfg_options,
				  scope, (struct option_cache *)(oc -> car)))
			status = 1;
	}

	return status;
}

void delete_linked_option (universe, options, code)
	struct universe *universe;
	struct option_state *options;
	int code;
{
	pair *tail, tmp = (pair)0;
	struct option_chain_head *head;

	if (universe -> index >= options -> universe_count)
		return;
	head = ((struct option_chain_head *)
		options -> universes [universe -> index]);
	if (!head)
		return;

	for (tail = &head -> first; *tail; tail = &((*tail) -> cdr)) {
		if (code ==
		    ((struct option_cache *)(*tail) -> car) -> option -> code)
		{
			tmp = (*tail) -> cdr;
			option_cache_dereference ((struct option_cache **)
						  (&(*tail) -> car), MDL);
			dfree (*tail, MDL);
			(*tail) = tmp;
			break;
		}
	}
}

struct option_cache *lookup_linked_option (universe, options, code)
	struct universe *universe;
	struct option_state *options;
	unsigned code;
{
	pair oc;
	struct option_chain_head *head;

	if (universe -> index >= options -> universe_count)
		return 0;
	head = ((struct option_chain_head *)
		options -> universes [universe -> index]);
	if (!head)
		return 0;

	for (oc = head -> first; oc; oc = oc -> cdr) {
		if (code ==
		    ((struct option_cache *)(oc -> car)) -> option -> code) {
			return (struct option_cache *)(oc -> car);
		}
	}

	return (struct option_cache *)0;
}

int linked_option_state_dereference (universe, state, file, line)
	struct universe *universe;
	struct option_state *state;
	const char *file;
	int line;
{
	return (option_chain_head_dereference
		((struct option_chain_head **)
		 (&state -> universes [universe -> index]), MDL));
}

void linked_option_space_foreach (struct packet *packet, struct lease *lease,
				  struct client_state *client_state,
				  struct option_state *in_options,
				  struct option_state *cfg_options,
				  struct binding_scope **scope,
				  struct universe *u, void *stuff,
				  void (*func) (struct option_cache *,
						struct packet *,
						struct lease *,
						struct client_state *,
						struct option_state *,
						struct option_state *,
						struct binding_scope **,
						struct universe *, void *))
{
	pair car;
	struct option_chain_head *head;

	if (u -> index >= cfg_options -> universe_count)
		return;
	head = ((struct option_chain_head *)
		cfg_options -> universes [u -> index]);
	if (!head)
		return;
	for (car = head -> first; car; car = car -> cdr) {
		(*func) ((struct option_cache *)(car -> car),
			 packet, lease, client_state,
			 in_options, cfg_options, scope, u, stuff);
	}
}

void do_packet (interface, packet, len, from_port, from, hfrom)
	struct interface_info *interface;
	struct dhcp_packet *packet;
	unsigned len;
	unsigned int from_port;
	struct iaddr from;
	struct hardware *hfrom;
{
	int i;
	struct option_cache *op;
	struct packet *decoded_packet;
#if defined (DEBUG_MEMORY_LEAKAGE)
	unsigned long previous_outstanding = dmalloc_outstanding;
#endif

#if defined (TRACING)
	trace_inpacket_stash (interface, packet, len, from_port, from, hfrom);
#endif

	decoded_packet = (struct packet *)0;
	if (!packet_allocate (&decoded_packet, MDL)) {
		log_error ("do_packet: no memory for incoming packet!");
		return;
	}
	decoded_packet -> raw = packet;
	decoded_packet -> packet_length = len;
	decoded_packet -> client_port = from_port;
	decoded_packet -> client_addr = from;
	interface_reference (&decoded_packet -> interface, interface, MDL);
	decoded_packet -> haddr = hfrom;
	
	if (packet -> hlen > sizeof packet -> chaddr) {
		packet_dereference (&decoded_packet, MDL);
		log_info ("Discarding packet with bogus hlen.");
		return;
	}

	/* If there's an option buffer, try to parse it. */
	if (decoded_packet -> packet_length >= DHCP_FIXED_NON_UDP + 4) {
		if (!parse_options (decoded_packet)) {
			if (decoded_packet -> options)
				option_state_dereference
					(&decoded_packet -> options, MDL);
			packet_dereference (&decoded_packet, MDL);
			return;
		}

		if (decoded_packet -> options_valid &&
		    (op = lookup_option (&dhcp_universe,
					 decoded_packet -> options, 
					 DHO_DHCP_MESSAGE_TYPE))) {
			struct data_string dp;
			memset (&dp, 0, sizeof dp);
			evaluate_option_cache (&dp, decoded_packet,
					       (struct lease *)0,
					       (struct client_state *)0,
					       decoded_packet -> options,
					       (struct option_state *)0,
					       (struct binding_scope **)0,
					       op, MDL);
			if (dp.len > 0)
				decoded_packet -> packet_type = dp.data [0];
			else
				decoded_packet -> packet_type = 0;
			data_string_forget (&dp, MDL);
		}
	}
		
	if (decoded_packet -> packet_type)
		dhcp (decoded_packet);
	else
		bootp (decoded_packet);

	/* If the caller kept the packet, they'll have upped the refcnt. */
	packet_dereference (&decoded_packet, MDL);

#if defined (DEBUG_MEMORY_LEAKAGE)
	log_info ("generation %ld: %ld new, %ld outstanding, %ld long-term",
		  dmalloc_generation,
		  dmalloc_outstanding - previous_outstanding,
		  dmalloc_outstanding, dmalloc_longterm);
#endif
#if defined (DEBUG_MEMORY_LEAKAGE)
	dmalloc_dump_outstanding ();
#endif
#if defined (DEBUG_RC_HISTORY_EXHAUSTIVELY)
	dump_rc_history (0);
#endif
}

