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

%token ALIAS AUTH_GROUP BACKEND BLOCKSIZE CHAP CHAP_MUTUAL CLOSING_BRACKET
%token DEBUG DEVICE_ID DISCOVERY_AUTH_GROUP LISTEN LISTEN_ISER LUN MAXPROC NUM
%token OPENING_BRACKET OPTION PATH PIDFILE PORTAL_GROUP SERIAL SIZE STR TARGET 
%token TIMEOUT

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
	debug_statement
	|
	timeout_statement
	|
	maxproc_statement
	|
	pidfile_statement
	|
	auth_group_definition
	|
	portal_group_definition
	|
	target_statement
	;

debug_statement:	DEBUG NUM
	{
		conf->conf_debug = $2;
	}
	;

timeout_statement:	TIMEOUT NUM
	{
		conf->conf_timeout = $2;
	}
	;

maxproc_statement:	MAXPROC NUM
	{
		conf->conf_maxproc = $2;
	}
	;

pidfile_statement:	PIDFILE STR
	{
		if (conf->conf_pidfile_path != NULL) {
			log_warnx("pidfile specified more than once");
			free($2);
			return (1);
		}
		conf->conf_pidfile_path = $2;
	}
	;

auth_group_definition:	AUTH_GROUP auth_group_name
    OPENING_BRACKET auth_group_entries CLOSING_BRACKET
	{
		auth_group = NULL;
	}
	;

