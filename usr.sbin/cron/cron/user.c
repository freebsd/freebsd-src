/* Copyright 1988,1990,1993,1994 by Paul Vixie
 * All rights reserved
 */

/*
 * Copyright (c) 1997 by Internet Software Consortium
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM DISCLAIMS
 * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL INTERNET SOFTWARE
 * CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */


/* vix 26jan87 [log is in RCS file]
 */

#include "cron.h"

static char *User_name;

void
free_user(user *u)
{
	entry *e, *ne;

	free(u->name);
	for (e = u->crontab;  e != NULL;  e = ne) {
		ne = e->next;
		free_entry(e);
	}
	free(u);
}

static void
log_error(const char *msg)
{
	log_it(User_name, getpid(), "PARSE", msg);
}

/* NULL pw implies syscrontab */
user *
load_user(int crontab_fd, struct passwd *pw, const char *name)
{
	char envstr[MAX_ENVSTR];
	FILE *file;
	user *u;
	entry *e;
	int status;
	char **envp, **tenvp;

	if (!(file = fdopen(crontab_fd, "r"))) {
		warn("fdopen on crontab_fd in load_user");
		return (NULL);
	}

	Debug(DPARS, ("load_user()\n"))

	/* file is open.  build user entry, then read the crontab file.
	 */
	if ((u = (user *) malloc(sizeof(user))) == NULL) {
		errno = ENOMEM;
		return (NULL);
	}
	if ((u->name = strdup(name)) == NULL) {
		free(u);
		errno = ENOMEM;
		return (NULL);
	}
	u->crontab = NULL;

	/* 
	 * init environment.  this will be copied/augmented for each entry.
	 */
	if ((envp = env_init()) == NULL) {
		free(u->name);
		free(u);
		return (NULL);
	}

	/*
	 * load the crontab
	 */
	while ((status = load_env(envstr, file)) >= OK) {
		switch (status) {
		case ERR:
			free_user(u);
			u = NULL;
			goto done;
		case FALSE:
			User_name = u->name;    /* for log_error */
			e = load_entry(file, log_error, pw, envp);
			if (e) {
				e->next = u->crontab;
				u->crontab = e;
			}
			break;
		case TRUE:
			if ((tenvp = env_set(envp, envstr))) {
				envp = tenvp;
			} else {
				free_user(u);
				u = NULL;
				goto done;
			}
			break;
		}
	}

 done:
	env_free(envp);
	fclose(file);
	Debug(DPARS, ("...load_user() done\n"))
	return (u);
}
