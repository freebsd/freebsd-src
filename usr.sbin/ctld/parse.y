%{
/*-
 * Copyright (c) 2012 The FreeBSD Foundation
 * All rights reserved.
 *
 * This software was developed by Edward Tomasz Napierala under sponsorship
 * from the FreeBSD Foundation.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
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

#include <sys/queue.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "ctld.h"

extern FILE *yyin;
extern char *yytext;
extern int lineno;

static struct conf *conf = NULL;
static struct auth_group *auth_group = NULL;
static struct portal_group *portal_group = NULL;
static struct target *target = NULL;
static struct lun *lun = NULL;

extern void	yyerror(const char *);
extern int	yylex(void);
extern void	yyrestart(FILE *);

%}

%token ALIAS AUTH_GROUP AUTH_TYPE BACKEND BLOCKSIZE CHAP CHAP_MUTUAL
%token CLOSING_BRACKET DEBUG DEVICE_ID DISCOVERY_AUTH_GROUP INITIATOR_NAME
%token INITIATOR_PORTAL LISTEN LISTEN_ISER LUN MAXPROC NUM OPENING_BRACKET
%token OPTION PATH PIDFILE PORTAL_GROUP SERIAL SIZE STR TARGET TIMEOUT

%union
{
	uint64_t num;
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
	debug
	|
	timeout
	|
	maxproc
	|
	pidfile
	|
	auth_group
	|
	portal_group
	|
	target
	;

debug:		DEBUG NUM
	{
		conf->conf_debug = $2;
	}
	;

timeout:	TIMEOUT NUM
	{
		conf->conf_timeout = $2;
	}
	;

maxproc:	MAXPROC NUM
	{
		conf->conf_maxproc = $2;
	}
	;

pidfile:	PIDFILE STR
	{
		if (conf->conf_pidfile_path != NULL) {
			log_warnx("pidfile specified more than once");
			free($2);
			return (1);
		}
		conf->conf_pidfile_path = $2;
	}
	;

auth_group:	AUTH_GROUP auth_group_name
    OPENING_BRACKET auth_group_entries CLOSING_BRACKET
	{
		auth_group = NULL;
	}
	;

auth_group_name:	STR
	{
		/*
		 * Make it possible to redefine default
		 * auth-group. but only once.
		 */
		if (strcmp($1, "default") == 0 &&
		    conf->conf_default_ag_defined == false) {
			auth_group = auth_group_find(conf, $1);
			conf->conf_default_ag_defined = true;
		} else {
			auth_group = auth_group_new(conf, $1);
		}
		free($1);
		if (auth_group == NULL)
			return (1);
	}
	;

auth_group_entries:
	|
	auth_group_entries auth_group_entry
	;

auth_group_entry:
	auth_group_auth_type
	|
	auth_group_chap
	|
	auth_group_chap_mutual
	|
	auth_group_initiator_name
	|
	auth_group_initiator_portal
	;

auth_group_auth_type:	AUTH_TYPE STR
	{
		int error;

		error = auth_group_set_type_str(auth_group, $2);
		free($2);
		if (error != 0)
			return (1);
	}
	;

auth_group_chap:	CHAP STR STR
	{
		const struct auth *ca;

		ca = auth_new_chap(auth_group, $2, $3);
		free($2);
		free($3);
		if (ca == NULL)
			return (1);
	}
	;

auth_group_chap_mutual:	CHAP_MUTUAL STR STR STR STR
	{
		const struct auth *ca;

		ca = auth_new_chap_mutual(auth_group, $2, $3, $4, $5);
		free($2);
		free($3);
		free($4);
		free($5);
		if (ca == NULL)
			return (1);
	}
	;

auth_group_initiator_name:	INITIATOR_NAME STR
	{
		const struct auth_name *an;

		an = auth_name_new(auth_group, $2);
		free($2);
		if (an == NULL)
			return (1);
	}
	;

auth_group_initiator_portal:	INITIATOR_PORTAL STR
	{
		const struct auth_portal *ap;

		ap = auth_portal_new(auth_group, $2);
		free($2);
		if (ap == NULL)
			return (1);
	}
	;

portal_group:	PORTAL_GROUP portal_group_name
    OPENING_BRACKET portal_group_entries CLOSING_BRACKET
	{
		portal_group = NULL;
	}
	;

