/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2014-2025 Baptiste Daroussin <bapt@FreeBSD.org>
 * Copyright (c) 2013 Bryan Drewery <bdrewery@FreeBSD.org>
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
 */

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/utsname.h>
#include <sys/sysctl.h>

#include <dirent.h>
#include <ucl.h>
#include <err.h>
#include <errno.h>
#include <libutil.h>
#include <paths.h>
#include <stdbool.h>
#include <unistd.h>
#include <ctype.h>

#include "config.h"

struct config_value {
	char *value;
	STAILQ_ENTRY(config_value) next;
};

struct config_entry {
	uint8_t type;
	const char *key;
	const char *val;
	char *value;
	STAILQ_HEAD(, config_value) *list;
	bool envset;
	bool main_only;				/* Only set in pkg.conf. */
};

static struct repositories repositories = STAILQ_HEAD_INITIALIZER(repositories);

static struct config_entry c[] = {
	[PACKAGESITE] = {
		PKG_CONFIG_STRING,
		"PACKAGESITE",
		URL_SCHEME_PREFIX "http://pkg.FreeBSD.org/${ABI}/latest",
		NULL,
		NULL,
		false,
		false,
	},
	[ABI] = {
		PKG_CONFIG_STRING,
		"ABI",
		NULL,
		NULL,
		NULL,
		false,
		true,
	},
	[MIRROR_TYPE] = {
		PKG_CONFIG_STRING,
		"MIRROR_TYPE",
		"SRV",
		NULL,
		NULL,
		false,
		false,
	},
	[ASSUME_ALWAYS_YES] = {
		PKG_CONFIG_BOOL,
		"ASSUME_ALWAYS_YES",
		"NO",
		NULL,
		NULL,
		false,
		true,
	},
	[SIGNATURE_TYPE] = {
		PKG_CONFIG_STRING,
		"SIGNATURE_TYPE",
		NULL,
		NULL,
		NULL,
		false,
		false,
	},
	[FINGERPRINTS] = {
		PKG_CONFIG_STRING,
		"FINGERPRINTS",
		NULL,
		NULL,
		NULL,
		false,
		false,
	},
	[REPOS_DIR] = {
		PKG_CONFIG_LIST,
		"REPOS_DIR",
		NULL,
		NULL,
		NULL,
		false,
		true,
	},
	[PUBKEY] = {
		PKG_CONFIG_STRING,
		"PUBKEY",
		NULL,
		NULL,
		NULL,
		false,
		false
	},
	[PKG_ENV] = {
		PKG_CONFIG_OBJECT,
		"PKG_ENV",
		NULL,
		NULL,
		NULL,
		false,
		false,
	}
};

static char *
pkg_get_myabi(void)
{
	struct utsname uts;
	char machine_arch[255];
	char *abi;
	size_t len;
	int error;

	error = uname(&uts);
	if (error)
		return (NULL);

	len = sizeof(machine_arch);
	error = sysctlbyname("hw.machine_arch", machine_arch, &len, NULL, 0);
	if (error)
		return (NULL);
	machine_arch[len] = '\0';

	/*
	 * Use __FreeBSD_version rather than kernel version (uts.release) for
	 * use in jails. This is equivalent to the value of uname -U.
	 */
	error = asprintf(&abi, "%s:%d:%s", uts.sysname, __FreeBSD_version/100000,
	    machine_arch);
	if (error < 0)
		return (NULL);

	return (abi);
}

static void
subst_packagesite(const char *abi)
{
	char *newval;
	const char *variable_string;
	const char *oldval;

	if (c[PACKAGESITE].value != NULL)
		oldval = c[PACKAGESITE].value;
	else
		oldval = c[PACKAGESITE].val;

	if ((variable_string = strstr(oldval, "${ABI}")) == NULL)
		return;

	asprintf(&newval, "%.*s%s%s",
	    (int)(variable_string - oldval), oldval, abi,
	    variable_string + strlen("${ABI}"));
	if (newval == NULL)
		errx(EXIT_FAILURE, "asprintf");

	free(c[PACKAGESITE].value);
	c[PACKAGESITE].value = newval;
}

