#if !defined(lint) && !defined(LINT)
static char rcsid[] = "$Header: /a/cvs/386BSD/src/libexec/crond/database.c,v 1.1.1.1 1993/06/12 14:55:04 rgrimes Exp $";
#endif

/* vix 26jan87 [RCS has the log]
 */

/* Copyright 1988,1990 by Paul Vixie
 * All rights reserved
 *
 * Distribute freely, except: don't remove my name from the source or
 * documentation (don't take credit for my work), mark your changes (don't
 * get me blamed for your possible bugs), don't alter or remove this
 * notice.  May be sold if buildable source is provided to buyer.  No
 * warrantee of any kind, express or implied, is included with this
 * software; use at your own risk, responsibility for damages (if any) to
 * anyone resulting from the use of this software rests entirely with the
 * user.
 *
 * Send bug reports, bug fixes, enhancements, requests, flames, etc., and
 * I'll try to keep a version up to date.  I can be reached as follows:
 * Paul Vixie, 329 Noe Street, San Francisco, CA, 94114, (415) 864-7013,
 * paul@vixie.sf.ca.us || {hoptoad,pacbell,decwrl,crash}!vixie!paul
 */


#include "cron.h"
#include <pwd.h>
#if defined(BSD)
# include <sys/file.h>
# include <sys/dir.h>
#endif
#if defined(ATT)
# include <sys/file.h>
# include <ndir.h>
# include <fcntl.h>
#endif


extern void	perror(), exit();


void
load_database(old_db)
	cron_db		*old_db;
{
	extern void	link_user(), unlink_user(), free_user();
	extern user	*load_user(), *find_user();
	extern char	*env_get();

	static DIR	*dir = NULL;

	struct stat	statbuf;
	struct direct	*dp;
	cron_db		new_db;
	user		*u;

	Debug(DLOAD, ("[%d] load_database()\n", getpid()))

	/* before we start loading any data, do a stat on SPOOL_DIR
	 * so that if anything changes as of this moment (i.e., before we've
	 * cached any of the database), we'll see the changes next time.
	 */
	if (stat(SPOOL_DIR, &statbuf) < OK)
	{
		log_it("CROND", getpid(), "STAT FAILED", SPOOL_DIR);
		(void) exit(ERROR_EXIT);
	}

	/* if spooldir's mtime has not changed, we don't need to fiddle with
	 * the database.  Note that if /etc/passwd changes (like, someone's
	 * UID/GID/HOME/SHELL, we won't see it.  Maybe we should
	 * keep an mtime for the passwd file?  HINT
	 *
	 * Note that old_db->mtime is initialized to 0 in main(), and
	 * so is guaranteed to be different than the stat() mtime the first
	 * time this function is called.
	 */
	if (old_db->mtime == statbuf.st_mtime)
	{
		Debug(DLOAD, ("[%d] spool dir mtime unch, no load needed.\n",
			getpid()))
		return;
	}

	/* make sure the dir is open.  only happens the first time, since
	 * the DIR is static and we don't close it.  Rewind the dir.
	 */
	if (dir == NULL)
	{
		if (!(dir = opendir(SPOOL_DIR)))
		{
			log_it("CROND", getpid(), "OPENDIR FAILED", SPOOL_DIR);
			(void) exit(ERROR_EXIT);
		}
	}
	(void) rewinddir(dir);

	/* something's different.  make a new database, moving unchanged
	 * elements from the old database, reloading elements that have
	 * actually changed.  Whatever is left in the old database when
	 * we're done is chaff -- crontabs that disappeared.
	 */
	new_db.mtime = statbuf.st_mtime;
	new_db.head = new_db.tail = NULL;

	while (NULL != (dp = readdir(dir)))
	{
		extern struct passwd	*getpwnam();
		struct passwd		*pw;
		int			crontab_fd;
		char			fname[MAXNAMLEN+1],
					tabname[MAXNAMLEN+1];

		(void) strncpy(fname, dp->d_name, (int) dp->d_namlen);
		fname[dp->d_namlen] = '\0';

		/* avoid file names beginning with ".".  this is good
		 * because we would otherwise waste two guaranteed calls
		 * to getpwnam() for . and .., and also because user names
		 * starting with a period are just too nasty to consider.
		 */
		if (fname[0] == '.')
			goto next_crontab;

		if (NULL == (pw = getpwnam(fname)))
		{
			/* file doesn't have a user in passwd file.
			 */
			log_it(fname, getpid(), "ORPHAN", "no passwd entry");
			goto next_crontab;
		}

		sprintf(tabname, CRON_TAB(fname));
		if ((crontab_fd = open(tabname, O_RDONLY, 0)) < OK)
		{
			/* crontab not accessible?
			 */
			log_it(fname, getpid(), "CAN'T OPEN", tabname);
			goto next_crontab;
		}

		if (fstat(crontab_fd, &statbuf) < OK)
		{
			log_it(fname, getpid(), "FSTAT FAILED", tabname);
			goto next_crontab;
		}

		Debug(DLOAD, ("\t%s:", fname))
		u = find_user(old_db, fname);
		if (u != NULL)
		{
			/* if crontab has not changed since we last read it
			 * in, then we can just use our existing entry.
			 * note that we do not check for changes in the
			 * passwd entry (uid, home dir, etc).  HINT
			 */
			if (u->mtime == statbuf.st_mtime)
			{
				Debug(DLOAD, (" [no change, using old data]"))
				unlink_user(old_db, u);
				link_user(&new_db, u);
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
		}
		u = load_user(
			crontab_fd,
			pw->pw_name,
			pw->pw_uid,
			pw->pw_gid,
			pw->pw_dir,
			pw->pw_shell
		);
		if (u != NULL)
		{
			u->mtime = statbuf.st_mtime;
			link_user(&new_db, u);
		}
next_crontab:
		if (crontab_fd >= OK) {
			Debug(DLOAD, (" [done]\n"))
			close(crontab_fd);
		}
	}
	/* if we don't do this, then when our children eventually call
	 * getpwnam() in do_command.c's child_process to verify MAILTO=,
	 * they will screw us up (and v-v).
	 *
	 * (this was lots of fun to find...)
	 */
	endpwent();

	/* whatever's left in the old database is now junk.
	 */
	Debug(DLOAD, ("unlinking old database:\n"))
	for (u = old_db->head;  u != NULL;  u = u->next)
	{
		Debug(DLOAD, ("\t%s\n", env_get(USERENV, u->envp)))
		unlink_user(old_db, u);
		free_user(u);
	}

	/* overwrite the database control block with the new one.
	 */
	Debug(DLOAD, ("installing new database\n"))
#if defined(BSD)
	/* BSD has structure assignments */
	*old_db = new_db;
#endif
#if defined(ATT)
	/* ATT, well, I don't know.  Use memcpy(). */
	memcpy(old_db, &new_db, sizeof(cron_db));
#endif
	Debug(DLOAD, ("load_database is done\n"))
}


void
link_user(db, u)
	cron_db	*db;
	user	*u;
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
unlink_user(db, u)
	cron_db	*db;
	user	*u;
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
find_user(db, name)
	cron_db	*db;
	char	*name;
{
	char	*env_get();
	user	*u;

	for (u = db->head;  u != NULL;  u = u->next)
		if (!strcmp(env_get(USERENV, u->envp), name))
			break;
	return u;
}
