/* clparse.c

   Parser for dhclient config and lease files... */

/*
 * Copyright (c) 1996-2001 Internet Software Consortium.
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
"$Id: clparse.c,v 1.62.2.1 2001/06/01 17:26:44 mellon Exp $ Copyright (c) 1996-2001 The Internet Software Consortium.  All rights reserved.\n"
"$FreeBSD$\n";
#endif /* not lint */

#include "dhcpd.h"

static TIME parsed_time;

struct client_config top_level_config;

char client_script_name [] = "/sbin/dhclient-script";

u_int32_t default_requested_options [] = {
	DHO_SUBNET_MASK,
	DHO_BROADCAST_ADDRESS,
	DHO_TIME_OFFSET,
	DHO_ROUTERS,
	DHO_DOMAIN_NAME,
	DHO_DOMAIN_NAME_SERVERS,
	DHO_HOST_NAME,
	0
};

/* client-conf-file :== client-declarations END_OF_FILE
   client-declarations :== <nil>
			 | client-declaration
			 | client-declarations client-declaration */

isc_result_t read_client_conf ()
{
	struct client_config *config;
	struct client_state *state;
	struct interface_info *ip;
	isc_result_t status;

	/* Set up the initial dhcp option universe. */
	initialize_common_option_spaces ();

	/* Initialize the top level client configuration. */
	memset (&top_level_config, 0, sizeof top_level_config);

	/* Set some defaults... */
	top_level_config.timeout = 60;
	top_level_config.select_interval = 0;
	top_level_config.reboot_timeout = 10;
	top_level_config.retry_interval = 300;
	top_level_config.backoff_cutoff = 15;
	top_level_config.initial_interval = 3;
	top_level_config.bootp_policy = P_ACCEPT;
	top_level_config.script_name = path_dhclient_script;
	top_level_config.requested_options = default_requested_options;
	top_level_config.omapi_port = -1;

	group_allocate (&top_level_config.on_receipt, MDL);
	if (!top_level_config.on_receipt)
		log_fatal ("no memory for top-level on_receipt group");

	group_allocate (&top_level_config.on_transmission, MDL);
	if (!top_level_config.on_transmission)
		log_fatal ("no memory for top-level on_transmission group");

	status = read_client_conf_file (path_dhclient_conf,
					(struct interface_info *)0,
					&top_level_config);
	if (status != ISC_R_SUCCESS) {
		;
#ifdef LATER
		/* Set up the standard name service updater routine. */
		parse = (struct parse *)0;
		status = new_parse (&parse, -1, default_client_config,
				    (sizeof default_client_config) - 1,
				    "default client configuration", 0);
		if (status != ISC_R_SUCCESS)
			log_fatal ("can't begin default client config!");

		do {
			token = peek_token (&val, (unsigned *)0, cfile);
			if (token == END_OF_FILE)
				break;
			parse_client_statement (cfile,
						(struct interface_info *)0,
						&top_level_config);
		} while (1);
		end_parse (&parse);
#endif
	}

	/* Set up state and config structures for clients that don't
	   have per-interface configuration statements. */
	config = (struct client_config *)0;
	for (ip = interfaces; ip; ip = ip -> next) {
		if (!ip -> client) {
			ip -> client = (struct client_state *)
				dmalloc (sizeof (struct client_state), MDL);
			if (!ip -> client)
				log_fatal ("no memory for client state.");
			memset (ip -> client, 0, sizeof *(ip -> client));
			ip -> client -> interface = ip;
		}

		if (!ip -> client -> config) {
			if (!config) {
				config = (struct client_config *)
					dmalloc (sizeof (struct client_config),
						 MDL);
				if (!config)
				    log_fatal ("no memory for client config.");
				memcpy (config, &top_level_config,
					sizeof top_level_config);
			}
			ip -> client -> config = config;
		}
	}
	return status;
}

int read_client_conf_file (const char *name, struct interface_info *ip,
			   struct client_config *client)
{
	int file;
	struct parse *cfile;
	const char *val;
	int token;
	isc_result_t status;
	
	if ((file = open (name, O_RDONLY)) < 0)
		return uerr2isc (errno);

