/* omshell.c

   Examine and modify omapi objects. */

/*
 * Copyright (c) 2004 by Internet Systems Consortium, Inc. ("ISC")
 * Copyright (c) 2001-2003 by Internet Software Consortium
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
"$Id: omshell.c,v 1.7.2.16 2004/06/10 17:59:24 dhankins Exp $ Copyright (c) 2004 Internet Systems Consortium.  All rights reserved.\n";
#endif /* not lint */

#include <time.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <isc-dhcp/result.h>
#include "dhcpctl.h"
#include "dhcpd.h"

/* Fixups */
isc_result_t find_class (struct class **c, const char *n, const char *f, int l)
{
	return 0;
}
int parse_allow_deny (struct option_cache **oc, struct parse *cfile, int flag)
{
	return 0;
}
void dhcp (struct packet *packet) { }
void bootp (struct packet *packet) { }
int check_collection (struct packet *p, struct lease *l, struct collection *c)
{
	return 0;
}
void classify (struct packet *packet, struct class *class) { }

static void usage (char *s) {
	fprintf (stderr, "Usage: %s\n", s);
	exit (1);
}

static void check (isc_result_t status, const char *func) {
	if (status != ISC_R_SUCCESS) {
		fprintf (stderr, "%s: %s\n", func, isc_result_totext (status));
		exit (1);
	}
}

