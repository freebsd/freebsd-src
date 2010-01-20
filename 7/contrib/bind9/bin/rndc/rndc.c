/*
 * Copyright (C) 2004-2006, 2008  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 2000-2003  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/* $Id: rndc.c,v 1.96.18.21 2008/10/15 03:07:19 marka Exp $ */

/*! \file */

/*
 * Principal Author: DCL
 */

#include <config.h>

#include <stdlib.h>

#include <isc/app.h>
#include <isc/buffer.h>
#include <isc/commandline.h>
#include <isc/file.h>
#include <isc/log.h>
#include <isc/net.h>
#include <isc/mem.h>
#include <isc/random.h>
#include <isc/socket.h>
#include <isc/stdtime.h>
#include <isc/string.h>
#include <isc/task.h>
#include <isc/thread.h>
#include <isc/util.h>

#include <isccfg/namedconf.h>

#include <isccc/alist.h>
#include <isccc/base64.h>
#include <isccc/cc.h>
#include <isccc/ccmsg.h>
#include <isccc/result.h>
#include <isccc/sexpr.h>
#include <isccc/types.h>
#include <isccc/util.h>

#include <dns/name.h>

#include <bind9/getaddresses.h>

#include "util.h"

#define SERVERADDRS 10

const char *progname;
isc_boolean_t verbose;

static const char *admin_conffile;
static const char *admin_keyfile;
static const char *version = VERSION;
static const char *servername = NULL;
static isc_sockaddr_t serveraddrs[SERVERADDRS];
static isc_sockaddr_t local4, local6;
static isc_boolean_t local4set = ISC_FALSE, local6set = ISC_FALSE;
static int nserveraddrs;
static int currentaddr = 0;
static unsigned int remoteport = 0;
static isc_socketmgr_t *socketmgr = NULL;
static unsigned char databuf[2048];
static isccc_ccmsg_t ccmsg;
static isccc_region_t secret;
static isc_boolean_t failed = ISC_FALSE;
static isc_mem_t *mctx;
static int sends, recvs, connects;
static char *command;
static char *args;
static char program[256];
static isc_socket_t *sock = NULL;
static isc_uint32_t serial;

static void rndc_startconnect(isc_sockaddr_t *addr, isc_task_t *task);

static void
usage(int status) {
	fprintf(stderr, "\
Usage: %s [-c config] [-s server] [-p port]\n\
	[-k key-file ] [-y key] [-V] command\n\
\n\
command is one of the following:\n\
\n\
  reload	Reload configuration file and zones.\n\
  reload zone [class [view]]\n\
		Reload a single zone.\n\
  refresh zone [class [view]]\n\
		Schedule immediate maintenance for a zone.\n\
  retransfer zone [class [view]]\n\
		Retransfer a single zone without checking serial number.\n\
  freeze	Suspend updates to all dynamic zones.\n\
  freeze zone [class [view]]\n\
		Suspend updates to a dynamic zone.\n\
  thaw		Enable updates to all dynamic zones and reload them.\n\
  thaw zone [class [view]]\n\
		Enable updates to a frozen dynamic zone and reload it.\n\
  notify zone [class [view]]\n\
		Resend NOTIFY messages for the zone.\n\
  reconfig	Reload configuration file and new zones only.\n\
  stats		Write server statistics to the statistics file.\n\
  querylog	Toggle query logging.\n\
  dumpdb [-all|-cache|-zones] [view ...]\n\
		Dump cache(s) to the dump file (named_dump.db).\n\
  stop		Save pending updates to master files and stop the server.\n\
  stop -p	Save pending updates to master files and stop the server\n\
		reporting process id.\n\
  halt		Stop the server without saving pending updates.\n\
  halt -p	Stop the server without saving pending updates reporting\n\
		process id.\n\
  trace		Increment debugging level by one.\n\
  trace level	Change the debugging level.\n\
  notrace	Set debugging level to 0.\n\
  flush 	Flushes all of the server's caches.\n\
  flush [view]	Flushes the server's cache for a view.\n\
  flushname name [view]\n\
		Flush the given name from the server's cache(s)\n\
  status	Display status of the server.\n\
  recursing	Dump the queries that are currently recursing (named.recursing)\n\
  validation newstate [view]\n\
		Enable / disable DNSSEC validation.\n\
  *restart	Restart the server.\n\
\n\
* == not yet implemented\n\
Version: %s\n",
		progname, version);

	exit(status);
}