static int
boolstr_to_bool(const char *str)
{
	if (str != NULL && (strcasecmp(str, "true") == 0 ||
	    strcasecmp(str, "yes") == 0 || strcasecmp(str, "on") == 0 ||
	    str[0] == '1'))
		return (true);

	return (false);
}

static void
config_parse(const ucl_object_t *obj)
{
	FILE *buffp;
	char *buf = NULL;
	size_t bufsz = 0;
	const ucl_object_t *cur, *seq, *tmp;
	ucl_object_iter_t it = NULL, itseq = NULL, it_obj = NULL;
	struct config_entry *temp_config;
	struct config_value *cv;
	const char *key, *evkey;
	int i;
	size_t j;

	/* Temporary config for configs that may be disabled. */
	temp_config = calloc(CONFIG_SIZE, sizeof(struct config_entry));
	buffp = open_memstream(&buf, &bufsz);
	if (buffp == NULL)
		err(EXIT_FAILURE, "open_memstream()");

	while ((cur = ucl_iterate_object(obj, &it, true))) {
		key = ucl_object_key(cur);
		if (key == NULL)
			continue;
		if (buf != NULL)
			memset(buf, 0, bufsz);
		rewind(buffp);

			for (j = 0; j < strlen(key); ++j)
				fputc(toupper(key[j]), buffp);
			fflush(buffp);

		for (i = 0; i < CONFIG_SIZE; i++) {
			if (strcmp(buf, c[i].key) == 0)
				break;
		}

		/* Silently skip unknown keys to be future compatible. */
		if (i == CONFIG_SIZE)
			continue;

		/* env has priority over config file */
		if (c[i].envset)
			continue;

		/* Parse sequence value ["item1", "item2"] */
		switch (c[i].type) {
		case PKG_CONFIG_LIST:
			if (cur->type != UCL_ARRAY) {
				warnx("Skipping invalid array "
				    "value for %s.\n", c[i].key);
				continue;
			}
			temp_config[i].list =
			    malloc(sizeof(*temp_config[i].list));
			STAILQ_INIT(temp_config[i].list);

			while ((seq = ucl_iterate_object(cur, &itseq, true))) {
				if (seq->type != UCL_STRING)
					continue;
				cv = malloc(sizeof(struct config_value));
				cv->value =
				    strdup(ucl_object_tostring(seq));
				STAILQ_INSERT_TAIL(temp_config[i].list, cv,
				    next);
			}
			break;
		case PKG_CONFIG_BOOL:
			temp_config[i].value =
			    strdup(ucl_object_toboolean(cur) ? "yes" : "no");
			break;
		case PKG_CONFIG_OBJECT:
			if (strcmp(c[i].key, "PKG_ENV") == 0) {
				while ((tmp =
				    ucl_iterate_object(cur, &it_obj, true))) {
					evkey = ucl_object_key(tmp);
					if (evkey != NULL && *evkey != '\0') {
						setenv(evkey, ucl_object_tostring_forced(tmp), 1);
					}
				}
			}
			break;
		default:
			/* Normal string value. */
			temp_config[i].value = strdup(ucl_object_tostring(cur));
			break;
		}
	}

	/* Repo is enabled, copy over all settings from temp_config. */
	for (i = 0; i < CONFIG_SIZE; i++) {
		if (c[i].envset)
			continue;
		/* Prevent overriding ABI, ASSUME_ALWAYS_YES, etc. */
		if (c[i].main_only == true)
			continue;
		switch (c[i].type) {
		case PKG_CONFIG_LIST:
			c[i].list = temp_config[i].list;
			break;
		default:
			c[i].value = temp_config[i].value;
			break;
		}
	}

	free(temp_config);
	fclose(buffp);
	free(buf);
}


