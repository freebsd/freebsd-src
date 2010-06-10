%{
/*-
 * Copyright (c) 2009-2010 The FreeBSD Foundation
 * All rights reserved.
 *
 * This software was developed by Pawel Jakub Dawidek under sponsorship from
 * the FreeBSD Foundation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <sys/param.h>	/* MAXHOSTNAMELEN */
#include <sys/queue.h>
#include <sys/sysctl.h>

#include <arpa/inet.h>

#include <assert.h>
#include <err.h>
#include <stdio.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#include "hast.h"

extern int depth;
extern int lineno;

extern FILE *yyin;
extern char *yytext;

static struct hastd_config lconfig;
static struct hast_resource *curres;
static bool mynode;

static char depth0_control[HAST_ADDRSIZE];
static char depth0_listen[HAST_ADDRSIZE];
static int depth0_replication;
static int depth0_timeout;

static char depth1_provname[PATH_MAX];
static char depth1_localpath[PATH_MAX];

static bool
isitme(const char *name)
{
	char buf[MAXHOSTNAMELEN];
	char *pos;
	size_t bufsize;

	/*
	 * First check if the give name matches our full hostname.
	 */
	if (gethostname(buf, sizeof(buf)) < 0)
		err(EX_OSERR, "gethostname() failed");
	if (strcmp(buf, name) == 0)
		return (true);

	/*
	 * Now check if it matches first part of the host name.
	 */
	pos = strchr(buf, '.');
	if (pos != NULL && pos != buf && strncmp(buf, name, pos - buf) == 0)
		return (true);

	/*
	 * At the end check if name is equal to our host's UUID.
	 */
	bufsize = sizeof(buf);
	if (sysctlbyname("kern.hostuuid", buf, &bufsize, NULL, 0) < 0)
		err(EX_OSERR, "sysctlbyname(kern.hostuuid) failed");
	if (strcasecmp(buf, name) == 0)
		return (true);

	/*
	 * Looks like this isn't about us.
	 */
	return (false);
}

void
yyerror(const char *str)
{

	fprintf(stderr, "error at line %d near '%s': %s\n",
	    lineno, yytext, str);
}

struct hastd_config *
yy_config_parse(const char *config)
{
	int ret;

	curres = NULL;
	mynode = false;

	depth0_timeout = HAST_TIMEOUT;
	depth0_replication = HAST_REPLICATION_MEMSYNC;
	strlcpy(depth0_control, HAST_CONTROL, sizeof(depth0_control));
	strlcpy(depth0_listen, HASTD_LISTEN, sizeof(depth0_listen));

	TAILQ_INIT(&lconfig.hc_resources);

	yyin = fopen(config, "r");
	if (yyin == NULL)
		err(EX_OSFILE, "cannot open configuration file %s", config);
	ret = yyparse();
	fclose(yyin);
	if (ret != 0) {
		yy_config_free(&lconfig);
		exit(EX_CONFIG);
	}

	/*
	 * Let's see if everything is set up.
	 */
	if (lconfig.hc_controladdr[0] == '\0') {
		strlcpy(lconfig.hc_controladdr, depth0_control,
		    sizeof(lconfig.hc_controladdr));
	}
	if (lconfig.hc_listenaddr[0] == '\0') {
		strlcpy(lconfig.hc_listenaddr, depth0_listen,
		    sizeof(lconfig.hc_listenaddr));
	}
	TAILQ_FOREACH(curres, &lconfig.hc_resources, hr_next) {
		assert(curres->hr_provname[0] != '\0');
		assert(curres->hr_localpath[0] != '\0');
		assert(curres->hr_remoteaddr[0] != '\0');

		if (curres->hr_replication == -1) {
			/*
			 * Replication is not set at resource-level.
			 * Use global or default setting.
			 */
			curres->hr_replication = depth0_replication;
		}
		if (curres->hr_timeout == -1) {
			/*
			 * Timeout is not set at resource-level.
			 * Use global or default setting.
			 */
			curres->hr_timeout = depth0_timeout;
		}
	}

	return (&lconfig);
}

void
yy_config_free(struct hastd_config *config)
{
	struct hast_resource *res;

	while ((res = TAILQ_FIRST(&config->hc_resources)) != NULL) {
		TAILQ_REMOVE(&config->hc_resources, res, hr_next);
		free(res);
	}
}
%}