portal_group_name:	STR
	{
		/*
		 * Make it possible to redefine default
		 * portal-group. but only once.
		 */
		if (strcmp($1, "default") == 0 &&
		    conf->conf_default_pg_defined == false) {
			portal_group = portal_group_find(conf, $1);
			conf->conf_default_pg_defined = true;
		} else {
			portal_group = portal_group_new(conf, $1);
		}
		free($1);
		if (portal_group == NULL)
			return (1);
	}
	;

portal_group_entries:
	|
	portal_group_entries portal_group_entry
	;

portal_group_entry:
	portal_group_discovery_auth_group
	|
	portal_group_listen
	|
	portal_group_listen_iser
	;

portal_group_discovery_auth_group:	DISCOVERY_AUTH_GROUP STR
	{
		if (portal_group->pg_discovery_auth_group != NULL) {
			log_warnx("discovery-auth-group for portal-group "
			    "\"%s\" specified more than once",
			    portal_group->pg_name);
			return (1);
		}
		portal_group->pg_discovery_auth_group =
		    auth_group_find(conf, $2);
		if (portal_group->pg_discovery_auth_group == NULL) {
			log_warnx("unknown discovery-auth-group \"%s\" "
			    "for portal-group \"%s\"",
			    $2, portal_group->pg_name);
			return (1);
		}
		free($2);
	}
	;

portal_group_listen:	LISTEN STR
	{
		int error;

		error = portal_group_add_listen(portal_group, $2, false);
		free($2);
		if (error != 0)
			return (1);
	}
	;

portal_group_listen_iser:	LISTEN_ISER STR
	{
		int error;

		error = portal_group_add_listen(portal_group, $2, true);
		free($2);
		if (error != 0)
			return (1);
	}
	;

target:	TARGET target_name
    OPENING_BRACKET target_entries CLOSING_BRACKET
	{
		target = NULL;
	}
	;

target_name:	STR
	{
		target = target_new(conf, $1);
		free($1);
		if (target == NULL)
			return (1);
	}
	;

target_entries:
	|
	target_entries target_entry
	;

target_entry:
	target_alias
	|
	target_auth_group
	|
	target_auth_type
	|
	target_chap
	|
	target_chap_mutual
	|
	target_initiator_name
	|
	target_initiator_portal
	|
	target_portal_group
	|
	target_lun
	;

target_alias:	ALIAS STR
	{
		if (target->t_alias != NULL) {
			log_warnx("alias for target \"%s\" "
			    "specified more than once", target->t_name);
			return (1);
		}
		target->t_alias = $2;
	}
	;

target_auth_group:	AUTH_GROUP STR
	{
		if (target->t_auth_group != NULL) {
			if (target->t_auth_group->ag_name != NULL)
				log_warnx("auth-group for target \"%s\" "
				    "specified more than once", target->t_name);
			else
				log_warnx("cannot use both auth-group and explicit "
				    "authorisations for target \"%s\"",
				    target->t_name);
			return (1);
		}
		target->t_auth_group = auth_group_find(conf, $2);
		if (target->t_auth_group == NULL) {
			log_warnx("unknown auth-group \"%s\" for target "
			    "\"%s\"", $2, target->t_name);
			return (1);
		}
		free($2);
	}
	;

target_auth_type:	AUTH_TYPE STR
	{
		int error;

		if (target->t_auth_group != NULL) {
			if (target->t_auth_group->ag_name != NULL) {
				log_warnx("cannot use both auth-group and "
				    "auth-type for target \"%s\"",
				    target->t_name);
				return (1);
			}
		} else {
			target->t_auth_group = auth_group_new(conf, NULL);
			if (target->t_auth_group == NULL) {
				free($2);
				return (1);
			}
			target->t_auth_group->ag_target = target;
		}
		error = auth_group_set_type_str(target->t_auth_group, $2);
		free($2);
		if (error != 0)
			return (1);
	}
	;

target_chap:	CHAP STR STR
	{
		const struct auth *ca;

		if (target->t_auth_group != NULL) {
			if (target->t_auth_group->ag_name != NULL) {
				log_warnx("cannot use both auth-group and "
				    "chap for target \"%s\"",
				    target->t_name);
				free($2);
				free($3);
				return (1);
			}
		} else {
			target->t_auth_group = auth_group_new(conf, NULL);
			if (target->t_auth_group == NULL) {
				free($2);
				free($3);
				return (1);
			}
			target->t_auth_group->ag_target = target;
		}
		ca = auth_new_chap(target->t_auth_group, $2, $3);
		free($2);
		free($3);
		if (ca == NULL)
			return (1);
	}
	;