static void
parse_mirror_type(struct repository *r, const char *mt)
{
	if (strcasecmp(mt, "srv") == 0)
		r->mirror_type = MIRROR_SRV;
	r->mirror_type = MIRROR_NONE;
}

static void
repo_free(struct repository *r)
{
	free(r->name);
	free(r->url);
	free(r->fingerprints);
	free(r->pubkey);
	free(r);
}

static bool
parse_signature_type(struct repository *repo, const char *st)
{
	if (strcasecmp(st, "FINGERPRINTS") == 0)
		repo->signature_type = SIGNATURE_FINGERPRINT;
	else if (strcasecmp(st, "PUBKEY") == 0)
		repo->signature_type = SIGNATURE_PUBKEY;
	else if (strcasecmp(st, "NONE") == 0)
		repo->signature_type = SIGNATURE_NONE;
	else {
		warnx("Signature type %s is not supported for bootstrapping,"
		    " ignoring repository %s", st, repo->name);
		return (false);
	}
	return (true);
}

static struct repository *
find_repository(const char *name)
{
	struct repository *repo;
	STAILQ_FOREACH(repo, &repositories, next) {
		if (strcmp(repo->name, name) == 0)
			return (repo);
	}
	return (NULL);
}

static void
parse_repo(const ucl_object_t *o)
{
	const ucl_object_t *cur;
	const char *key, *reponame;
	ucl_object_iter_t it = NULL;
	bool newrepo = false;
	struct repository *repo;

	reponame = ucl_object_key(o);
	repo = find_repository(reponame);
	if (repo == NULL) {
		repo = calloc(1, sizeof(struct repository));
		if (repo == NULL)
			err(EXIT_FAILURE, "calloc");

		repo->name = strdup(reponame);
		if (repo->name == NULL)
			err(EXIT_FAILURE, "strdup");
		newrepo = true;
	}
	while ((cur = ucl_iterate_object(o, &it, true))) {
		key = ucl_object_key(cur);
		if (key == NULL)
			continue;
		if (strcasecmp(key, "url") == 0) {
			free(repo->url);
			repo->url = strdup(ucl_object_tostring(cur));
			if (repo->url == NULL)
				err(EXIT_FAILURE, "strdup");
		} else if (strcasecmp(key, "mirror_type") == 0) {
			parse_mirror_type(repo, ucl_object_tostring(cur));
		} else if (strcasecmp(key, "signature_type") == 0) {
			if (!parse_signature_type(repo, ucl_object_tostring(cur))) {
				if (newrepo)
					repo_free(repo);
				else
					STAILQ_REMOVE(&repositories, repo, repository, next);
				return;
			}
		} else if (strcasecmp(key, "fingerprints") == 0) {
			free(repo->fingerprints);
			repo->fingerprints = strdup(ucl_object_tostring(cur));
			if (repo->fingerprints == NULL)
				err(EXIT_FAILURE, "strdup");
		} else if (strcasecmp(key, "pubkey") == 0) {
			free(repo->pubkey);
			repo->pubkey = strdup(ucl_object_tostring(cur));
			if (repo->pubkey == NULL)
				err(EXIT_FAILURE, "strdup");
		} else if (strcasecmp(key, "enabled") == 0) {
			if ((cur->type != UCL_BOOLEAN) ||
			    !ucl_object_toboolean(cur)) {
				if (newrepo)
					repo_free(repo);
				else
					STAILQ_REMOVE(&repositories, repo, repository, next);
				return;
			}
		}
	}
	/* At least we need an url */
	if (repo->url == NULL) {
		repo_free(repo);
		return;
	}
	if (newrepo)
		STAILQ_INSERT_TAIL(&repositories, repo, next);
	return;
}

/*-
 * Parse new repo style configs in style:
 * Name:
 *   URL:
 *   MIRROR_TYPE:
 * etc...
 */