static void
get_addresses(const char *host, in_port_t port) {
	isc_result_t result;
	int found = 0, count;

	if (*host == '/') {
		result = isc_sockaddr_frompath(&serveraddrs[nserveraddrs],
					       host);
		if (result == ISC_R_SUCCESS)
			nserveraddrs++;
	} else {
		count = SERVERADDRS - nserveraddrs;
		result = bind9_getaddresses(host, port,
					    &serveraddrs[nserveraddrs],
					    count, &found);
		nserveraddrs += found;
	}
	if (result != ISC_R_SUCCESS)
		fatal("couldn't get address for '%s': %s",
		      host, isc_result_totext(result));
	INSIST(nserveraddrs > 0);
}

static void
rndc_senddone(isc_task_t *task, isc_event_t *event) {
	isc_socketevent_t *sevent = (isc_socketevent_t *)event;

	UNUSED(task);

	sends--;
	if (sevent->result != ISC_R_SUCCESS)
		fatal("send failed: %s", isc_result_totext(sevent->result));
	isc_event_free(&event);
	if (sends == 0 && recvs == 0) {
		isc_socket_detach(&sock);
		isc_task_shutdown(task);
		RUNTIME_CHECK(isc_app_shutdown() == ISC_R_SUCCESS);
	}
}

static void
rndc_recvdone(isc_task_t *task, isc_event_t *event) {
	isccc_sexpr_t *response = NULL;
	isccc_sexpr_t *data;
	isccc_region_t source;
	char *errormsg = NULL;
	char *textmsg = NULL;
	isc_result_t result;

	recvs--;

	if (ccmsg.result == ISC_R_EOF)
		fatal("connection to remote host closed\n"
		      "This may indicate that\n"
		      "* the remote server is using an older version of"
		      " the command protocol,\n"
		      "* this host is not authorized to connect,\n"
		      "* the clocks are not syncronized, or\n"
		      "* the key is invalid.");

	if (ccmsg.result != ISC_R_SUCCESS)
		fatal("recv failed: %s", isc_result_totext(ccmsg.result));

	source.rstart = isc_buffer_base(&ccmsg.buffer);
	source.rend = isc_buffer_used(&ccmsg.buffer);

	DO("parse message", isccc_cc_fromwire(&source, &response, &secret));

	data = isccc_alist_lookup(response, "_data");
	if (data == NULL)
		fatal("no data section in response");
	result = isccc_cc_lookupstring(data, "err", &errormsg);
	if (result == ISC_R_SUCCESS) {
		failed = ISC_TRUE;
		fprintf(stderr, "%s: '%s' failed: %s\n",
			progname, command, errormsg);
	}
	else if (result != ISC_R_NOTFOUND)
		fprintf(stderr, "%s: parsing response failed: %s\n",
			progname, isc_result_totext(result));

	result = isccc_cc_lookupstring(data, "text", &textmsg);
	if (result == ISC_R_SUCCESS)
		printf("%s\n", textmsg);
	else if (result != ISC_R_NOTFOUND)
		fprintf(stderr, "%s: parsing response failed: %s\n",
			progname, isc_result_totext(result));

	isc_event_free(&event);
	isccc_sexpr_free(&response);
	if (sends == 0 && recvs == 0) {
		isc_socket_detach(&sock);
		isc_task_shutdown(task);
		RUNTIME_CHECK(isc_app_shutdown() == ISC_R_SUCCESS);
	}
}

