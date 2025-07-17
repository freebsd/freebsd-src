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

#if !defined(lint) && !defined(LINT)
static const char rcsid[] =
    "$Id: database.c,v 1.3 1998/08/14 00:32:38 vixie Exp $";
#endif

/* vix 26jan87 [RCS has the log]
 */

#include "cron.h"

#define TMAX(a,b) ((a)>(b)?(a):(b))

static	void		process_crontab(const char *, const char *,
					const char *, struct stat *,
					cron_db *, cron_db *);

void
load_database(cron_db *old_db)
{
	struct stat statbuf, syscron_stat, st;
	cron_db new_db;
	DIR_T *dp;
	DIR *dir;
	user *u, *nu;
	time_t maxmtime;
	struct {
		const char *name;
		struct stat st;
	} syscrontabs [] = {
		{ SYSCRONTABS },
		{ LOCALSYSCRONTABS }
	};
	int i, ret;

	Debug(DLOAD, ("[%d] load_database()\n", getpid()))

	/* before we start loading any data, do a stat on SPOOL_DIR
	 * so that if anything changes as of this moment (i.e., before we've
	 * cached any of the database), we'll see the changes next time.
	 */
	if (stat(SPOOL_DIR, &statbuf) < OK) {
		log_it("CRON", getpid(), "STAT FAILED", SPOOL_DIR);
		(void) exit(ERROR_EXIT);
	}

	/* track system crontab file
	 */
	if (stat(SYSCRONTAB, &syscron_stat) < OK)
		syscron_stat.st_mtime = 0;

	maxmtime = TMAX(statbuf.st_mtime, syscron_stat.st_mtime);

	for (i = 0; i < nitems(syscrontabs); i++) {
		if (stat(syscrontabs[i].name, &syscrontabs[i].st) != -1) {
			maxmtime = TMAX(syscrontabs[i].st.st_mtime, maxmtime);
			/* Traverse into directory */
			if (!(dir = opendir(syscrontabs[i].name)))
				continue;
			while (NULL != (dp = readdir(dir))) {
				if (dp->d_name[0] == '.')
					continue;
				ret = fstatat(dirfd(dir), dp->d_name, &st, 0);
				if (ret != 0 || !S_ISREG(st.st_mode))
					continue;
				maxmtime = TMAX(st.st_mtime, maxmtime);
			}
			closedir(dir);
		} else {
			syscrontabs[i].st.st_mtime = 0;
		}
	}

	/* if spooldir's mtime has not changed, we don't need to fiddle with
	 * the database.
	 *
	 * Note that old_db->mtime is initialized to 0 in main(), and
	 * so is guaranteed to be different than the stat() mtime the first
	 * time this function is called.
	 */
	if (old_db->mtime == maxmtime) {
		Debug(DLOAD, ("[%d] spool dir mtime unch, no load needed.\n",
			      getpid()))
		return;
	}

	/* something's different.  make a new database, moving unchanged
	 * elements from the old database, reloading elements that have
	 * actually changed.  Whatever is left in the old database when
	 * we're done is chaff -- crontabs that disappeared.
	 */
	new_db.mtime = maxmtime;
	new_db.head = new_db.tail = NULL;

	if (syscron_stat.st_mtime) {
		process_crontab("root", SYS_NAME,
				SYSCRONTAB, &syscron_stat,
				&new_db, old_db);
	}

	for (i = 0; i < nitems(syscrontabs); i++) {
		char tabname[MAXPATHLEN];
		if (syscrontabs[i].st.st_mtime == 0)
			continue;
		if (!(dir = opendir(syscrontabs[i].name))) {
			log_it("CRON", getpid(), "OPENDIR FAILED",
			    syscrontabs[i].name);
			(void) exit(ERROR_EXIT);
		}

		while (NULL != (dp = readdir(dir))) {
			if (dp->d_name[0] == '.')
				continue;
			if (fstatat(dirfd(dir), dp->d_name, &st, 0) == 0 &&
			    !S_ISREG(st.st_mode))
				continue;
			snprintf(tabname, sizeof(tabname), "%s/%s",
			    syscrontabs[i].name, dp->d_name);
			process_crontab("root", SYS_NAME, tabname,
			    &syscrontabs[i].st, &new_db, old_db);
		}
		closedir(dir);
	}

	/* we used to keep this dir open all the time, for the sake of
	 * efficiency.  however, we need to close it in every fork, and
	 * we fork a lot more often than the mtime of the dir changes.
	 */
	if (!(dir = opendir(SPOOL_DIR))) {
		log_it("CRON", getpid(), "OPENDIR FAILED", SPOOL_DIR);
		(void) exit(ERROR_EXIT);
	}

	while (NULL != (dp = readdir(dir))) {
		char fname[MAXNAMLEN+1], tabname[MAXNAMLEN+1];

		/* avoid file names beginning with ".".  this is good
		 * because we would otherwise waste two guaranteed calls
		 * to getpwnam() for . and .., and also because user names
		 * starting with a period are just too nasty to consider.
		 */
		if (dp->d_name[0] == '.')
			continue;

		(void) strncpy(fname, dp->d_name, sizeof(fname));
		fname[sizeof(fname)-1] = '\0';

		if (snprintf(tabname, sizeof tabname, CRON_TAB(fname))
		    >= sizeof(tabname))
			continue;	/* XXX log? */

		process_crontab(fname, fname, tabname,
				&statbuf, &new_db, old_db);
	}
	closedir(dir);

	/* if we don't do this, then when our children eventually call
	 * getpwnam() in do_command.c's child_process to verify MAILTO=,
	 * they will screw us up (and v-v).
	 */
	endpwent();

	/* whatever's left in the old database is now junk.
	 */
	Debug(DLOAD, ("unlinking old database:\n"))
	for (u = old_db->head;  u != NULL;  u = nu) {
		Debug(DLOAD, ("\t%s\n", u->name))
		nu = u->next;
		unlink_user(old_db, u);
		free_user(u);
	}

	/* overwrite the database control block with the new one.
	 */
	*old_db = new_db;
	Debug(DLOAD, ("load_database is done\n"))
}