%token CONTROL LISTEN PORT REPLICATION TIMEOUT EXTENTSIZE RESOURCE NAME LOCAL REMOTE ON
%token FULLSYNC MEMSYNC ASYNC
%token NUM STR OB CB

%type <num> replication_type

%union
{
	int num;
	char *str;
}

%token <num> NUM
%token <str> STR

%%

statements:
	|
	statements statement
	;

statement:
	control_statement
	|
	listen_statement
	|
	replication_statement
	|
	timeout_statement
	|
	node_statement
	|
	resource_statement
	;

control_statement:	CONTROL STR
	{
		switch (depth) {
		case 0:
			if (strlcpy(depth0_control, $2,
			    sizeof(depth0_control)) >=
			    sizeof(depth0_control)) {
				errx(EX_CONFIG, "control argument too long");
			}
			break;
		case 1:
			if (mynode) {
				if (strlcpy(lconfig.hc_controladdr, $2,
				    sizeof(lconfig.hc_controladdr)) >=
				    sizeof(lconfig.hc_controladdr)) {
					errx(EX_CONFIG,
					    "control argument too long");
				}
			}
			break;
		default:
			assert(!"control at wrong depth level");
		}
	}
	;

listen_statement:	LISTEN STR
	{
		switch (depth) {
		case 0:
			if (strlcpy(depth0_listen, $2,
			    sizeof(depth0_listen)) >=
			    sizeof(depth0_listen)) {
				errx(EX_CONFIG, "listen argument too long");
			}
			break;
		case 1:
			if (mynode) {
				if (strlcpy(lconfig.hc_listenaddr, $2,
				    sizeof(lconfig.hc_listenaddr)) >=
				    sizeof(lconfig.hc_listenaddr)) {
					errx(EX_CONFIG,
					    "listen argument too long");
				}
			}
			break;
		default:
			assert(!"listen at wrong depth level");
		}
	}
	;

replication_statement:	REPLICATION replication_type
	{
		switch (depth) {
		case 0:
			depth0_replication = $2;
			break;
		case 1:
			if (curres != NULL)
				curres->hr_replication = $2;
			break;
		default:
			assert(!"replication at wrong depth level");
		}
	}
	;

replication_type:
	FULLSYNC	{ $$ = HAST_REPLICATION_FULLSYNC; }
	|
	MEMSYNC		{ $$ = HAST_REPLICATION_MEMSYNC; }
	|
	ASYNC		{ $$ = HAST_REPLICATION_ASYNC; }
	;

timeout_statement:	TIMEOUT NUM
	{
		switch (depth) {
		case 0:
			depth0_timeout = $2;
			break;
		case 1:
			if (curres != NULL)
				curres->hr_timeout = $2;
			break;
		default:
			assert(!"timeout at wrong depth level");
		}
	}
	;

node_statement:		ON node_start OB node_entries CB
	{
		mynode = false;
	}
	;

node_start:	STR
	{
		if (isitme($1))
			mynode = true;
	}
	;

node_entries:
	|
	node_entries node_entry
	;

node_entry:
	control_statement
	|
	listen_statement
	;

resource_statement:	RESOURCE resource_start OB resource_entries CB
	{
		if (curres != NULL) {
			/*
			 * Let's see there are some resource-level settings
			 * that we can use for node-level settings.
			 */
			if (curres->hr_provname[0] == '\0' &&
			    depth1_provname[0] != '\0') {
				/*
				 * Provider name is not set at node-level,
				 * but is set at resource-level, use it.
				 */
				strlcpy(curres->hr_provname, depth1_provname,
				    sizeof(curres->hr_provname));
			}
			if (curres->hr_localpath[0] == '\0' &&
			    depth1_localpath[0] != '\0') {
				/*
				 * Path to local provider is not set at
				 * node-level, but is set at resource-level,
				 * use it.
				 */
				strlcpy(curres->hr_localpath, depth1_localpath,
				    sizeof(curres->hr_localpath));
			}

			/*
			 * If provider name is not given, use resource name
			 * as provider name.
			 */
			if (curres->hr_provname[0] == '\0') {
				strlcpy(curres->hr_provname, curres->hr_name,
				    sizeof(curres->hr_provname));
			}

			/*
			 * Remote address has to be configured at this point.
			 */
			if (curres->hr_remoteaddr[0] == '\0') {
				errx(EX_CONFIG,
				    "remote address not configured for resource %s",
				    curres->hr_name);
			}
			/*
			 * Path to local provider has to be configured at this
			 * point.
			 */
			if (curres->hr_localpath[0] == '\0') {
				errx(EX_CONFIG,
				    "path local component not configured for resource %s",
				    curres->hr_name);
			}

			/* Put it onto resource list. */
			TAILQ_INSERT_TAIL(&lconfig.hc_resources, curres, hr_next);
			curres = NULL;
		}
	}
	;