target_chap_mutual:	CHAP_MUTUAL STR STR STR STR
	{
		const struct auth *ca;

		if (target->t_auth_group != NULL) {
			if (target->t_auth_group->ag_name != NULL) {
				log_warnx("cannot use both auth-group and "
				    "chap-mutual for target \"%s\"",
				    target->t_name);
				free($2);
				free($3);
				free($4);
				free($5);
				return (1);
			}
		} else {
			target->t_auth_group = auth_group_new(conf, NULL);
			if (target->t_auth_group == NULL) {
				free($2);
				free($3);
				free($4);
				free($5);
				return (1);
			}
			target->t_auth_group->ag_target = target;
		}
		ca = auth_new_chap_mutual(target->t_auth_group,
		    $2, $3, $4, $5);
		free($2);
		free($3);
		free($4);
		free($5);
		if (ca == NULL)
			return (1);
	}
	;

target_initiator_name:	INITIATOR_NAME STR
	{
		const struct auth_name *an;

		if (target->t_auth_group != NULL) {
			if (target->t_auth_group->ag_name != NULL) {
				log_warnx("cannot use both auth-group and "
				    "initiator-name for target \"%s\"",
				    target->t_name);
				free($2);
				return (1);
			}
		} else {
			target->t_auth_group = auth_group_new(conf, NULL);
			if (target->t_auth_group == NULL) {
				free($2);
				return (1);
			}
			target->t_auth_group->ag_target = target;
		}
		an = auth_name_new(target->t_auth_group, $2);
		free($2);
		if (an == NULL)
			return (1);
	}
	;

target_initiator_portal:	INITIATOR_PORTAL STR
	{
		const struct auth_portal *ap;

		if (target->t_auth_group != NULL) {
			if (target->t_auth_group->ag_name != NULL) {
				log_warnx("cannot use both auth-group and "
				    "initiator-portal for target \"%s\"",
				    target->t_name);
				free($2);
				return (1);
			}
		} else {
			target->t_auth_group = auth_group_new(conf, NULL);
			if (target->t_auth_group == NULL) {
				free($2);
				return (1);
			}
			target->t_auth_group->ag_target = target;
		}
		ap = auth_portal_new(target->t_auth_group, $2);
		free($2);
		if (ap == NULL)
			return (1);
	}
	;

target_portal_group:	PORTAL_GROUP STR
	{
		if (target->t_portal_group != NULL) {
			log_warnx("portal-group for target \"%s\" "
			    "specified more than once", target->t_name);
			free($2);
			return (1);
		}
		target->t_portal_group = portal_group_find(conf, $2);
		if (target->t_portal_group == NULL) {
			log_warnx("unknown portal-group \"%s\" for target "
			    "\"%s\"", $2, target->t_name);
			free($2);
			return (1);
		}
		free($2);
	}
	;

target_lun:	LUN lun_number
    OPENING_BRACKET lun_entries CLOSING_BRACKET
	{
		lun = NULL;
	}
	;

lun_number:	NUM
	{
		lun = lun_new(target, $1);
		if (lun == NULL)
			return (1);
	}
	;

lun_entries:
	|
	lun_entries lun_entry
	;

lun_entry:
	lun_backend
	|
	lun_blocksize
	|
	lun_device_id
	|
	lun_option
	|
	lun_path
	|
	lun_serial
	|
	lun_size
	;

lun_backend:	BACKEND STR
	{
		if (lun->l_backend != NULL) {
			log_warnx("backend for lun %d, target \"%s\" "
			    "specified more than once",
			    lun->l_lun, target->t_name);
			free($2);
			return (1);
		}
		lun_set_backend(lun, $2);
		free($2);
	}
	;

lun_blocksize:	BLOCKSIZE NUM
	{
		if (lun->l_blocksize != 0) {
			log_warnx("blocksize for lun %d, target \"%s\" "
			    "specified more than once",
			    lun->l_lun, target->t_name);
			return (1);
		}
		lun_set_blocksize(lun, $2);
	}
	;

lun_device_id:	DEVICE_ID STR
	{
		if (lun->l_device_id != NULL) {
			log_warnx("device_id for lun %d, target \"%s\" "
			    "specified more than once",
			    lun->l_lun, target->t_name);
			free($2);
			return (1);
		}
		lun_set_device_id(lun, $2);
		free($2);
	}
	;