	cfile = (struct parse *)0;
	new_parse (&cfile, file, (char *)0, 0, path_dhclient_conf, 0);

	do {
		token = peek_token (&val, (unsigned *)0, cfile);
		if (token == END_OF_FILE)
			break;
		parse_client_statement (cfile, ip, client);
	} while (1);
	token = next_token (&val, (unsigned *)0, cfile);
	status = (cfile -> warnings_occurred
		  ? ISC_R_BADPARSE
		  : ISC_R_SUCCESS);
	close (file);
	end_parse (&cfile);
	return status;
}


/* lease-file :== client-lease-statements END_OF_FILE
   client-lease-statements :== <nil>
		     | client-lease-statements LEASE client-lease-statement */

void read_client_leases ()
{
	int file;
	struct parse *cfile;
	const char *val;
	int token;

	/* Open the lease file.   If we can't open it, just return -
	   we can safely trust the server to remember our state. */
	if ((file = open (path_dhclient_db, O_RDONLY)) < 0)
		return;
	cfile = (struct parse *)0;
	new_parse (&cfile, file, (char *)0, 0, path_dhclient_db, 0);

	do {
		token = next_token (&val, (unsigned *)0, cfile);
		if (token == END_OF_FILE)
			break;
		if (token != LEASE) {
			log_error ("Corrupt lease file - possible data loss!");
			skip_to_semi (cfile);
			break;
		} else
			parse_client_lease_statement (cfile, 0);

	} while (1);

	close (file);
	end_parse (&cfile);
}

/* client-declaration :== 
	SEND option-decl |
	DEFAULT option-decl |
	SUPERSEDE option-decl |
	PREPEND option-decl |
	APPEND option-decl |
	hardware-declaration |
	REQUEST option-list |
	REQUIRE option-list |
	TIMEOUT number |
	RETRY number |
	REBOOT number |
	SELECT_TIMEOUT number |
	SCRIPT string |
	VENDOR_SPACE string |
	interface-declaration |
	LEASE client-lease-statement |
	ALIAS client-lease-statement |
	KEY key-definition */

void parse_client_statement (cfile, ip, config)
	struct parse *cfile;
	struct interface_info *ip;
	struct client_config *config;
{
	int token;
	const char *val;
	struct option *option;
	struct executable_statement *stmt, **p;
	enum statement_op op;
	int lose;
	char *name;
	struct data_string key_id;
	enum policy policy;
	int known;
	int tmp, i;
	isc_result_t status;