static void
rndc_recvnonce(isc_task_t *task, isc_event_t *event) {
	isccc_sexpr_t *response = NULL;
	isccc_sexpr_t *_ctrl;
	isccc_region_t source;
	isc_result_t result;
	isc_uint32_t nonce;
	isccc_sexpr_t *request = NULL;
	isccc_time_t now;
	isc_region_t r;
	isccc_sexpr_t *data;
	isccc_region_t message;
	isc_uint32_t len;
	isc_buffer_t b;

	recvs--;

	if (ccmsg.result == ISC_R_EOF)
		fatal("connection to remote host closed\n"
		      "This may indicate that\n"
		      "* the remote server is using an older version of"
		      " the command protocol,\n"
		      "* this host is not authorized to connect,\n"
		      "* the clocks are not syncronized, or\n"
		      "* the key is invalid.");

	if (ccmsg.result != ISC_R_SUCCESS)
		fatal("recv failed: %s", isc_result_totext(ccmsg.result));

	source.rstart = isc_buffer_base(&ccmsg.buffer);
	source.rend = isc_buffer_used(&ccmsg.buffer);

	DO("parse message", isccc_cc_fromwire(&source, &response, &secret));

	_ctrl = isccc_alist_lookup(response, "_ctrl");
	if (_ctrl == NULL)
		fatal("_ctrl section missing");
	nonce = 0;
	if (isccc_cc_lookupuint32(_ctrl, "_nonce", &nonce) != ISC_R_SUCCESS)
		nonce = 0;

	isc_stdtime_get(&now);

	DO("create message", isccc_cc_createmessage(1, NULL, NULL, ++serial,
						    now, now + 60, &request));
	data = isccc_alist_lookup(request, "_data");
	if (data == NULL)
		fatal("_data section missing");
	if (isccc_cc_definestring(data, "type", args) == NULL)
		fatal("out of memory");
	if (nonce != 0) {
		_ctrl = isccc_alist_lookup(request, "_ctrl");
		if (_ctrl == NULL)
			fatal("_ctrl section missing");
		if (isccc_cc_defineuint32(_ctrl, "_nonce", nonce) == NULL)
			fatal("out of memory");
	}
	message.rstart = databuf + 4;
	message.rend = databuf + sizeof(databuf);
	DO("render message", isccc_cc_towire(request, &message, &secret));
	len = sizeof(databuf) - REGION_SIZE(message);
	isc_buffer_init(&b, databuf, 4);
	isc_buffer_putuint32(&b, len - 4);
	r.length = len;
	r.base = databuf;

	isccc_ccmsg_cancelread(&ccmsg);
	DO("schedule recv", isccc_ccmsg_readmessage(&ccmsg, task,
						    rndc_recvdone, NULL));
	recvs++;
	DO("send message", isc_socket_send(sock, &r, task, rndc_senddone,
					   NULL));
	sends++;

	isc_event_free(&event);
	isccc_sexpr_free(&response);
	return;
}

static void
rndc_connected(isc_task_t *task, isc_event_t *event) {
	char socktext[ISC_SOCKADDR_FORMATSIZE];
	isc_socketevent_t *sevent = (isc_socketevent_t *)event;
	isccc_sexpr_t *request = NULL;
	isccc_sexpr_t *data;
	isccc_time_t now;
	isccc_region_t message;
	isc_region_t r;
	isc_uint32_t len;
	isc_buffer_t b;
	isc_result_t result;

	connects--;

	if (sevent->result != ISC_R_SUCCESS) {
		isc_sockaddr_format(&serveraddrs[currentaddr], socktext,
				    sizeof(socktext));
		if (sevent->result != ISC_R_CANCELED &&
		    ++currentaddr < nserveraddrs)
		{
			notify("connection failed: %s: %s", socktext,
			       isc_result_totext(sevent->result));
			isc_socket_detach(&sock);
			isc_event_free(&event);
			rndc_startconnect(&serveraddrs[currentaddr], task);
			return;
		} else
			fatal("connect failed: %s: %s", socktext,
			      isc_result_totext(sevent->result));
	}

	isc_stdtime_get(&now);
	DO("create message", isccc_cc_createmessage(1, NULL, NULL, ++serial,
						    now, now + 60, &request));
	data = isccc_alist_lookup(request, "_data");
	if (data == NULL)
		fatal("_data section missing");
	if (isccc_cc_definestring(data, "type", "null") == NULL)
		fatal("out of memory");
	message.rstart = databuf + 4;
	message.rend = databuf + sizeof(databuf);
	DO("render message", isccc_cc_towire(request, &message, &secret));
	len = sizeof(databuf) - REGION_SIZE(message);
	isc_buffer_init(&b, databuf, 4);
	isc_buffer_putuint32(&b, len - 4);
	r.length = len;
	r.base = databuf;

	isccc_ccmsg_init(mctx, sock, &ccmsg);
	isccc_ccmsg_setmaxsize(&ccmsg, 1024);

	DO("schedule recv", isccc_ccmsg_readmessage(&ccmsg, task,
						    rndc_recvnonce, NULL));
	recvs++;
	DO("send message", isc_socket_send(sock, &r, task, rndc_senddone,
					   NULL));
	sends++;
	isc_event_free(&event);
}