static void
parse_repo_file(ucl_object_t *obj, const char *requested_repo)
{
	ucl_object_iter_t it = NULL;
	const ucl_object_t *cur;
	const char *key;

	while ((cur = ucl_iterate_object(obj, &it, true))) {
		key = ucl_object_key(cur);

		if (key == NULL)
			continue;

		if (cur->type != UCL_OBJECT)
			continue;

		if (requested_repo != NULL && strcmp(requested_repo, key) != 0)
			continue;
		parse_repo(cur);
	}
}


static int
read_conf_file(const char *confpath, const char *requested_repo,
    pkg_conf_file_t conftype)
{
	struct ucl_parser *p;
	ucl_object_t *obj = NULL;
	char *abi = pkg_get_myabi(), *major, *minor;
	struct utsname uts;
	int ret;

	if (uname(&uts))
		err(EXIT_FAILURE, "uname");
	if (abi == NULL)
		errx(EXIT_FAILURE, "Failed to determine ABI");

	p = ucl_parser_new(0);
	asprintf(&major, "%d",  __FreeBSD_version/100000);
	if (major == NULL)
		err(EXIT_FAILURE, "asprintf");
	asprintf(&minor, "%d",  (__FreeBSD_version / 1000) % 100);
	if (minor == NULL)
		err(EXIT_FAILURE, "asprintf");
	ucl_parser_register_variable(p, "ABI", abi);
	ucl_parser_register_variable(p, "OSNAME", uts.sysname);
	ucl_parser_register_variable(p, "RELEASE", major);
	ucl_parser_register_variable(p, "VERSION_MAJOR", major);
	ucl_parser_register_variable(p, "VERSION_MINOR", minor);

	if (!ucl_parser_add_file(p, confpath)) {
		if (errno != ENOENT)
			errx(EXIT_FAILURE, "Unable to parse configuration "
			    "file %s: %s", confpath, ucl_parser_get_error(p));
		/* no configuration present */
		ret = 1;
		goto out;
	}

	obj = ucl_parser_get_object(p);
	if (obj->type != UCL_OBJECT) 
		warnx("Invalid configuration format, ignoring the "
		    "configuration file %s", confpath);
	else {
		if (conftype == CONFFILE_PKG)
			config_parse(obj);
		else if (conftype == CONFFILE_REPO)
			parse_repo_file(obj, requested_repo);
	}
	ucl_object_unref(obj);

	ret = 0;
out:
	ucl_parser_free(p);
	free(abi);
	free(major);
	free(minor);

	return (ret);
}

static void
load_repositories(const char *repodir, const char *requested_repo)
{
	struct dirent *ent;
	DIR *d;
	char *p;
	size_t n;
	char path[MAXPATHLEN];

	if ((d = opendir(repodir)) == NULL)
		return;

	while ((ent = readdir(d))) {
		/* Trim out 'repos'. */
		if ((n = strlen(ent->d_name)) <= 5)
			continue;
		p = &ent->d_name[n - 5];
		if (strcmp(p, ".conf") == 0) {
			snprintf(path, sizeof(path), "%s%s%s",
			    repodir,
			    repodir[strlen(repodir) - 1] == '/' ? "" : "/",
			    ent->d_name);
			if (access(path, F_OK) != 0)
				continue;
			if (read_conf_file(path, requested_repo,
			    CONFFILE_REPO)) {
				goto cleanup;
			}
		}
	}

cleanup:
	closedir(d);
}