	switch (peek_token (&val, (unsigned *)0, cfile)) {
	      case INCLUDE:
		next_token (&val, (unsigned *)0, cfile);
		token = next_token (&val, (unsigned *)0, cfile);
		if (token != STRING) {
			parse_warn (cfile, "filename string expected.");
			skip_to_semi (cfile);
		} else {
			status = read_client_conf_file (val, ip, config);
			if (status != ISC_R_SUCCESS)
				parse_warn (cfile, "%s: bad parse.", val);
			parse_semi (cfile);
		}
		return;
		
	      case KEY:
		next_token (&val, (unsigned *)0, cfile);
		if (ip) {
			/* This may seem arbitrary, but there's a reason for
			   doing it: the authentication key database is not
			   scoped.  If we allow the user to declare a key other
			   than in the outer scope, the user is very likely to
			   believe that the key will only be used in that
			   scope.  If the user only wants the key to be used on
			   one interface, because it's known that the other
			   interface may be connected to an insecure net and
			   the secret key is considered sensitive, we don't
			   want to lull them into believing they've gotten
			   their way.   This is a bit contrived, but people
			   tend not to be entirely rational about security. */
			parse_warn (cfile, "key definition not allowed here.");
			skip_to_semi (cfile);
			break;
		}
		parse_key (cfile);
		return;

		/* REQUIRE can either start a policy statement or a
		   comma-seperated list of names of required options. */
	      case REQUIRE:
		next_token (&val, (unsigned *)0, cfile);
		token = peek_token (&val, (unsigned *)0, cfile);
		if (token == AUTHENTICATION) {
			policy = P_REQUIRE;
			goto do_policy;
		}
		parse_option_list (cfile, &config -> required_options);
		return;

	      case IGNORE:
		next_token (&val, (unsigned *)0, cfile);
		policy = P_IGNORE;
		goto do_policy;

	      case ACCEPT:
		next_token (&val, (unsigned *)0, cfile);
		policy = P_ACCEPT;
		goto do_policy;

	      case PREFER:
		next_token (&val, (unsigned *)0, cfile);
		policy = P_PREFER;
		goto do_policy;

	      case DONT:
		next_token (&val, (unsigned *)0, cfile);
		policy = P_DONT;
		goto do_policy;

	      do_policy:
		token = next_token (&val, (unsigned *)0, cfile);
		if (token == AUTHENTICATION) {
			if (policy != P_PREFER &&
			    policy != P_REQUIRE &&
			    policy != P_DONT) {
				parse_warn (cfile,
					    "invalid authentication policy.");
				skip_to_semi (cfile);
				return;
			}
			config -> auth_policy = policy;
		} else if (token != TOKEN_BOOTP) {
			if (policy != P_PREFER &&
			    policy != P_IGNORE &&
			    policy != P_ACCEPT) {
				parse_warn (cfile, "invalid bootp policy.");
				skip_to_semi (cfile);
				return;
			}
			config -> bootp_policy = policy;
		} else {
			parse_warn (cfile, "expecting a policy type.");
			skip_to_semi (cfile);
			return;
		} 
		break;

	      case OPTION:
		token = next_token (&val, (unsigned *)0, cfile);

		token = peek_token (&val, (unsigned *)0, cfile);
		if (token == SPACE) {
			if (ip) {
				parse_warn (cfile,
					    "option space definitions %s",
					    " may not be scoped.");
				skip_to_semi (cfile);
				break;
			}
			parse_option_space_decl (cfile);
			return;
		}

		option = parse_option_name (cfile, 1, &known);
		if (!option)
			return;

		token = next_token (&val, (unsigned *)0, cfile);
		if (token != CODE) {
			parse_warn (cfile, "expecting \"code\" keyword.");
			skip_to_semi (cfile);
			free_option (option, MDL);
			return;
		}
		if (ip) {
			parse_warn (cfile,
				    "option definitions may only appear in %s",
				    "the outermost scope.");
			skip_to_semi (cfile);
			free_option (option, MDL);
			return;
		}
		if (!parse_option_code_definition (cfile, option))
			free_option (option, MDL);
		return;

	      case MEDIA:
		token = next_token (&val, (unsigned *)0, cfile);
		parse_string_list (cfile, &config -> media, 1);
		return;

	      case HARDWARE:
		token = next_token (&val, (unsigned *)0, cfile);
		if (ip) {
			parse_hardware_param (cfile, &ip -> hw_address);
		} else {
			parse_warn (cfile, "hardware address parameter %s",
				    "not allowed here.");
			skip_to_semi (cfile);
		}
		return;

	      case REQUEST:
		token = next_token (&val, (unsigned *)0, cfile);
		if (config -> requested_options == default_requested_options)
			config -> requested_options = (u_int32_t *)0;
		parse_option_list (cfile, &config -> requested_options);
		return;

	      case TIMEOUT:
		token = next_token (&val, (unsigned *)0, cfile);
		parse_lease_time (cfile, &config -> timeout);
		return;

	      case RETRY:
		token = next_token (&val, (unsigned *)0, cfile);
		parse_lease_time (cfile, &config -> retry_interval);
		return;

	      case SELECT_TIMEOUT:
		token = next_token (&val, (unsigned *)0, cfile);
		parse_lease_time (cfile, &config -> select_interval);
		return;

	      case OMAPI:
		token = next_token (&val, (unsigned *)0, cfile);
		token = next_token (&val, (unsigned *)0, cfile);
		if (token != PORT) {
			parse_warn (cfile,
				    "unexpected omapi subtype: %s", val);
			skip_to_semi (cfile);
			return;
		}
		token = next_token (&val, (unsigned *)0, cfile);
		if (token != NUMBER) {
			parse_warn (cfile, "invalid port number: `%s'", val);
			skip_to_semi (cfile);
			return;
		}
		tmp = atoi (val);
		if (tmp < 0 || tmp > 65535)
			parse_warn (cfile, "invalid omapi port %d.", tmp);
		else if (config != &top_level_config)
			parse_warn (cfile,
				    "omapi port only works at top level.");
		else
			config -> omapi_port = tmp;
		parse_semi (cfile);
		return;
		
	      case REBOOT:
		token = next_token (&val, (unsigned *)0, cfile);
		parse_lease_time (cfile, &config -> reboot_timeout);
		return;

	      case BACKOFF_CUTOFF:
		token = next_token (&val, (unsigned *)0, cfile);
		parse_lease_time (cfile, &config -> backoff_cutoff);
		return;

	      case INITIAL_INTERVAL:
		token = next_token (&val, (unsigned *)0, cfile);
		parse_lease_time (cfile, &config -> initial_interval);
		return;

	      case SCRIPT:
		token = next_token (&val, (unsigned *)0, cfile);
		parse_string (cfile, &config -> script_name, (unsigned *)0);
		return;

	      case VENDOR:
		token = next_token (&val, (unsigned *)0, cfile);
		token = next_token (&val, (unsigned *)0, cfile);
		if (token != OPTION) {
			parse_warn (cfile, "expecting 'vendor option space'");
			skip_to_semi (cfile);
			return;
		}
		token = next_token (&val, (unsigned *)0, cfile);
		if (token != SPACE) {
			parse_warn (cfile, "expecting 'vendor option space'");
			skip_to_semi (cfile);
			return;
		}
		token = next_token (&val, (unsigned *)0, cfile);
		if (!is_identifier (token)) {
			parse_warn (cfile, "expecting an identifier.");
			skip_to_semi (cfile);
			return;
		}
		config -> vendor_space_name = dmalloc (strlen (val) + 1, MDL);
		if (!config -> vendor_space_name)
			log_fatal ("no memory for vendor option space name.");
		strcpy (config -> vendor_space_name, val);
		for (i = 0; i < universe_count; i++)
			if (!strcmp (universes [i] -> name,
				     config -> vendor_space_name))
				break;
		if (i == universe_count) {
			log_error ("vendor option space %s not found.",
				   config -> vendor_space_name);
		}
		parse_semi (cfile);
		return;

	      case INTERFACE:
		token = next_token (&val, (unsigned *)0, cfile);
		if (ip)
			parse_warn (cfile, "nested interface declaration.");
		parse_interface_declaration (cfile, config, (char *)0);
		return;

	      case PSEUDO:
		token = next_token (&val, (unsigned *)0, cfile);
		token = next_token (&val, (unsigned *)0, cfile);
		name = dmalloc (strlen (val) + 1, MDL);
		if (!name)
			log_fatal ("no memory for pseudo interface name");
		strcpy (name, val);
		parse_interface_declaration (cfile, config, name);
		return;
		
	      case LEASE:
		token = next_token (&val, (unsigned *)0, cfile);
		parse_client_lease_statement (cfile, 1);
		return;

	      case ALIAS:
		token = next_token (&val, (unsigned *)0, cfile);
		parse_client_lease_statement (cfile, 2);
		return;

	      case REJECT:
		token = next_token (&val, (unsigned *)0, cfile);
		parse_reject_statement (cfile, config);
		return;

	      default:
		lose = 0;
		stmt = (struct executable_statement *)0;
		if (!parse_executable_statement (&stmt,
						 cfile, &lose, context_any)) {
			if (!lose) {
				parse_warn (cfile, "expecting a statement.");
				skip_to_semi (cfile);
			}
		} else {
			struct executable_statement **eptr, *sptr;
			if (stmt &&
			    (stmt -> op == send_option_statement ||
			     (stmt -> op == on_statement &&
			      (stmt -> data.on.evtypes & ON_TRANSMISSION)))) {
			    eptr = &config -> on_transmission -> statements;
			    if (stmt -> op == on_statement) {
				    sptr = (struct executable_statement *)0;
				    executable_statement_reference
					    (&sptr,
					     stmt -> data.on.statements, MDL);
				    executable_statement_dereference (&stmt,
								      MDL);
				    executable_statement_reference (&stmt,
								    sptr,
								    MDL);
				    executable_statement_dereference (&sptr,
								      MDL);
			    }
			} else
			    eptr = &config -> on_receipt -> statements;

			if (stmt) {
				for (; *eptr; eptr = &(*eptr) -> next)
					;
				executable_statement_reference (eptr,
								stmt, MDL);
			}
			return;
		}
		break;
	}
	parse_semi (cfile);
}