static void
rndc_startconnect(isc_sockaddr_t *addr, isc_task_t *task) {
	isc_result_t result;
	int pf;
	isc_sockettype_t type;

	char socktext[ISC_SOCKADDR_FORMATSIZE];

	isc_sockaddr_format(addr, socktext, sizeof(socktext));

	notify("using server %s (%s)", servername, socktext);

	pf = isc_sockaddr_pf(addr);
	if (pf == AF_INET || pf == AF_INET6)
		type = isc_sockettype_tcp;
	else
		type = isc_sockettype_unix;
	DO("create socket", isc_socket_create(socketmgr, pf, type, &sock));
	switch (isc_sockaddr_pf(addr)) {
	case AF_INET:
		DO("bind socket", isc_socket_bind(sock, &local4, 0));
		break;
	case AF_INET6:
		DO("bind socket", isc_socket_bind(sock, &local6, 0));
		break;
	default:
		break;
	}
	DO("connect", isc_socket_connect(sock, addr, task, rndc_connected,
					 NULL));
	connects++;
}

static void
rndc_start(isc_task_t *task, isc_event_t *event) {
	isc_event_free(&event);

	currentaddr = 0;
	rndc_startconnect(&serveraddrs[currentaddr], task);
}

static void
parse_config(isc_mem_t *mctx, isc_log_t *log, const char *keyname,
	     cfg_parser_t **pctxp, cfg_obj_t **configp)
{
	isc_result_t result;
	const char *conffile = admin_conffile;
	const cfg_obj_t *addresses = NULL;
	const cfg_obj_t *defkey = NULL;
	const cfg_obj_t *options = NULL;
	const cfg_obj_t *servers = NULL;
	const cfg_obj_t *server = NULL;
	const cfg_obj_t *keys = NULL;
	const cfg_obj_t *key = NULL;
	const cfg_obj_t *defport = NULL;
	const cfg_obj_t *secretobj = NULL;
	const cfg_obj_t *algorithmobj = NULL;
	cfg_obj_t *config = NULL;
	const cfg_obj_t *address = NULL;
	const cfg_listelt_t *elt;
	const char *secretstr;
	const char *algorithm;
	static char secretarray[1024];
	const cfg_type_t *conftype = &cfg_type_rndcconf;
	isc_boolean_t key_only = ISC_FALSE;
	const cfg_listelt_t *element;

	if (! isc_file_exists(conffile)) {
		conffile = admin_keyfile;
		conftype = &cfg_type_rndckey;

		if (! isc_file_exists(conffile))
			fatal("neither %s nor %s was found",
			      admin_conffile, admin_keyfile);
		key_only = ISC_TRUE;
	}

	DO("create parser", cfg_parser_create(mctx, log, pctxp));

	/*
	 * The parser will output its own errors, so DO() is not used.
	 */
	result = cfg_parse_file(*pctxp, conffile, conftype, &config);
	if (result != ISC_R_SUCCESS)
		fatal("could not load rndc configuration");

	if (!key_only)
		(void)cfg_map_get(config, "options", &options);

	if (key_only && servername == NULL)
		servername = "127.0.0.1";
	else if (servername == NULL && options != NULL) {
		const cfg_obj_t *defserverobj = NULL;
		(void)cfg_map_get(options, "default-server", &defserverobj);
		if (defserverobj != NULL)
			servername = cfg_obj_asstring(defserverobj);
	}

	if (servername == NULL)
		fatal("no server specified and no default");

	if (!key_only) {
		(void)cfg_map_get(config, "server", &servers);
		if (servers != NULL) {
			for (elt = cfg_list_first(servers);
			     elt != NULL;
			     elt = cfg_list_next(elt))
			{
				const char *name;
				server = cfg_listelt_value(elt);
				name = cfg_obj_asstring(cfg_map_getname(server));
				if (strcasecmp(name, servername) == 0)
					break;
				server = NULL;
			}
		}
	}

	/*
	 * Look for the name of the key to use.
	 */
	if (keyname != NULL)
		;		/* Was set on command line, do nothing. */
	else if (server != NULL) {
		DO("get key for server", cfg_map_get(server, "key", &defkey));
		keyname = cfg_obj_asstring(defkey);
	} else if (options != NULL) {
		DO("get default key", cfg_map_get(options, "default-key",
						  &defkey));
		keyname = cfg_obj_asstring(defkey);
	} else if (!key_only)
		fatal("no key for server and no default");

	/*
	 * Get the key's definition.
	 */
	if (key_only)
		DO("get key", cfg_map_get(config, "key", &key));
	else {
		DO("get config key list", cfg_map_get(config, "key", &keys));
		for (elt = cfg_list_first(keys);
		     elt != NULL;
		     elt = cfg_list_next(elt))
		{
			key = cfg_listelt_value(elt);
			if (strcasecmp(cfg_obj_asstring(cfg_map_getname(key)),
				       keyname) == 0)
				break;
		}
		if (elt == NULL)
			fatal("no key definition for name %s", keyname);
	}
	(void)cfg_map_get(key, "secret", &secretobj);
	(void)cfg_map_get(key, "algorithm", &algorithmobj);
	if (secretobj == NULL || algorithmobj == NULL)
		fatal("key must have algorithm and secret");

	secretstr = cfg_obj_asstring(secretobj);
	algorithm = cfg_obj_asstring(algorithmobj);

	if (strcasecmp(algorithm, "hmac-md5") != 0)
		fatal("unsupported algorithm: %s", algorithm);

	secret.rstart = (unsigned char *)secretarray;
	secret.rend = (unsigned char *)secretarray + sizeof(secretarray);
	DO("decode base64 secret", isccc_base64_decode(secretstr, &secret));
	secret.rend = secret.rstart;
	secret.rstart = (unsigned char *)secretarray;

	/*
	 * Find the port to connect to.
	 */
	if (remoteport != 0)
		;		/* Was set on command line, do nothing. */
	else {
		if (server != NULL)
			(void)cfg_map_get(server, "port", &defport);
		if (defport == NULL && options != NULL)
			(void)cfg_map_get(options, "default-port", &defport);
	}
	if (defport != NULL) {
		remoteport = cfg_obj_asuint32(defport);
		if (remoteport > 65535 || remoteport == 0)
			fatal("port %u out of range", remoteport);
	} else if (remoteport == 0)
		remoteport = NS_CONTROL_PORT;

	if (server != NULL)
		result = cfg_map_get(server, "addresses", &addresses);
	else
		result = ISC_R_NOTFOUND;
	if (result == ISC_R_SUCCESS) {
		for (element = cfg_list_first(addresses);
		     element != NULL;
		     element = cfg_list_next(element))
		{
			isc_sockaddr_t sa;

			address = cfg_listelt_value(element);
			if (!cfg_obj_issockaddr(address)) {
				unsigned int myport;
				const char *name;
				const cfg_obj_t *obj;

				obj = cfg_tuple_get(address, "name");
				name = cfg_obj_asstring(obj);
				obj = cfg_tuple_get(address, "port");
				if (cfg_obj_isuint32(obj)) {
					myport = cfg_obj_asuint32(obj);
					if (myport > ISC_UINT16_MAX ||
					    myport == 0)
						fatal("port %u out of range",
						      myport);
				} else
					myport = remoteport;
				if (nserveraddrs < SERVERADDRS)
					get_addresses(name, (in_port_t) myport);
				else
					fprintf(stderr, "too many address: "
						"%s: dropped\n", name);
				continue;
			}
			sa = *cfg_obj_assockaddr(address);
			if (isc_sockaddr_getport(&sa) == 0)
				isc_sockaddr_setport(&sa, remoteport);
			if (nserveraddrs < SERVERADDRS)
				serveraddrs[nserveraddrs++] = sa;
			else {
				char socktext[ISC_SOCKADDR_FORMATSIZE];

				isc_sockaddr_format(&sa, socktext,
						    sizeof(socktext));
				fprintf(stderr,
					"too many address: %s: dropped\n",
					socktext);
			}
		}
	}

	if (!local4set && server != NULL) {
		address = NULL;
		cfg_map_get(server, "source-address", &address);
		if (address != NULL) {
			local4 = *cfg_obj_assockaddr(address);
			local4set = ISC_TRUE;
		}
	}
	if (!local4set && options != NULL) {
		address = NULL;
		cfg_map_get(options, "default-source-address", &address);
		if (address != NULL) {
			local4 = *cfg_obj_assockaddr(address);
			local4set = ISC_TRUE;
		}
	}

	if (!local6set && server != NULL) {
		address = NULL;
		cfg_map_get(server, "source-address-v6", &address);
		if (address != NULL) {
			local6 = *cfg_obj_assockaddr(address);
			local6set = ISC_TRUE;
		}
	}
	if (!local6set && options != NULL) {
		address = NULL;
		cfg_map_get(options, "default-source-address-v6", &address);
		if (address != NULL) {
			local6 = *cfg_obj_assockaddr(address);
			local6set = ISC_TRUE;
		}
	}

	*configp = config;
}

