/* resolv.c

   Parser for /etc/resolv.conf file. */

/*
 * Copyright (c) 1996-2002 Internet Software Consortium.
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
"$Id: resolv.c,v 1.16.2.1 2002/11/17 02:26:59 dhankins Exp $ Copyright (c) 1996-2002 The Internet Software Consortium.  All rights reserved.\n";
#endif /* not lint */

#include "dhcpd.h"

struct name_server *name_servers;
struct domain_search_list *domains;
char path_resolv_conf [] = _PATH_RESOLV_CONF;

void read_resolv_conf (parse_time)
	TIME parse_time;
{
	int file;
	struct parse *cfile;
	const char *val;
	int token;
	int declaration = 0;
	struct name_server *sp, *sl, *ns;
	struct domain_search_list *dp, *dl, *nd;
	struct iaddr *iaddr;

	if ((file = open (path_resolv_conf, O_RDONLY)) < 0) {
		log_error ("Can't open %s: %m", path_resolv_conf);
		return;
	}

	cfile = (struct parse *)0;
	new_parse (&cfile, file, (char *)0, 0, path_resolv_conf, 1);

	do {
		token = next_token (&val, (unsigned *)0, cfile);
		if (token == END_OF_FILE)
			break;
		else if (token == EOL)
			continue;
		else if (token == DOMAIN || token == SEARCH) {
			do {
				struct domain_search_list *nd, **dp;
				char *dn;

				dn = parse_host_name (cfile);
				if (!dn)
					break;

				dp = &domains;
				for (nd = domains; nd; nd = nd -> next) {
					dp = &nd -> next;
					if (!strcmp (nd -> domain, dn))
						break;
				}
				if (!nd) {
					nd = new_domain_search_list (MDL);
					if (!nd)
						log_fatal ("No memory for %s",
							   dn);
					nd -> next =
						(struct domain_search_list *)0;
					*dp = nd;
					nd -> domain = dn;
					dn = (char *)0;
				}
				nd -> rcdate = parse_time;
				token = peek_token (&val,
						    (unsigned *)0, cfile);
			} while (token != EOL);
			if (token != EOL) {
				parse_warn (cfile,
					    "junk after domain declaration");
				skip_to_semi (cfile);
			}
			token = next_token (&val, (unsigned *)0, cfile);
		} else if (token == NAMESERVER) {
			struct name_server *ns, **sp;
			struct iaddr iaddr;

			parse_ip_addr (cfile, &iaddr);

			sp = &name_servers;
			for (ns = name_servers; ns; ns = ns -> next) {
				sp = &ns -> next;
				if (!memcmp (&ns -> addr.sin_addr,
					     iaddr.iabuf, iaddr.len))
					break;
			}
			if (!ns) {
				ns = new_name_server (MDL);
				if (!ns)
				    log_fatal ("No memory for nameserver %s",
					       piaddr (iaddr));
				ns -> next = (struct name_server *)0;
				*sp = ns;
				memcpy (&ns -> addr.sin_addr,
					iaddr.iabuf, iaddr.len);
#ifdef HAVE_SA_LEN
				ns -> addr.sin_len = sizeof ns -> addr;
#endif
				ns -> addr.sin_family = AF_INET;
				ns -> addr.sin_port = htons (53);
				memset (ns -> addr.sin_zero, 0,
					sizeof ns -> addr.sin_zero);
			}
			ns -> rcdate = parse_time;
			skip_to_semi (cfile);
		} else
			skip_to_semi (cfile); /* Ignore what we don't grok. */
	} while (1);
	token = next_token (&val, (unsigned *)0, cfile);

	/* Lose servers that are no longer in /etc/resolv.conf. */
	sl = (struct name_server *)0;
	for (sp = name_servers; sp; sp = ns) {
		ns = sp -> next;
		if (sp -> rcdate != parse_time) {
			if (sl)
				sl -> next = sp -> next;
			else
				name_servers = sp -> next;
			/* We can't actually free the name server structure,
			   because somebody might be hanging on to it.    If
			   your /etc/resolv.conf file changes a lot, this
			   could be a noticable memory leak. */
		} else
			sl = sp;
	}

	/* Lose domains that are no longer in /etc/resolv.conf. */
	dl = (struct domain_search_list *)0;
	for (dp = domains; dp; dp = nd) {
		nd = dp -> next;
		if (dp -> rcdate != parse_time) {
			if (dl)
				dl -> next = dp -> next;
			else
				domains = dp -> next;
			free_domain_search_list (dp, MDL);
		} else
			dl = dp;
	}
	close (file);
	end_parse (&cfile);
}

/* Pick a name server from the /etc/resolv.conf file. */

struct name_server *first_name_server ()
{
	FILE *rc;
	static TIME rcdate;
	struct stat st;

	/* Check /etc/resolv.conf and reload it if it's changed. */
	if (cur_time > rcdate) {
		if (stat (path_resolv_conf, &st) < 0) {
			log_error ("Can't stat %s", path_resolv_conf);
			return (struct name_server *)0;
		}
		if (st.st_mtime > rcdate) {
			char rcbuf [512];
			char *s, *t, *u;
			rcdate = cur_time + 1;
			
			read_resolv_conf (rcdate);
		}
	}

	return name_servers;
}