/* option-list :== option_name |
   		   option_list COMMA option_name */

void parse_option_list (cfile, list)
	struct parse *cfile;
	u_int32_t **list;
{
	int ix, i;
	int token;
	const char *val;
	pair p = (pair)0, q, r;

	ix = 0;
	do {
		token = next_token (&val, (unsigned *)0, cfile);
		if (token == SEMI)
			break;
		if (!is_identifier (token)) {
			parse_warn (cfile, "%s: expected option name.", val);
			skip_to_semi (cfile);
			return;
		}
		for (i = 0; i < 256; i++) {
			if (!strcasecmp (dhcp_options [i].name, val))
				break;
		}
		if (i == 256) {
			parse_warn (cfile, "%s: expected option name.", val);
			skip_to_semi (cfile);
			return;
		}
		r = new_pair (MDL);
		if (!r)
			log_fatal ("can't allocate pair for option code.");
		r -> car = (caddr_t)(long)i;
		r -> cdr = (pair)0;
		if (p)
			q -> cdr = r;
		else
			p = r;
		q = r;
		++ix;
		token = next_token (&val, (unsigned *)0, cfile);
	} while (token == COMMA);
	if (token != SEMI) {
		parse_warn (cfile, "expecting semicolon.");
		skip_to_semi (cfile);
		return;
	}
	/* XXX we can't free the list here, because we may have copied
	   XXX it from an outer config state. */
	*list = (u_int32_t *)0;
	if (ix) {
		*list = dmalloc ((ix + 1) * sizeof **list, MDL);
		if (!*list)
			log_error ("no memory for option list.");
		else {
			ix = 0;
			for (q = p; q; q = q -> cdr)
				(*list) [ix++] = (u_int32_t)(long)q -> car;
			(*list) [ix] = 0;
		}
		while (p) {
			q = p -> cdr;
			free_pair (p, MDL);
			p = q;
		}
	}
}