int
main(int argc, char **argv) {
	isc_boolean_t show_final_mem = ISC_FALSE;
	isc_result_t result = ISC_R_SUCCESS;
	isc_taskmgr_t *taskmgr = NULL;
	isc_task_t *task = NULL;
	isc_log_t *log = NULL;
	isc_logconfig_t *logconfig = NULL;
	isc_logdestination_t logdest;
	cfg_parser_t *pctx = NULL;
	cfg_obj_t *config = NULL;
	const char *keyname = NULL;
	struct in_addr in;
	struct in6_addr in6;
	char *p;
	size_t argslen;
	int ch;
	int i;

	result = isc_file_progname(*argv, program, sizeof(program));
	if (result != ISC_R_SUCCESS)
		memcpy(program, "rndc", 5);
	progname = program;

	admin_conffile = RNDC_CONFFILE;
	admin_keyfile = RNDC_KEYFILE;

	isc_sockaddr_any(&local4);
	isc_sockaddr_any6(&local6);

	result = isc_app_start();
	if (result != ISC_R_SUCCESS)
		fatal("isc_app_start() failed: %s", isc_result_totext(result));

	while ((ch = isc_commandline_parse(argc, argv, "b:c:k:Mmp:s:Vy:"))
	       != -1) {
		switch (ch) {
		case 'b':
			if (inet_pton(AF_INET, isc_commandline_argument,
				      &in) == 1) {
				isc_sockaddr_fromin(&local4, &in, 0);
				local4set = ISC_TRUE;
			} else if (inet_pton(AF_INET6, isc_commandline_argument,
					     &in6) == 1) {
				isc_sockaddr_fromin6(&local6, &in6, 0);
				local6set = ISC_TRUE;
			}
			break;

		case 'c':
			admin_conffile = isc_commandline_argument;
			break;

		case 'k':
			admin_keyfile = isc_commandline_argument;
			break;

		case 'M':
			isc_mem_debugging = ISC_MEM_DEBUGTRACE;
			break;

		case 'm':
			show_final_mem = ISC_TRUE;
			break;

		case 'p':
			remoteport = atoi(isc_commandline_argument);
			if (remoteport > 65535 || remoteport == 0)
				fatal("port '%s' out of range",
				      isc_commandline_argument);
			break;

		case 's':
			servername = isc_commandline_argument;
			break;

		case 'V':
			verbose = ISC_TRUE;
			break;

		case 'y':
			keyname = isc_commandline_argument;
			break;

		case '?':
			usage(0);
			break;

		default:
			fatal("unexpected error parsing command arguments: "
			      "got %c\n", ch);
			break;
		}
	}

	argc -= isc_commandline_index;
	argv += isc_commandline_index;

	if (argc < 1)
		usage(1);

	isc_random_get(&serial);

	DO("create memory context", isc_mem_create(0, 0, &mctx));
	DO("create socket manager", isc_socketmgr_create(mctx, &socketmgr));
	DO("create task manager", isc_taskmgr_create(mctx, 1, 0, &taskmgr));
	DO("create task", isc_task_create(taskmgr, 0, &task));

	DO("create logging context", isc_log_create(mctx, &log, &logconfig));
	isc_log_setcontext(log);
	DO("setting log tag", isc_log_settag(logconfig, progname));
	logdest.file.stream = stderr;
	logdest.file.name = NULL;
	logdest.file.versions = ISC_LOG_ROLLNEVER;
	logdest.file.maximum_size = 0;
	DO("creating log channel",
	   isc_log_createchannel(logconfig, "stderr",
				 ISC_LOG_TOFILEDESC, ISC_LOG_INFO, &logdest,
				 ISC_LOG_PRINTTAG|ISC_LOG_PRINTLEVEL));
	DO("enabling log channel", isc_log_usechannel(logconfig, "stderr",
						      NULL, NULL));

	parse_config(mctx, log, keyname, &pctx, &config);

	isccc_result_register();

	command = *argv;

	/*
	 * Convert argc/argv into a space-delimited command string
	 * similar to what the user might enter in interactive mode
	 * (if that were implemented).
	 */
	argslen = 0;
	for (i = 0; i < argc; i++)
		argslen += strlen(argv[i]) + 1;

	args = isc_mem_get(mctx, argslen);
	if (args == NULL)
		DO("isc_mem_get", ISC_R_NOMEMORY);

	p = args;
	for (i = 0; i < argc; i++) {
		size_t len = strlen(argv[i]);
		memcpy(p, argv[i], len);
		p += len;
		*p++ = ' ';
	}

	p--;
	*p++ = '\0';
	INSIST(p == args + argslen);

	notify("%s", command);

	if (strcmp(command, "restart") == 0)
		fatal("'%s' is not implemented", command);

	if (nserveraddrs == 0)
		get_addresses(servername, (in_port_t) remoteport);

	DO("post event", isc_app_onrun(mctx, task, rndc_start, NULL));

	result = isc_app_run();
	if (result != ISC_R_SUCCESS)
		fatal("isc_app_run() failed: %s", isc_result_totext(result));

	if (connects > 0 || sends > 0 || recvs > 0)
		isc_socket_cancel(sock, task, ISC_SOCKCANCEL_ALL);

	isc_task_detach(&task);
	isc_taskmgr_destroy(&taskmgr);
	isc_socketmgr_destroy(&socketmgr);
	isc_log_destroy(&log);
	isc_log_setcontext(NULL);

	cfg_obj_destroy(pctx, &config);
	cfg_parser_destroy(&pctx);

	isc_mem_put(mctx, args, argslen);
	isccc_ccmsg_invalidate(&ccmsg);

	dns_name_destroy();

	if (show_final_mem)
		isc_mem_stats(mctx, stderr);

	isc_mem_destroy(&mctx);

	if (failed)
		return (1);

	return (0);
}