auth_group_name:	STR
	{
		auth_group = auth_group_new(conf, $1);
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
	auth_group_chap
	|
	auth_group_chap_mutual
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

portal_group_definition:	PORTAL_GROUP portal_group_name
    OPENING_BRACKET portal_group_entries CLOSING_BRACKET
	{
		portal_group = NULL;
	}
	;

portal_group_name:	STR
	{
		portal_group = portal_group_new(conf, $1);
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

target_statement:	TARGET target_iqn
    OPENING_BRACKET target_entries CLOSING_BRACKET
	{
		target = NULL;
	}
	;

target_iqn:	STR
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
	alias_statement
	|
	auth_group_statement
	|
	chap_statement
	|
	chap_mutual_statement
	|
	portal_group_statement
	|
	lun_statement
	;

alias_statement:	ALIAS STR
	{
		if (target->t_alias != NULL) {
			log_warnx("alias for target \"%s\" "
			    "specified more than once", target->t_iqn);
			return (1);
		}
		target->t_alias = $2;
	}
	;

auth_group_statement:	AUTH_GROUP STR
	{
		if (target->t_auth_group != NULL) {
			if (target->t_auth_group->ag_name != NULL)
				log_warnx("auth-group for target \"%s\" "
				    "specified more than once", target->t_iqn);
			else
				log_warnx("cannot mix auth-group with explicit "
				    "authorisations for target \"%s\"",
				    target->t_iqn);
			return (1);
		}
		target->t_auth_group = auth_group_find(conf, $2);
		if (target->t_auth_group == NULL) {
			log_warnx("unknown auth-group \"%s\" for target "
			    "\"%s\"", $2, target->t_iqn);
			return (1);
		}
		free($2);
	}
	;

chap_statement:	CHAP STR STR
	{
		const struct auth *ca;

		if (target->t_auth_group != NULL) {
			if (target->t_auth_group->ag_name != NULL) {
				log_warnx("cannot mix auth-group with explicit "
				    "authorisations for target \"%s\"",
				    target->t_iqn);
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

chap_mutual_statement:	CHAP_MUTUAL STR STR STR STR
	{
		const struct auth *ca;

		if (target->t_auth_group != NULL) {
			if (target->t_auth_group->ag_name != NULL) {
				log_warnx("cannot mix auth-group with explicit "
				    "authorisations for target \"%s\"",
				    target->t_iqn);
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

portal_group_statement:	PORTAL_GROUP STR
	{
		if (target->t_portal_group != NULL) {
			log_warnx("portal-group for target \"%s\" "
			    "specified more than once", target->t_iqn);
			free($2);
			return (1);
		}
		target->t_portal_group = portal_group_find(conf, $2);
		if (target->t_portal_group == NULL) {
			log_warnx("unknown portal-group \"%s\" for target "
			    "\"%s\"", $2, target->t_iqn);
			free($2);
			return (1);
		}
		free($2);
	}
	;

lun_statement:	LUN lun_number
    OPENING_BRACKET lun_statement_entries CLOSING_BRACKET
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

lun_statement_entries:
	|
	lun_statement_entries lun_statement_entry
	;

lun_statement_entry:
	backend_statement
	|
	blocksize_statement
	|
	device_id_statement
	|
	option_statement
	|
	path_statement
	|
	serial_statement
	|
	size_statement
	;

backend_statement:	BACKEND STR
	{
		if (lun->l_backend != NULL) {
			log_warnx("backend for lun %d, target \"%s\" "
			    "specified more than once",
			    lun->l_lun, target->t_iqn);
			free($2);
			return (1);
		}
		lun_set_backend(lun, $2);
		free($2);
	}
	;

blocksize_statement:	BLOCKSIZE NUM
	{
		if (lun->l_blocksize != 0) {
			log_warnx("blocksize for lun %d, target \"%s\" "
			    "specified more than once",
			    lun->l_lun, target->t_iqn);
			return (1);
		}
		lun_set_blocksize(lun, $2);
	}
	;

device_id_statement:	DEVICE_ID STR
	{
		if (lun->l_device_id != NULL) {
			log_warnx("device_id for lun %d, target \"%s\" "
			    "specified more than once",
			    lun->l_lun, target->t_iqn);
			free($2);
			return (1);
		}
		lun_set_device_id(lun, $2);
		free($2);
	}
	;

option_statement:	OPTION STR STR
	{
		struct lun_option *clo;
		
		clo = lun_option_new(lun, $2, $3);
		free($2);
		free($3);
		if (clo == NULL)
			return (1);
	}
	;

path_statement:	PATH STR
	{
		if (lun->l_path != NULL) {
			log_warnx("path for lun %d, target \"%s\" "
			    "specified more than once",
			    lun->l_lun, target->t_iqn);
			free($2);
			return (1);
		}
		lun_set_path(lun, $2);
		free($2);
	}
	;

serial_statement:	SERIAL STR
	{
		if (lun->l_serial != NULL) {
			log_warnx("serial for lun %d, target \"%s\" "
			    "specified more than once",
			    lun->l_lun, target->t_iqn);
			free($2);
			return (1);
		}
		lun_set_serial(lun, $2);
		free($2);
	}
	;

size_statement:	SIZE NUM
	{
		if (lun->l_size != 0) {
			log_warnx("size for lun %d, target \"%s\" "
			    "specified more than once",
			    lun->l_lun, target->t_iqn);
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

	ag = auth_group_new(conf, "no-authentication");
	ag->ag_type = AG_TYPE_NO_AUTHENTICATION;

	/*
	 * Here, the type doesn't really matter, as the group doesn't contain
	 * any entries and thus will always deny access.
	 */
	ag = auth_group_new(conf, "no-access");
	ag->ag_type = AG_TYPE_CHAP;

	pg = portal_group_new(conf, "default");
	portal_group_add_listen(pg, "0.0.0.0:3260", false);
	portal_group_add_listen(pg, "[::]:3260", false);

	yyin = fopen(path, "r");
	if (yyin == NULL) {
		log_warn("unable to open configuration file %s", path);
		conf_delete(conf);
		return (NULL);
	}
	check_perms(path);
	lineno = 0;
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

	error = conf_verify(conf);
	if (error != 0) {
		conf_delete(conf);
		return (NULL);
	}

	return (conf);
}