int
config_init(const char *requested_repo)
{
	char *val;
	int i;
	const char *localbase;
	char *abi, *env_list_item;
	char confpath[MAXPATHLEN];
	struct config_value *cv;

	for (i = 0; i < CONFIG_SIZE; i++) {
		val = getenv(c[i].key);
		if (val != NULL) {
			c[i].envset = true;
			switch (c[i].type) {
			case PKG_CONFIG_LIST:
				/* Split up comma-separated items from env. */
				c[i].list = malloc(sizeof(*c[i].list));
				STAILQ_INIT(c[i].list);
				for (env_list_item = strtok(val, ",");
				    env_list_item != NULL;
				    env_list_item = strtok(NULL, ",")) {
					cv =
					    malloc(sizeof(struct config_value));
					cv->value =
					    strdup(env_list_item);
					STAILQ_INSERT_TAIL(c[i].list, cv,
					    next);
				}
				break;
			default:
				c[i].val = val;
				break;
			}
		}
	}

	/* Read LOCALBASE/etc/pkg.conf first. */
	localbase = getlocalbase();
	snprintf(confpath, sizeof(confpath), "%s/etc/pkg.conf", localbase);

	if (access(confpath, F_OK) == 0 && read_conf_file(confpath, NULL,
	    CONFFILE_PKG))
		goto finalize;

	/* Then read in all repos from REPOS_DIR list of directories. */
	if (c[REPOS_DIR].list == NULL) {
		c[REPOS_DIR].list = malloc(sizeof(*c[REPOS_DIR].list));
		STAILQ_INIT(c[REPOS_DIR].list);
		cv = malloc(sizeof(struct config_value));
		cv->value = strdup("/etc/pkg");
		STAILQ_INSERT_TAIL(c[REPOS_DIR].list, cv, next);
		cv = malloc(sizeof(struct config_value));
		if (asprintf(&cv->value, "%s/etc/pkg/repos", localbase) < 0)
			goto finalize;
		STAILQ_INSERT_TAIL(c[REPOS_DIR].list, cv, next);
	}

	STAILQ_FOREACH(cv, c[REPOS_DIR].list, next)
		load_repositories(cv->value, requested_repo);

finalize:
	if (c[ABI].val == NULL && c[ABI].value == NULL) {
		abi = pkg_get_myabi();
		if (abi == NULL)
			errx(EXIT_FAILURE, "Failed to determine the system "
			    "ABI");
		c[ABI].val = abi;
	}

	return (0);
}

int
config_string(pkg_config_key k, const char **val)
{
	if (c[k].type != PKG_CONFIG_STRING)
		return (-1);

	if (c[k].value != NULL)
		*val = c[k].value;
	else
		*val = c[k].val;

	return (0);
}

int
config_bool(pkg_config_key k, bool *val)
{
	const char *value;

	if (c[k].type != PKG_CONFIG_BOOL)
		return (-1);

	*val = false;

	if (c[k].value != NULL)
		value = c[k].value;
	else
		value = c[k].val;

	if (boolstr_to_bool(value))
		*val = true;

	return (0);
}

struct repositories *
config_get_repositories(void)
{
	if (STAILQ_EMPTY(&repositories)) {
		/* Fall back to PACKAGESITE - deprecated - */
		struct repository *r = calloc(1, sizeof(*r));
		if (r == NULL)
			err(EXIT_FAILURE, "calloc");
		r->name = strdup("fallback");
		if (r->name == NULL)
			err(EXIT_FAILURE, "strdup");
		subst_packagesite(c[ABI].value != NULL ? c[ABI].value : c[ABI].val);
		r->url = c[PACKAGESITE].value;
		if (c[SIGNATURE_TYPE].value != NULL)
			if (!parse_signature_type(r, c[SIGNATURE_TYPE].value))
				exit(EXIT_FAILURE);
		if (c[MIRROR_TYPE].value != NULL)
			parse_mirror_type(r, c[MIRROR_TYPE].value);
		if (c[PUBKEY].value != NULL)
			r->pubkey = c[PUBKEY].value;
		if (c[FINGERPRINTS].value != NULL)
			r->fingerprints = c[FINGERPRINTS].value;
		STAILQ_INSERT_TAIL(&repositories, r, next);
	}
	return (&repositories);
}

void
config_finish(void) {
	int i;

	for (i = 0; i < CONFIG_SIZE; i++)
		free(c[i].value);
}