/* interface-declaration :==
   	INTERFACE string LBRACE client-declarations RBRACE */

void parse_interface_declaration (cfile, outer_config, name)
	struct parse *cfile;
	struct client_config *outer_config;
	char *name;
{
	int token;
	const char *val;
	struct client_state *client, **cp;
	struct interface_info *ip = (struct interface_info *)0;

	token = next_token (&val, (unsigned *)0, cfile);
	if (token != STRING) {
		parse_warn (cfile, "expecting interface name (in quotes).");
		skip_to_semi (cfile);
		return;
	}

	if (!interface_or_dummy (&ip, val))
		log_fatal ("Can't allocate interface %s.", val);

	/* If we were given a name, this is a pseudo-interface. */
	if (name) {
		make_client_state (&client);
		client -> name = name;
		client -> interface = ip;
		for (cp = &ip -> client; *cp; cp = &((*cp) -> next))
			;
		*cp = client;
	} else {
		if (!ip -> client) {
			make_client_state (&ip -> client);
			ip -> client -> interface = ip;
		}
		client = ip -> client;
	}

	if (!client -> config)
		make_client_config (client, outer_config);

	ip -> flags &= ~INTERFACE_AUTOMATIC;
	interfaces_requested = 1;

	token = next_token (&val, (unsigned *)0, cfile);
	if (token != LBRACE) {
		parse_warn (cfile, "expecting left brace.");
		skip_to_semi (cfile);
		return;
	}

	do {
		token = peek_token (&val, (unsigned *)0, cfile);
		if (token == END_OF_FILE) {
			parse_warn (cfile,
				    "unterminated interface declaration.");
			return;
		}
		if (token == RBRACE)
			break;
		parse_client_statement (cfile, ip, client -> config);
	} while (1);
	token = next_token (&val, (unsigned *)0, cfile);
}