int main (int argc, char **argv, char **envp)
{
	isc_result_t status, waitstatus;
	dhcpctl_handle connection;
	dhcpctl_handle authenticator;
	dhcpctl_handle oh;
	dhcpctl_data_string cid, ip_addr;
	dhcpctl_data_string result, groupname, identifier;
	struct data_string secret;
	const char *name = 0, *algorithm = "hmac-md5";
	int i, j;
	int port = 7911;
	const char *server = "127.0.0.1";
	struct parse *cfile;
	enum dhcp_token token;
	const char *val;
	char *s;
	char buf[1024];
	char s1[1024];
	int connected = 0;

	for (i = 1; i < argc; i++) {
		usage(argv[0]);
	}

	/* Initially, log errors to stderr as well as to syslogd. */
#ifdef SYSLOG_4_2
	openlog ("omshell", LOG_NDELAY);
	log_priority = DHCPD_LOG_FACILITY;
#else
	openlog ("omshell", LOG_NDELAY, DHCPD_LOG_FACILITY);
#endif
	status = dhcpctl_initialize ();
	if (status != ISC_R_SUCCESS) {
		fprintf (stderr, "dhcpctl_initialize: %s\n",
			 isc_result_totext (status));
		exit (1);
	}

	memset (&oh, 0, sizeof oh);

	do {
	    if (!connected) {
	    } else if (oh == NULL) {
		printf ("obj: <null>\n");
	    } else {
		dhcpctl_remote_object_t *r = (dhcpctl_remote_object_t *)oh;
		omapi_generic_object_t *g =
			(omapi_generic_object_t *)(r -> inner);
		
		printf ("obj: ");

		if (r -> rtype -> type != omapi_datatype_string) {
			printf ("?\n");
		} else {
			printf ("%.*s\n",
				(int)(r -> rtype -> u . buffer . len),
				r -> rtype -> u . buffer . value);
		}
		
		for (i = 0; i < g -> nvalues; i++) {
		    omapi_value_t *v = g -> values [i];
			
		    if (!g -> values [i])
			    continue;

		    printf ("%.*s = ", (int)v -> name -> len,
			    v -> name -> value);
			
		    if (!v -> value) {
			printf ("<null>\n");
			continue;
		    }
		    switch (v -> value -> type) {
			  case omapi_datatype_int:
			    printf ("%d\n",
				    v -> value -> u . integer);
			    break;
			 
			  case omapi_datatype_string:
			    printf ("\"%.*s\"\n",
				    (int) v -> value -> u.buffer.len,
				    v -> value -> u.buffer.value);
			    break;
				
			  case omapi_datatype_data:
			    printf ("%s\n",
				    print_hex_1 (v -> value -> u.buffer.len,
						 v -> value -> u.buffer.value,
						 60));
			    break;
			    
			  case omapi_datatype_object:
			    printf ("<obj>\n");
			    break;
		    }
		}
	    }

	    fputs ("> ", stdout);
	    fflush (stdout);
	    if (fgets (buf, sizeof(buf), stdin) == NULL)
		break;

	    status = new_parse (&cfile, 0, buf, strlen(buf), "<STDIN>", 1);
	    check(status, "new_parse()");
	    
	    token = next_token (&val, (unsigned *)0, cfile);
	    switch (token) {
		  default:
		    parse_warn (cfile, "unknown token: %s", val);
		    skip_to_semi (cfile);
		    break;
		    
		  case END_OF_FILE:
		  case EOL:
		    break;
		    
		  case TOKEN_HELP:
		  case '?':
		    printf ("Commands:\n");
		    printf ("  port <server omapi port>\n");
		    printf ("  server <server address>\n");
		    printf ("  key <key name> <key value>\n");
		    printf ("  connect\n");
		    printf ("  new <object-type>\n");
		    printf ("  set <name> = <value>\n");
		    printf ("  create\n");
		    printf ("  open\n");
		    printf ("  update\n");
		    printf ("  unset <name>\n");
		    printf ("  refresh\n");
		    printf ("  remove\n");
		    skip_to_semi (cfile);
		    break;
		    
		  case PORT:
		    token = next_token (&val, (unsigned *)0, cfile);
		    if (is_identifier (token)) {
			    struct servent *se;
			    se = getservbyname (val, "tcp");
			    if (se)
				    port = ntohs (se -> s_port);
			    else {
				    printf ("unknown service name: %s\n", val);
				    break;
			    }
		    } else if (token == NUMBER) {
			    port = atoi (val);
		    } else {
			    skip_to_semi (cfile);
			    printf ("usage: port <port>\n");
			    break;
		    }
		    token = next_token (&val, (unsigned *)0, cfile);
		    if (token != END_OF_FILE && token != EOL) {
			    printf ("usage: port <server>\n");
			    skip_to_semi (cfile);
			    break;
		    }
		    break;

		  case SERVER:
		    token = next_token (&val, (unsigned *)0, cfile);
		    if (token == NUMBER) {
			    int alen = (sizeof buf) - 1;
			    int len;

			    s = &buf [0];
			    len = strlen (val);
			    if (len + 1 > alen) {
			      baddq:
				printf ("usage: server <server>\n");
				skip_to_semi (cfile);
				break;
			    }			    strcpy (buf, val);
			    s += len;
			    token = next_token (&val, (unsigned *)0, cfile);
			    if (token != DOT)
				    goto baddq;
			    *s++ = '.';
			    token = next_token (&val, (unsigned *)0, cfile);
			    if (token != NUMBER)
				    goto baddq;
			    len = strlen (val);
			    if (len + 1 > alen)
				    goto baddq;
			    strcpy (s, val);
			    s += len;
			    token = next_token (&val, (unsigned *)0, cfile);
			    if (token != DOT)
				    goto baddq;
			    *s++ = '.';
			    token = next_token (&val, (unsigned *)0, cfile);
			    if (token != NUMBER)
				    goto baddq;
			    len = strlen (val);
			    if (len + 1 > alen)
				    goto baddq;
			    strcpy (s, val);
			    s += len;
			    token = next_token (&val, (unsigned *)0, cfile);
			    if (token != DOT)
				    goto baddq;
			    *s++ = '.';
			    token = next_token (&val, (unsigned *)0, cfile);
			    if (token != NUMBER)
				    goto baddq;
			    len = strlen (val);
			    if (len + 1 > alen)
				    goto baddq;
			    strcpy (s, val);
			    val = &buf [0];
		    } else if (is_identifier (token)) {
			    /* Use val directly. */
		    } else {
			    printf ("usage: server <server>\n");
			    skip_to_semi (cfile);
			    break;
		    }

		    s = dmalloc (strlen (val) + 1, MDL);
		    if (!server) {
			    printf ("no memory to store server name.\n");
			    skip_to_semi (cfile);
			    break;
		    }
		    strcpy (s, val);
		    server = s;

		    token = next_token (&val, (unsigned *)0, cfile);
		    if (token != END_OF_FILE && token != EOL) {
			    printf ("usage: server <server>\n");
			    skip_to_semi (cfile);
			    break;
		    }
		    break;

		  case KEY:
		    token = next_token (&val, (unsigned *)0, cfile);
		    if (!is_identifier (token)) {
			    printf ("usage: key <name> <value>\n");
			    skip_to_semi (cfile);
			    break;
		    }
		    s = dmalloc (strlen (val) + 1, MDL);
		    if (!s) {
			    printf ("no memory for key name.\n");
			    skip_to_semi (cfile);
			    break;
		    }
		    strcpy (s, val);
		    name = s;
		    memset (&secret, 0, sizeof secret);
		    if (!parse_base64 (&secret, cfile)) {
			    skip_to_semi (cfile);
			    break;
		    }
		    token = next_token (&val, (unsigned *)0, cfile);
		    if (token != END_OF_FILE && token != EOL) {
			    printf ("usage: key <name> <secret>\n");
			    skip_to_semi (cfile);
			    break;
		    }
		    break;

		  case CONNECT:
		    token = next_token (&val, (unsigned *)0, cfile);
		    if (token != END_OF_FILE && token != EOL) {
			    printf ("usage: connect\n");
			    skip_to_semi (cfile);
			    break;
		    }

		    authenticator = dhcpctl_null_handle;

		    if (name) {
			status = dhcpctl_new_authenticator (&authenticator,
							    name, algorithm,
							    secret.data,
							    secret.len);

			if (status != ISC_R_SUCCESS) {
			    fprintf (stderr,
				     "Cannot create authenticator: %s\n",
				     isc_result_totext (status));
			    break;
			}
		    }

		    memset (&connection, 0, sizeof connection);
		    status = dhcpctl_connect (&connection,
					      server, port, authenticator);
		    if (status != ISC_R_SUCCESS) {
			    fprintf (stderr, "dhcpctl_connect: %s\n",
				     isc_result_totext (status));
			    break;
		    }
		    connected = 1;
		    break;

		  case TOKEN_NEW:
		    token = next_token (&val, (unsigned *)0, cfile);
		    if ((!is_identifier (token) && token != STRING)) {
			    printf ("usage: new <object-type>\n");
			    break;
		    }
		    
		    if (oh) {
			    printf ("an object is already open.\n");
			    skip_to_semi (cfile);
			    break;
		    }
		    
		    if (!connected) {
			    printf ("not connected.\n");
			    skip_to_semi (cfile);
			    break;
		    }

		    status = dhcpctl_new_object (&oh, connection, val);
		    if (status != ISC_R_SUCCESS) {
			    printf ("can't create object: %s\n",
				    isc_result_totext (status));
			    break;
		    }
		    
		    token = next_token (&val, (unsigned *)0, cfile);
		    if (token != END_OF_FILE && token != EOL) {
			    printf ("usage: new <object-type>\n");
			    skip_to_semi (cfile);
			    break;
		    }
		    break;

		  case TOKEN_CLOSE:
		    token = next_token (&val, (unsigned *)0, cfile);
		    if (token != END_OF_FILE && token != EOL) {
			    printf ("usage: close\n");
			    skip_to_semi (cfile);
			    break;
		    }

		    if (!connected) {
			    printf ("not connected.\n");
			    skip_to_semi (cfile);
			    break;
		    }

		    if (!oh) {
			    printf ("not open.\n");
			    skip_to_semi (cfile);
			    break;
		    }
		    omapi_object_dereference (&oh, MDL);
		    
		    break;

		  case TOKEN_SET:
		    token = next_token (&val, (unsigned *)0, cfile);

		    if ((!is_identifier (token) && token != STRING)) {
			  set_usage:
			    printf ("usage: set <name> = <value>\n");
			    skip_to_semi (cfile);
			    break;
		    }
		    
		    if (oh == NULL) {
			    printf ("no open object.\n");
			    skip_to_semi (cfile);
			    break;
		    }
		    
		    if (!connected) {
			    printf ("not connected.\n");
			    skip_to_semi (cfile);
			    break;
		    }

		    s1[0] = '\0';
		    strncat (s1, val, sizeof(s1)-1);
		    
		    token = next_token (&val, (unsigned *)0, cfile);
		    if (token != EQUAL)
			    goto set_usage;

		    token = next_token (&val, (unsigned *)0, cfile);
		    switch (token) {
			  case STRING:
			    dhcpctl_set_string_value (oh, val, s1);
			    token = next_token (&val, (unsigned *)0, cfile);
			    break;
			    
			  case NUMBER:
			    strcpy (buf, val);
			    token = peek_token (&val, (unsigned *)0, cfile);
			    /* Colon-seperated hex list? */
			    if (token == COLON)
				goto cshl;
			    else if (token == DOT) {
				s = buf;
				val = buf;
				do {
				    int intval = atoi (val);
				dotiszero:
				    if (intval > 255) {
					parse_warn (cfile,
						    "dotted octet > 255: %s",
						    val);
					skip_to_semi (cfile);
					goto badnum;
				    }
				    *s++ = intval;
				    token = next_token (&val,
							(unsigned *)0, cfile);
				    if (token != DOT)
					    break;
				    /* DOT is zero. */
				    while ((token = next_token (&val,
					(unsigned *)0, cfile)) == DOT)
					*s++ = 0;
				} while (token == NUMBER);
				dhcpctl_set_data_value (oh, buf,
							(unsigned)(s - buf),
							s1);
				break;
			    }
			    dhcpctl_set_int_value (oh, atoi (buf), s1);
			    token = next_token (&val, (unsigned *)0, cfile);
			  badnum:
			    break;
			    
			  case NUMBER_OR_NAME:
			    strcpy (buf, val);
			  cshl:
			    s = buf;
			    val = buf;
			    do {
				convert_num (cfile, (unsigned char *)s,
					     val, 16, 8);
				++s;
				token = next_token (&val,
						    (unsigned *)0, cfile);
				if (token != COLON)
				    break;
				token = next_token (&val,
						    (unsigned *)0, cfile);
			    } while (token == NUMBER ||
				     token == NUMBER_OR_NAME);
			    dhcpctl_set_data_value (oh, buf,
						    (unsigned)(s - buf), s1);
			    break;

			  default:
			    printf ("invalid value.\n");
			    skip_to_semi (cfile);
		    }
		    
		    if (token != END_OF_FILE && token != EOL)
			    goto set_usage;
		    break;
		    
		  case UNSET:
		    token = next_token (&val, (unsigned *)0, cfile);

		    if ((!is_identifier (token) && token != STRING)) {
			  unset_usage:
			    printf ("usage: unset <name>\n");
			    skip_to_semi (cfile);
			    break;
		    }
		    
		    if (!oh) {
			    printf ("no open object.\n");
			    skip_to_semi (cfile);
			    break;
		    }
		    
		    if (!connected) {
			    printf ("not connected.\n");
			    skip_to_semi (cfile);
			    break;
		    }

		    s1[0] = '\0';
		    strncat (s1, val, sizeof(s1)-1);
		    
		    token = next_token (&val, (unsigned *)0, cfile);
		    if (token != END_OF_FILE && token != EOL)
			    goto unset_usage;

		    dhcpctl_set_null_value (oh, s1);
		    break;

			    
		  case TOKEN_CREATE:
		  case TOKEN_OPEN:
		    i = token;
		    token = next_token (&val, (unsigned *)0, cfile);
		    if (token != END_OF_FILE && token != EOL) {
			    printf ("usage: %s\n", val);
			    skip_to_semi (cfile);
			    break;
		    }
		    
		    if (!connected) {
			    printf ("not connected.\n");
			    skip_to_semi (cfile);
			    break;
		    }

		    if (!oh) {
			    printf ("you must make a new object first!\n");
			    skip_to_semi (cfile);
			    break;
		    }

		    if (i == TOKEN_CREATE)
			    i = DHCPCTL_CREATE | DHCPCTL_EXCL;
		    else
			    i = 0;
		    
		    status = dhcpctl_open_object (oh, connection, i);
		    if (status == ISC_R_SUCCESS)
			    status = dhcpctl_wait_for_completion
				    (oh, &waitstatus);
		    if (status == ISC_R_SUCCESS)
			    status = waitstatus;
		    if (status != ISC_R_SUCCESS) {
			    printf ("can't open object: %s\n",
				    isc_result_totext (status));
			    break;
		    }
		    
		    break;

		  case UPDATE:
		    token = next_token (&val, (unsigned *)0, cfile);
		    if (token != END_OF_FILE && token != EOL) {
			    printf ("usage: %s\n", val);
			    skip_to_semi (cfile);
			    break;
		    }
		    
		    if (!connected) {
			    printf ("not connected.\n");
			    skip_to_semi (cfile);
			    break;
		    }

		    if (!oh) {
			    printf ("you haven't opened an object yet!\n");
			    skip_to_semi (cfile);
			    break;
		    }

		    status = dhcpctl_object_update(connection, oh);
		    if (status == ISC_R_SUCCESS)
			    status = dhcpctl_wait_for_completion
				    (oh, &waitstatus);
		    if (status == ISC_R_SUCCESS)
			    status = waitstatus;
		    if (status != ISC_R_SUCCESS) {
			    printf ("can't update object: %s\n",
				    isc_result_totext (status));
			    break;
		    }
		    
		    break;

		  case REMOVE:
		    token = next_token (&val, (unsigned *)0, cfile);
		    if (token != END_OF_FILE && token != EOL) {
			    printf ("usage: remove\n");
			    skip_to_semi (cfile);
			    break;
		    }
		    
		    if (!connected) {
			    printf ("not connected.\n");
			    break;
		    }

		    if (!oh) {
			    printf ("no object.\n");
			    break;
		    }

		    status = dhcpctl_object_remove(connection, oh);
		    if (status == ISC_R_SUCCESS)
			    status = dhcpctl_wait_for_completion
				    (oh, &waitstatus);
		    if (status == ISC_R_SUCCESS)
			    status = waitstatus;
		    if (status != ISC_R_SUCCESS) {
			    printf ("can't destroy object: %s\n",
				    isc_result_totext (status));
			    break;
		    }
		    omapi_object_dereference (&oh, MDL);
		    break;

		  case REFRESH:
		    token = next_token (&val, (unsigned *)0, cfile);
		    if (token != END_OF_FILE && token != EOL) {
			    printf ("usage: refresh\n");
			    skip_to_semi (cfile);
			    break;
		    }
		    
		    if (!connected) {
			    printf ("not connected.\n");
			    break;
		    }

		    if (!oh) {
			    printf ("no object.\n");
			    break;
		    }

		    status = dhcpctl_object_refresh(connection, oh);
		    if (status == ISC_R_SUCCESS)
			    status = dhcpctl_wait_for_completion
				    (oh, &waitstatus);
		    if (status == ISC_R_SUCCESS)
			    status = waitstatus;
		    if (status != ISC_R_SUCCESS) {
			    printf ("can't refresh object: %s\n",
				    isc_result_totext (status));
			    break;
		    }
		    
		    break;
	    }
	} while (1);

	exit (0);
}

/* Sigh */
isc_result_t dhcp_set_control_state (control_object_state_t oldstate,
				     control_object_state_t newstate)
{
	return ISC_R_SUCCESS;
}