void
link_user(cron_db *db, user *u)
{
	if (db->head == NULL)
		db->head = u;
	if (db->tail)
		db->tail->next = u;
	u->prev = db->tail;
	u->next = NULL;
	db->tail = u;
}

void
unlink_user(cron_db *db, user *u)
{
	if (u->prev == NULL)
		db->head = u->next;
	else
		u->prev->next = u->next;

	if (u->next == NULL)
		db->tail = u->prev;
	else
		u->next->prev = u->prev;
}

user *
find_user(cron_db *db, const char *name)
{
	user *u;

	for (u = db->head;  u != NULL;  u = u->next)
		if (strcmp(u->name, name) == 0)
			break;
	return (u);
}

static void
process_crontab(const char *uname, const char *fname, const char *tabname,
		struct stat *statbuf, cron_db *new_db, cron_db *old_db)
{
	struct passwd *pw = NULL;
	int crontab_fd = OK - 1;
	user *u;
	entry *e;
	time_t now;

	if (strcmp(fname, SYS_NAME) != 0 && !(pw = getpwnam(uname))) {
		/* file doesn't have a user in passwd file.
		 */
		log_it(fname, getpid(), "ORPHAN", "no passwd entry");
		goto next_crontab;
	}

	if ((crontab_fd = open(tabname, O_RDONLY, 0)) < OK) {
		/* crontab not accessible?
		 */
		log_it(fname, getpid(), "CAN'T OPEN", tabname);
		goto next_crontab;
	}

	if (fstat(crontab_fd, statbuf) < OK) {
		log_it(fname, getpid(), "FSTAT FAILED", tabname);
		goto next_crontab;
	}

	Debug(DLOAD, ("\t%s:", fname))
	u = find_user(old_db, fname);
	if (u != NULL) {
		/* if crontab has not changed since we last read it
		 * in, then we can just use our existing entry.
		 */
		if (u->mtime == statbuf->st_mtime) {
			Debug(DLOAD, (" [no change, using old data]"))
			unlink_user(old_db, u);
			link_user(new_db, u);
			goto next_crontab;
		}

		/* before we fall through to the code that will reload
		 * the user, let's deallocate and unlink the user in
		 * the old database.  This is more a point of memory
		 * efficiency than anything else, since all leftover
		 * users will be deleted from the old database when
		 * we finish with the crontab...
		 */
		Debug(DLOAD, (" [delete old data]"))
		unlink_user(old_db, u);
		free_user(u);
		log_it(fname, getpid(), "RELOAD", tabname);
	}
	u = load_user(crontab_fd, pw, fname);
	if (u != NULL) {
		u->mtime = statbuf->st_mtime;
		/*
		 * TargetTime == 0 when we're initially populating the database,
		 * and TargetTime > 0 any time after that (i.e. we're reloading
		 * cron.d/ files because they've been created/modified).  In the
		 * latter case, we should check for any interval jobs and run
		 * them 'n' seconds from the time the job was loaded/reloaded.
		 * Otherwise, they will not be run until cron is restarted.
		 */
		if (TargetTime != 0) {
			now = time(NULL);
			for (e = u->crontab; e != NULL; e = e->next) {
				if ((e->flags & INTERVAL) != 0)
					e->lastexit = now;
			}
		}
		link_user(new_db, u);
	}

 next_crontab:
	if (crontab_fd >= OK) {
		Debug(DLOAD, (" [done]\n"))
		close(crontab_fd);
	}
}