int interface_or_dummy (struct interface_info **pi, const char *name)
{
	struct interface_info *i;
	struct interface_info *ip = (struct interface_info *)0;
	isc_result_t status;

	/* Find the interface (if any) that matches the name. */
	for (i = interfaces; i; i = i -> next) {
		if (!strcmp (i -> name, name)) {
			interface_reference (&ip, i, MDL);
			break;
		}
	}

	/* If it's not a real interface, see if it's on the dummy list. */
	if (!ip) {
		for (ip = dummy_interfaces; ip; ip = ip -> next) {
			if (!strcmp (ip -> name, name)) {
				interface_reference (&ip, i, MDL);
				break;
			}
		}
	}

	/* If we didn't find an interface, make a dummy interface as
	   a placeholder. */
	if (!ip) {
		isc_result_t status;
		status = interface_allocate (&ip, MDL);
		if (status != ISC_R_SUCCESS)
			log_fatal ("Can't record interface %s: %s",
				   name, isc_result_totext (status));
		strlcpy (ip -> name, name, IFNAMSIZ);
		if (dummy_interfaces) {
			interface_reference (&ip -> next,
					     dummy_interfaces, MDL);
			interface_dereference (&dummy_interfaces, MDL);
		}
		interface_reference (&dummy_interfaces, ip, MDL);
	}
	if (pi)
		status = interface_reference (pi, ip, MDL);
	interface_dereference (&ip, MDL);
	if (status != ISC_R_SUCCESS)
		return 0;
	return 1;
}

void make_client_state (state)
	struct client_state **state;
{
	*state = ((struct client_state *)dmalloc (sizeof **state, MDL));
	if (!*state)
		log_fatal ("no memory for client state\n");
	memset (*state, 0, sizeof **state);
}

void make_client_config (client, config)
	struct client_state *client;
	struct client_config *config;
{
	client -> config = (((struct client_config *)
			     dmalloc (sizeof (struct client_config), MDL)));
	if (!client -> config)
		log_fatal ("no memory for client config\n");
	memcpy (client -> config, config, sizeof *config);
	if (!clone_group (&client -> config -> on_receipt,
			  config -> on_receipt, MDL) ||
	    !clone_group (&client -> config -> on_transmission,
			  config -> on_transmission, MDL))
		log_fatal ("no memory for client state groups.");
}

/* client-lease-statement :==
	RBRACE client-lease-declarations LBRACE

	client-lease-declarations :==
		<nil> |
		client-lease-declaration |
		client-lease-declarations client-lease-declaration */