resource_start:	STR
	{
		/*
		 * Clear those, so we can tell if they were set at
		 * resource-level or not.
		 */
		depth1_provname[0] = '\0';
		depth1_localpath[0] = '\0';

		curres = calloc(1, sizeof(*curres));
		if (curres == NULL) {
			errx(EX_TEMPFAIL,
			    "cannot allocate memory for resource");
		}
		if (strlcpy(curres->hr_name, $1,
		    sizeof(curres->hr_name)) >=
		    sizeof(curres->hr_name)) {
			errx(EX_CONFIG,
			    "resource name (%s) too long", $1);
		}
		curres->hr_role = HAST_ROLE_INIT;
		curres->hr_previous_role = HAST_ROLE_INIT;
		curres->hr_replication = -1;
		curres->hr_timeout = -1;
		curres->hr_provname[0] = '\0';
		curres->hr_localpath[0] = '\0';
		curres->hr_localfd = -1;
		curres->hr_remoteaddr[0] = '\0';
		curres->hr_ggateunit = -1;
	}
	;

resource_entries:
	|
	resource_entries resource_entry
	;

resource_entry:
	replication_statement
	|
	timeout_statement
	|
	name_statement
	|
	local_statement
	|
	resource_node_statement
	;

name_statement:		NAME STR
	{
		switch (depth) {
		case 1:
			if (strlcpy(depth1_provname, $2,
			    sizeof(depth1_provname)) >=
			    sizeof(depth1_provname)) {
				errx(EX_CONFIG, "name argument too long");
			}
			break;
		case 2:
			if (mynode) {
				assert(curres != NULL);
				if (strlcpy(curres->hr_provname, $2,
				    sizeof(curres->hr_provname)) >=
				    sizeof(curres->hr_provname)) {
					errx(EX_CONFIG,
					    "name argument too long");
				}
			}
			break;
		default:
			assert(!"name at wrong depth level");
		}
	}
	;

local_statement:	LOCAL STR
	{
		switch (depth) {
		case 1:
			if (strlcpy(depth1_localpath, $2,
			    sizeof(depth1_localpath)) >=
			    sizeof(depth1_localpath)) {
				errx(EX_CONFIG, "local argument too long");
			}
			break;
		case 2:
			if (mynode) {
				assert(curres != NULL);
				if (strlcpy(curres->hr_localpath, $2,
				    sizeof(curres->hr_localpath)) >=
				    sizeof(curres->hr_localpath)) {
					errx(EX_CONFIG,
					    "local argument too long");
				}
			}
			break;
		default:
			assert(!"local at wrong depth level");
		}
	}
	;

resource_node_statement:ON resource_node_start OB resource_node_entries CB
	{
		mynode = false;
	}
	;

resource_node_start:	STR
	{
		if (curres != NULL && isitme($1))
			mynode = true;
	}
	;

resource_node_entries:
	|
	resource_node_entries resource_node_entry
	;

resource_node_entry:
	name_statement
	|
	local_statement
	|
	remote_statement
	;

remote_statement:	REMOTE STR
	{
		assert(depth == 2);
		if (mynode) {
			assert(curres != NULL);
			if (strlcpy(curres->hr_remoteaddr, $2,
			    sizeof(curres->hr_remoteaddr)) >=
			    sizeof(curres->hr_remoteaddr)) {
				errx(EX_CONFIG, "remote argument too long");
			}
		}
	}
	;