lun_option:	OPTION STR STR
	{
		struct lun_option *clo;
		
		clo = lun_option_new(lun, $2, $3);
		free($2);
		free($3);
		if (clo == NULL)
			return (1);
	}
	;

lun_path:	PATH STR
	{
		if (lun->l_path != NULL) {
			log_warnx("path for lun %d, target \"%s\" "
			    "specified more than once",
			    lun->l_lun, target->t_name);
			free($2);
			return (1);
		}
		lun_set_path(lun, $2);
		free($2);
	}
	;

lun_serial:	SERIAL STR
	{
		if (lun->l_serial != NULL) {
			log_warnx("serial for lun %d, target \"%s\" "
			    "specified more than once",
			    lun->l_lun, target->t_name);
			free($2);
			return (1);
		}
		lun_set_serial(lun, $2);
		free($2);
	} |	SERIAL NUM
	{
		char *str = NULL;

		if (lun->l_serial != NULL) {
			log_warnx("serial for lun %d, target \"%s\" "
			    "specified more than once",
			    lun->l_lun, target->t_name);
			return (1);
		}
		asprintf(&str, "%ju", $2);
		lun_set_serial(lun, str);
		free(str);
	}
	;

lun_size:	SIZE NUM
	{
		if (lun->l_size != 0) {
			log_warnx("size for lun %d, target \"%s\" "
			    "specified more than once",
			    lun->l_lun, target->t_name);
			return (1);
		}
		lun_set_size(lun, $2);
	}
	;
%%

void
yyerror(const char *str)
{

	log_warnx("error in configuration file at line %d near '%s': %s",
	    lineno, yytext, str);
}

static void
check_perms(const char *path)
{
	struct stat sb;
	int error;

	error = stat(path, &sb);
	if (error != 0) {
		log_warn("stat");
		return;
	}
	if (sb.st_mode & S_IWOTH) {
		log_warnx("%s is world-writable", path);
	} else if (sb.st_mode & S_IROTH) {
		log_warnx("%s is world-readable", path);
	} else if (sb.st_mode & S_IXOTH) {
		/*
		 * Ok, this one doesn't matter, but still do it,
		 * just for consistency.
		 */
		log_warnx("%s is world-executable", path);
	}

	/*
	 * XXX: Should we also check for owner != 0?
	 */
}

struct conf *
conf_new_from_file(const char *path)
{
	struct auth_group *ag;
	struct portal_group *pg;
	int error;

	log_debugx("obtaining configuration from %s", path);

	conf = conf_new();

	ag = auth_group_new(conf, "default");
	assert(ag != NULL);

	ag = auth_group_new(conf, "no-authentication");
	assert(ag != NULL);
	ag->ag_type = AG_TYPE_NO_AUTHENTICATION;

	ag = auth_group_new(conf, "no-access");
	assert(ag != NULL);
	ag->ag_type = AG_TYPE_DENY;

	pg = portal_group_new(conf, "default");
	assert(pg != NULL);

	yyin = fopen(path, "r");
	if (yyin == NULL) {
		log_warn("unable to open configuration file %s", path);
		conf_delete(conf);
		return (NULL);
	}
	check_perms(path);
	lineno = 1;
	yyrestart(yyin);
	error = yyparse();
	auth_group = NULL;
	portal_group = NULL;
	target = NULL;
	lun = NULL;
	fclose(yyin);
	if (error != 0) {
		conf_delete(conf);
		return (NULL);
	}

	if (conf->conf_default_ag_defined == false) {
		log_debugx("auth-group \"default\" not defined; "
		    "going with defaults");
		ag = auth_group_find(conf, "default");
		assert(ag != NULL);
		ag->ag_type = AG_TYPE_DENY;
	}

	if (conf->conf_default_pg_defined == false) {
		log_debugx("portal-group \"default\" not defined; "
		    "going with defaults");
		pg = portal_group_find(conf, "default");
		assert(pg != NULL);
		portal_group_add_listen(pg, "0.0.0.0:3260", false);
		portal_group_add_listen(pg, "[::]:3260", false);
	}

	conf->conf_kernel_port_on = true;

	error = conf_verify(conf);
	if (error != 0) {
		conf_delete(conf);
		return (NULL);
	}

	return (conf);
}