void parse_client_lease_statement (cfile, is_static)
	struct parse *cfile;
	int is_static;
{
	struct client_lease *lease, *lp, *pl;
	struct interface_info *ip = (struct interface_info *)0;
	int token;
	const char *val;
	struct client_state *client = (struct client_state *)0;

	token = next_token (&val, (unsigned *)0, cfile);
	if (token != LBRACE) {
		parse_warn (cfile, "expecting left brace.");
		skip_to_semi (cfile);
		return;
	}

	lease = ((struct client_lease *)
		 dmalloc (sizeof (struct client_lease), MDL));
	if (!lease)
		log_fatal ("no memory for lease.\n");
	memset (lease, 0, sizeof *lease);
	lease -> is_static = is_static;
	if (!option_state_allocate (&lease -> options, MDL))
		log_fatal ("no memory for lease options.\n");

	do {
		token = peek_token (&val, (unsigned *)0, cfile);
		if (token == END_OF_FILE) {
			parse_warn (cfile, "unterminated lease declaration.");
			return;
		}
		if (token == RBRACE)
			break;
		parse_client_lease_declaration (cfile, lease, &ip, &client);
	} while (1);
	token = next_token (&val, (unsigned *)0, cfile);

	/* If the lease declaration didn't include an interface
	   declaration that we recognized, it's of no use to us. */
	if (!ip) {
		destroy_client_lease (lease);
		return;
	}

	/* Make sure there's a client state structure... */
	if (!ip -> client) {
		make_client_state (&ip -> client);
		ip -> client -> interface = ip;
	}
	if (!client)
		client = ip -> client;

	/* If this is an alias lease, it doesn't need to be sorted in. */
	if (is_static == 2) {
		ip -> client -> alias = lease;
		return;
	}

	/* The new lease may supersede a lease that's not the
	   active lease but is still on the lease list, so scan the
	   lease list looking for a lease with the same address, and
	   if we find it, toss it. */
	pl = (struct client_lease *)0;
	for (lp = client -> leases; lp; lp = lp -> next) {
		if (lp -> address.len == lease -> address.len &&
		    !memcmp (lp -> address.iabuf, lease -> address.iabuf,
			     lease -> address.len)) {
			if (pl)
				pl -> next = lp -> next;
			else
				client -> leases = lp -> next;
			destroy_client_lease (lp);
			break;
		}
	}

	/* If this is a preloaded lease, just put it on the list of recorded
	   leases - don't make it the active lease. */
	if (is_static) {
		lease -> next = client -> leases;
		client -> leases = lease;
		return;
	}
		
	/* The last lease in the lease file on a particular interface is
	   the active lease for that interface.    Of course, we don't know
	   what the last lease in the file is until we've parsed the whole
	   file, so at this point, we assume that the lease we just parsed
	   is the active lease for its interface.   If there's already
	   an active lease for the interface, and this lease is for the same
	   ip address, then we just toss the old active lease and replace
	   it with this one.   If this lease is for a different address,
	   then if the old active lease has expired, we dump it; if not,
	   we put it on the list of leases for this interface which are
	   still valid but no longer active. */
	if (client -> active) {
		if (client -> active -> expiry < cur_time)
			destroy_client_lease (client -> active);
		else if (client -> active -> address.len ==
			 lease -> address.len &&
			 !memcmp (client -> active -> address.iabuf,
				  lease -> address.iabuf,
				  lease -> address.len))
			destroy_client_lease (client -> active);
		else {
			client -> active -> next = client -> leases;
			client -> leases = client -> active;
		}
	}
	client -> active = lease;

	/* phew. */
}

/* client-lease-declaration :==
	BOOTP |
	INTERFACE string |
	FIXED_ADDR ip_address |
	FILENAME string |
	SERVER_NAME string |
	OPTION option-decl |
	RENEW time-decl |
	REBIND time-decl |
	EXPIRE time-decl |
	KEY id */

void parse_client_lease_declaration (cfile, lease, ipp, clientp)
	struct parse *cfile;
	struct client_lease *lease;
	struct interface_info **ipp;
	struct client_state **clientp;
{
	int token;
	const char *val;
	char *t, *n;
	struct interface_info *ip;
	struct option_cache *oc;
	struct client_state *client = (struct client_state *)0;
	struct data_string key_id;

	switch (next_token (&val, (unsigned *)0, cfile)) {
	      case KEY:
		token = next_token (&val, (unsigned *)0, cfile);
		if (token != STRING && !is_identifier (token)) {
			parse_warn (cfile, "expecting key name.");
			skip_to_semi (cfile);
			break;
		}
		if (omapi_auth_key_lookup_name (&lease -> key, val) !=
		    ISC_R_SUCCESS)
			parse_warn (cfile, "unknown key %s", val);
		parse_semi (cfile);
		break;
	      case TOKEN_BOOTP:
		lease -> is_bootp = 1;
		break;

	      case INTERFACE:
		token = next_token (&val, (unsigned *)0, cfile);
		if (token != STRING) {
			parse_warn (cfile,
				    "expecting interface name (in quotes).");
			skip_to_semi (cfile);
			break;
		}
		interface_or_dummy (ipp, val);
		break;

	      case NAME:
		token = next_token (&val, (unsigned *)0, cfile);
		ip = *ipp;
		if (!ip) {
			parse_warn (cfile, "state name precedes interface.");
			break;
		}
		for (client = ip -> client; client; client = client -> next)
			if (client -> name && !strcmp (client -> name, val))
				break;
		if (!client)
			parse_warn (cfile,
				    "lease specified for unknown pseudo.");
		*clientp = client;
		break;

	      case FIXED_ADDR:
		if (!parse_ip_addr (cfile, &lease -> address))
			return;
		break;

	      case MEDIUM:
		parse_string_list (cfile, &lease -> medium, 0);
		return;

	      case FILENAME:
		parse_string (cfile, &lease -> filename, (unsigned *)0);
		return;

	      case SERVER_NAME:
		parse_string (cfile, &lease -> server_name, (unsigned *)0);
		return;

	      case RENEW:
		lease -> renewal = parse_date (cfile);
		return;

	      case REBIND:
		lease -> rebind = parse_date (cfile);
		return;

	      case EXPIRE:
		lease -> expiry = parse_date (cfile);
		return;

	      case OPTION:
		oc = (struct option_cache *)0;
		if (parse_option_decl (&oc, cfile)) {
			save_option (oc -> option -> universe,
				     lease -> options, oc);
			option_cache_dereference (&oc, MDL);
		}
		return;

	      default:
		parse_warn (cfile, "expecting lease declaration.");
		skip_to_semi (cfile);
		break;
	}
	token = next_token (&val, (unsigned *)0, cfile);
	if (token != SEMI) {
		parse_warn (cfile, "expecting semicolon.");
		skip_to_semi (cfile);
	}
}

void parse_string_list (cfile, lp, multiple)
	struct parse *cfile;
	struct string_list **lp;
	int multiple;
{
	int token;
	const char *val;
	struct string_list *cur, *tmp;

	/* Find the last medium in the media list. */
	if (*lp) {
		for (cur = *lp; cur -> next; cur = cur -> next)
			;
	} else {
		cur = (struct string_list *)0;
	}

	do {
		token = next_token (&val, (unsigned *)0, cfile);
		if (token != STRING) {
			parse_warn (cfile, "Expecting media options.");
			skip_to_semi (cfile);
			return;
		}

		tmp = ((struct string_list *)
		       dmalloc (strlen (val) + sizeof (struct string_list),
				MDL));
		if (!tmp)
			log_fatal ("no memory for string list entry.");

		strcpy (tmp -> string, val);
		tmp -> next = (struct string_list *)0;

		/* Store this medium at the end of the media list. */
		if (cur)
			cur -> next = tmp;
		else
			*lp = tmp;
		cur = tmp;

		token = next_token (&val, (unsigned *)0, cfile);
	} while (multiple && token == COMMA);

	if (token != SEMI) {
		parse_warn (cfile, "expecting semicolon.");
		skip_to_semi (cfile);
	}
}

void parse_reject_statement (cfile, config)
	struct parse *cfile;
	struct client_config *config;
{
	int token;
	const char *val;
	struct iaddr addr;
	struct iaddrlist *list;

	do {
		if (!parse_ip_addr (cfile, &addr)) {
			parse_warn (cfile, "expecting IP address.");
			skip_to_semi (cfile);
			return;
		}

		list = (struct iaddrlist *)dmalloc (sizeof (struct iaddrlist),
						    MDL);
		if (!list)
			log_fatal ("no memory for reject list!");

		list -> addr = addr;
		list -> next = config -> reject_list;
		config -> reject_list = list;

		token = next_token (&val, (unsigned *)0, cfile);
	} while (token == COMMA);

	if (token != SEMI) {
		parse_warn (cfile, "expecting semicolon.");
		skip_to_semi (cfile);
	}
}	

/* allow-deny-keyword :== BOOTP
   			| BOOTING
			| DYNAMIC_BOOTP
			| UNKNOWN_CLIENTS */

int parse_allow_deny (oc, cfile, flag)
	struct option_cache **oc;
	struct parse *cfile;
	int flag;
{
	enum dhcp_token token;
	const char *val;
	unsigned char rf = flag;
	struct expression *data = (struct expression *)0;
	int status;

	parse_warn (cfile, "allow/deny/ignore not permitted here.");
	skip_to_semi (cfile);
	return 0;
}

