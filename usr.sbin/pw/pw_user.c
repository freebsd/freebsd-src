/*-
 * Copyright (C) 1996
 *	David L. Nugent.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY DAVID L. NUGENT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL DAVID L. NUGENT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * 
 */

#ifndef lint
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

#include <ctype.h>
#include <err.h>
#include <fcntl.h>
#include <sys/param.h>
#include <dirent.h>
#include <paths.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <login_cap.h>
#include <pwd.h>
#include <grp.h>
#include <libutil.h>
#include "pw.h"
#include "bitmap.h"

#define LOGNAMESIZE (MAXLOGNAME-1)

static		char locked_str[] = "*LOCKED*";

static int	pw_userdel(char *name, long id);
static int	print_user(struct passwd * pwd);
static uid_t    pw_uidpolicy(struct userconf * cnf, long id);
static uid_t    pw_gidpolicy(struct cargs * args, char *nam, gid_t prefer);
static time_t   pw_pwdpolicy(struct userconf * cnf, struct cargs * args);
static time_t   pw_exppolicy(struct userconf * cnf, struct cargs * args);
static char    *pw_homepolicy(struct userconf * cnf, struct cargs * args, char const * user);
static char    *pw_shellpolicy(struct userconf * cnf, struct cargs * args, char *newshell);
static char    *pw_password(struct userconf * cnf, char const * user);
static char    *shell_path(char const * path, char *shells[], char *sh);
static void     rmat(uid_t uid);
static void     rmopie(char const * name);

static void
create_and_populate_homedir(struct passwd *pwd)
{
	struct userconf *cnf = conf.userconf;
	const char *skeldir;
	int skelfd = -1;

	skeldir = cnf->dotdir;

	if (skeldir != NULL && *skeldir != '\0') {
		skelfd = openat(conf.rootfd, cnf->dotdir,
		    O_DIRECTORY|O_CLOEXEC);
	}

	copymkdir(conf.rootfd, pwd->pw_dir, skelfd, cnf->homemode, pwd->pw_uid,
	    pwd->pw_gid, 0);
	pw_log(cnf, M_ADD, W_USER, "%s(%u) home %s made", pwd->pw_name,
	    pwd->pw_uid, pwd->pw_dir);
}

static int
set_passwd(struct passwd *pwd, bool update)
{
	int		 b, istty;
	struct termios	 t, n;
	login_cap_t	*lc;
	char		line[_PASSWORD_LEN+1];
	char		*p;

	if (conf.fd == '-') {
		if (!pwd->pw_passwd || *pwd->pw_passwd != '*') {
			pwd->pw_passwd = "*";	/* No access */
			return (1);
		}
		return (0);
	}

	if ((istty = isatty(conf.fd))) {
		if (tcgetattr(conf.fd, &t) == -1)
			istty = 0;
		else {
			n = t;
			n.c_lflag &= ~(ECHO);
			tcsetattr(conf.fd, TCSANOW, &n);
			printf("%s%spassword for user %s:",
			    update ? "new " : "",
			    conf.precrypted ? "encrypted " : "",
			    pwd->pw_name);
			fflush(stdout);
		}
	}
	b = read(conf.fd, line, sizeof(line) - 1);
	if (istty) {	/* Restore state */
		tcsetattr(conf.fd, TCSANOW, &t);
		fputc('\n', stdout);
		fflush(stdout);
	}

	if (b < 0)
		err(EX_IOERR, "-%c file descriptor",
		    conf.precrypted ? 'H' : 'h');
	line[b] = '\0';
	if ((p = strpbrk(line, "\r\n")) != NULL)
		*p = '\0';
	if (!*line)
		errx(EX_DATAERR, "empty password read on file descriptor %d",
		    conf.fd);
	if (conf.precrypted) {
		if (strchr(line, ':') != NULL)
			errx(EX_DATAERR, "bad encrypted password");
		pwd->pw_passwd = line;
	} else {
		lc = login_getpwclass(pwd);
		if (lc == NULL ||
				login_setcryptfmt(lc, "sha512", NULL) == NULL)
			warn("setting crypt(3) format");
		login_close(lc);
		pwd->pw_passwd = pw_pwcrypt(line);
	}
	return (1);
}

int
pw_usernext(struct userconf *cnf, bool quiet)
{
	uid_t next = pw_uidpolicy(cnf, -1);

	if (quiet)
		return (next);

	printf("%u:", next);
	pw_groupnext(cnf, quiet);

	return (EXIT_SUCCESS);
}

static int
pw_usershow(char *name, long id, struct passwd *fakeuser)
{
	struct passwd *pwd = NULL;

	if (id < 0 && name == NULL && !conf.all)
		errx(EX_DATAERR, "username or id or '-a' required");

	if (conf.all) {
		SETPWENT();
		while ((pwd = GETPWENT()) != NULL)
			print_user(pwd);
		ENDPWENT();
		return (EXIT_SUCCESS);
	}

	pwd = (name != NULL) ? GETPWNAM(pw_checkname(name, 0)) : GETPWUID(id);
	if (pwd == NULL) {
		if (conf.force) {
			pwd = fakeuser;
		} else {
			if (name == NULL)
				errx(EX_NOUSER, "no such uid `%ld'", id);
			errx(EX_NOUSER, "no such user `%s'", name);
		}
	}

	return (print_user(pwd));
}

static void
perform_chgpwent(const char *name, struct passwd *pwd)
{
	int rc;

	rc = chgpwent(name, pwd);
	if (rc == -1)
		errx(EX_IOERR, "user '%s' does not exist (NIS?)", pwd->pw_name);
	else if (rc != 0)
		err(EX_IOERR, "passwd file update");

	if (conf.userconf->nispasswd && *conf.userconf->nispasswd == '/') {
		rc = chgnispwent(conf.userconf->nispasswd, name, pwd);
		if (rc == -1)
			warn("User '%s' not found in NIS passwd", pwd->pw_name);
		else
			warn("NIS passwd update");
		/* NOTE: NIS-only update errors are not fatal */
	}
}

/*
 * The M_LOCK and M_UNLOCK functions simply add or remove
 * a "*LOCKED*" prefix from in front of the password to
 * prevent it decoding correctly, and therefore prevents
 * access. Of course, this only prevents access via
 * password authentication (not ssh, kerberos or any
 * other method that does not use the UNIX password) but
 * that is a known limitation.
 */
static int
pw_userlock(char *name, long id, int mode)
{
	struct passwd *pwd = NULL;
	char *passtmp = NULL;
	bool locked = false;

	if (id < 0 && name == NULL)
		errx(EX_DATAERR, "username or id required");

	pwd = (name != NULL) ? GETPWNAM(pw_checkname(name, 0)) : GETPWUID(id);
	if (pwd == NULL) {
		if (name == NULL)
			errx(EX_NOUSER, "no such uid `%ld'", id);
		errx(EX_NOUSER, "no such user `%s'", name);
	}

	if (name == NULL)
		name = pwd->pw_name;

	if (strncmp(pwd->pw_passwd, locked_str, sizeof(locked_str) -1) == 0)
		locked = true;
	if (mode == M_LOCK && locked)
		errx(EX_DATAERR, "user '%s' is already locked", pwd->pw_name);
	if (mode == M_UNLOCK && !locked)
		errx(EX_DATAERR, "user '%s' is not locked", pwd->pw_name);

	if (mode == M_LOCK) {
		asprintf(&passtmp, "%s%s", locked_str, pwd->pw_passwd);
		if (passtmp == NULL)	/* disaster */
			errx(EX_UNAVAILABLE, "out of memory");
		pwd->pw_passwd = passtmp;
	} else {
		pwd->pw_passwd += sizeof(locked_str)-1;
	}

	perform_chgpwent(name, pwd);
	free(passtmp);

	return (EXIT_SUCCESS);
}

/*-
 * -C config      configuration file
 * -q             quiet operation
 * -n name        login name
 * -u uid         user id
 * -c comment     user name/comment
 * -d directory   home directory
 * -e date        account expiry date
 * -p date        password expiry date
 * -g grp         primary group
 * -G grp1,grp2   additional groups
 * -m [ -k dir ]  create and set up home
 * -s shell       name of login shell
 * -o             duplicate uid ok
 * -L class       user class
 * -l name        new login name
 * -h fd          password filehandle
 * -H fd          encrypted password filehandle
 * -F             force print or add
 *   Setting defaults:
 * -D             set user defaults
 * -b dir         default home root dir
 * -e period      default expiry period
 * -p period      default password change period
 * -g group       default group
 * -G             grp1,grp2.. default additional groups
 * -L class       default login class
 * -k dir         default home skeleton
 * -s shell       default shell
 * -w method      default password method
 */

int
pw_user(int mode, char *name, long id, struct cargs * args)
{
	int	        rc, edited = 0;
	char           *p = NULL;
	struct carg    *arg;
	struct passwd  *pwd = NULL;
	struct group   *grp;
	struct stat     st;
	struct userconf	*cnf;
	char            line[_PASSWORD_LEN+1];
	char		path[MAXPATHLEN];
	FILE	       *fp;
	char *dmode_c;
	void *set = NULL;

	static struct passwd fakeuser =
	{
		"nouser",
		"*",
		-1,
		-1,
		0,
		"",
		"User &",
		"/nonexistent",
		"/bin/sh",
		0
#if defined(__FreeBSD__)
		,0
#endif
	};

	cnf = conf.userconf;

	if (mode == M_NEXT)
		return (pw_usernext(cnf, conf.quiet));

	if (mode == M_PRINT)
		return (pw_usershow(name, id, &fakeuser));

	if (mode == M_DELETE)
		return (pw_userdel(name, id));

	if (mode == M_LOCK || mode == M_UNLOCK)
		return (pw_userlock(name, id, mode));

	/*
	 * We can do all of the common legwork here
	 */

	if ((arg = getarg(args, 'b')) != NULL) {
		cnf->home = arg->val;
	}

	if ((arg = getarg(args, 'M')) != NULL) {
		dmode_c = arg->val;
		if ((set = setmode(dmode_c)) == NULL)
			errx(EX_DATAERR, "invalid directory creation mode '%s'",
			    dmode_c);
		cnf->homemode = getmode(set, _DEF_DIRMODE);
		free(set);
	}

	/*
	 * If we'll need to use it or we're updating it,
	 * then create the base home directory if necessary
	 */
	if (arg != NULL || getarg(args, 'm') != NULL) {
		int	l = strlen(cnf->home);

		if (l > 1 && cnf->home[l-1] == '/')	/* Shave off any trailing path delimiter */
			cnf->home[--l] = '\0';

		if (l < 2 || *cnf->home != '/')		/* Check for absolute path name */
			errx(EX_DATAERR, "invalid base directory for home '%s'", cnf->home);

		if (stat(cnf->home, &st) == -1) {
			char	dbuf[MAXPATHLEN];

			/*
			 * This is a kludge especially for Joerg :)
			 * If the home directory would be created in the root partition, then
			 * we really create it under /usr which is likely to have more space.
			 * But we create a symlink from cnf->home -> "/usr" -> cnf->home
			 */
			if (strchr(cnf->home+1, '/') == NULL) {
				snprintf(dbuf, MAXPATHLEN, "/usr%s", cnf->home);
				if (mkdir(dbuf, _DEF_DIRMODE) != -1 || errno == EEXIST) {
					chown(dbuf, 0, 0);
					/*
					 * Skip first "/" and create symlink:
					 * /home -> usr/home
					 */
					symlink(dbuf+1, cnf->home);
				}
				/* If this falls, fall back to old method */
			}
			strlcpy(dbuf, cnf->home, sizeof(dbuf));
			p = dbuf;
			if (stat(dbuf, &st) == -1) {
				while ((p = strchr(p + 1, '/')) != NULL) {
					*p = '\0';
					if (stat(dbuf, &st) == -1) {
						if (mkdir(dbuf, _DEF_DIRMODE) == -1)
							err(EX_OSFILE, "mkdir '%s'", dbuf);
						chown(dbuf, 0, 0);
					} else if (!S_ISDIR(st.st_mode))
						errx(EX_OSFILE, "'%s' (root home parent) is not a directory", dbuf);
					*p = '/';
				}
			}
			if (stat(dbuf, &st) == -1) {
				if (mkdir(dbuf, _DEF_DIRMODE) == -1)
					err(EX_OSFILE, "mkdir '%s'", dbuf);
				chown(dbuf, 0, 0);
			}
		} else if (!S_ISDIR(st.st_mode))
			errx(EX_OSFILE, "root home `%s' is not a directory", cnf->home);
	}

	if ((arg = getarg(args, 'e')) != NULL)
		cnf->expire_days = atoi(arg->val);

	if ((arg = getarg(args, 'y')) != NULL)
		cnf->nispasswd = arg->val;

	if ((arg = getarg(args, 'p')) != NULL && arg->val)
		cnf->password_days = atoi(arg->val);

	if ((arg = getarg(args, 'g')) != NULL) {
		if (!*(p = arg->val))	/* Handle empty group list specially */
			cnf->default_group = "";
		else {
			if ((grp = GETGRNAM(p)) == NULL) {
				if (!isdigit((unsigned char)*p) || (grp = GETGRGID((gid_t) atoi(p))) == NULL)
					errx(EX_NOUSER, "group `%s' does not exist", p);
			}
			cnf->default_group = newstr(grp->gr_name);
		}
	}
	if ((arg = getarg(args, 'L')) != NULL)
		cnf->default_class = pw_checkname(arg->val, 0);

	if ((arg = getarg(args, 'G')) != NULL && arg->val) {
		for (p = strtok(arg->val, ", \t"); p != NULL; p = strtok(NULL, ", \t")) {
			if ((grp = GETGRNAM(p)) == NULL) {
				if (!isdigit((unsigned char)*p) || (grp = GETGRGID((gid_t) atoi(p))) == NULL)
					errx(EX_NOUSER, "group `%s' does not exist", p);
			}
			sl_add(cnf->groups, newstr(grp->gr_name));
		}
	}

	if ((arg = getarg(args, 'k')) != NULL) {
		if (stat(cnf->dotdir = arg->val, &st) == -1 || !S_ISDIR(st.st_mode))
			errx(EX_OSFILE, "skeleton `%s' is not a directory or does not exist", cnf->dotdir);
	}

	if ((arg = getarg(args, 's')) != NULL)
		cnf->shell_default = arg->val;

	if ((arg = getarg(args, 'w')) != NULL)
		cnf->default_password = boolean_val(arg->val, cnf->default_password);
	if (mode == M_ADD && getarg(args, 'D')) {
		if (name != NULL)
			errx(EX_DATAERR, "can't combine `-D' with `-n name'");
		if ((arg = getarg(args, 'u')) != NULL && (p = strtok(arg->val, ", \t")) != NULL) {
			if ((cnf->min_uid = (uid_t) atoi(p)) == 0)
				cnf->min_uid = 1000;
			if ((p = strtok(NULL, " ,\t")) == NULL || (cnf->max_uid = (uid_t) atoi(p)) < cnf->min_uid)
				cnf->max_uid = 32000;
		}
		if ((arg = getarg(args, 'i')) != NULL && (p = strtok(arg->val, ", \t")) != NULL) {
			if ((cnf->min_gid = (gid_t) atoi(p)) == 0)
				cnf->min_gid = 1000;
			if ((p = strtok(NULL, " ,\t")) == NULL || (cnf->max_gid = (gid_t) atoi(p)) < cnf->min_gid)
				cnf->max_gid = 32000;
		}

		if (write_userconfig(conf.config))
			return (EXIT_SUCCESS);
		err(EX_IOERR, "config udpate");
	}

	if (name != NULL)
		pwd = GETPWNAM(pw_checkname(name, 0));

	if (id < 0 && name == NULL)
		errx(EX_DATAERR, "user name or id required");

	/*
	 * Update require that the user exists
	 */
	if (mode == M_UPDATE) {

		if (name == NULL && pwd == NULL)	/* Try harder */
			pwd = GETPWUID(id);

		if (pwd == NULL) {
			if (name == NULL)
				errx(EX_NOUSER, "no such uid `%ld'", id);
			errx(EX_NOUSER, "no such user `%s'", name);
		}

		if (name == NULL)
			name = pwd->pw_name;

		/*
		 * The rest is edit code
		 */
		if (conf.newname != NULL) {
			if (strcmp(pwd->pw_name, "root") == 0)
				errx(EX_DATAERR, "can't rename `root' account");
			pwd->pw_name = pw_checkname(conf.newname, 0);
			edited = 1;
		}

		if (id > 0 && isdigit((unsigned char)*arg->val)) {
			pwd->pw_uid = (uid_t)id;
			edited = 1;
			if (pwd->pw_uid != 0 && strcmp(pwd->pw_name, "root") == 0)
				errx(EX_DATAERR, "can't change uid of `root' account");
			if (pwd->pw_uid == 0 && strcmp(pwd->pw_name, "root") != 0)
				warnx("WARNING: account `%s' will have a uid of 0 (superuser access!)", pwd->pw_name);
		}

		if ((arg = getarg(args, 'g')) != NULL && pwd->pw_uid != 0) {	/* Already checked this */
			gid_t newgid = (gid_t) GETGRNAM(cnf->default_group)->gr_gid;
			if (newgid != pwd->pw_gid) {
				edited = 1;
				pwd->pw_gid = newgid;
			}
		}

		if ((arg = getarg(args, 'p')) != NULL) {
			if (*arg->val == '\0' || strcmp(arg->val, "0") == 0) {
				if (pwd->pw_change != 0) {
					pwd->pw_change = 0;
					edited = 1;
				}
			}
			else {
				time_t          now = time(NULL);
				time_t          expire = parse_date(now, arg->val);

				if (pwd->pw_change != expire) {
					pwd->pw_change = expire;
					edited = 1;
				}
			}
		}

		if ((arg = getarg(args, 'e')) != NULL) {
			if (*arg->val == '\0' || strcmp(arg->val, "0") == 0) {
				if (pwd->pw_expire != 0) {
					pwd->pw_expire = 0;
					edited = 1;
				}
			}
			else {
				time_t          now = time(NULL);
				time_t          expire = parse_date(now, arg->val);

				if (pwd->pw_expire != expire) {
					pwd->pw_expire = expire;
					edited = 1;
				}
			}
		}

		if ((arg = getarg(args, 's')) != NULL) {
			char *shell = shell_path(cnf->shelldir, cnf->shells, arg->val);
			if (shell == NULL)
				shell = "";
			if (strcmp(shell, pwd->pw_shell) != 0) {
				pwd->pw_shell = shell;
				edited = 1;
			}
		}

		if (getarg(args, 'L')) {
			if (cnf->default_class == NULL)
				cnf->default_class = "";
			if (strcmp(pwd->pw_class, cnf->default_class) != 0) {
				pwd->pw_class = cnf->default_class;
				edited = 1;
			}
		}

		if ((arg  = getarg(args, 'd')) != NULL) {
			if (strcmp(pwd->pw_dir, arg->val))
				edited = 1;
			if (stat(pwd->pw_dir = arg->val, &st) == -1) {
				if (getarg(args, 'm') == NULL && strcmp(pwd->pw_dir, "/nonexistent") != 0)
				  warnx("WARNING: home `%s' does not exist", pwd->pw_dir);
			} else if (!S_ISDIR(st.st_mode))
				warnx("WARNING: home `%s' is not a directory", pwd->pw_dir);
		}

		if ((arg = getarg(args, 'w')) != NULL && conf.fd == -1) {
			login_cap_t *lc;

			lc = login_getpwclass(pwd);
			if (lc == NULL ||
			    login_setcryptfmt(lc, "sha512", NULL) == NULL)
				warn("setting crypt(3) format");
			login_close(lc);
			pwd->pw_passwd = pw_password(cnf, pwd->pw_name);
			edited = 1;
		}

	} else {
		login_cap_t *lc;

		/*
		 * Add code
		 */

		if (name == NULL)	/* Required */
			errx(EX_DATAERR, "login name required");
		else if ((pwd = GETPWNAM(name)) != NULL)	/* Exists */
			errx(EX_DATAERR, "login name `%s' already exists", name);

		/*
		 * Now, set up defaults for a new user
		 */
		pwd = &fakeuser;
		pwd->pw_name = name;
		pwd->pw_class = cnf->default_class ? cnf->default_class : "";
		pwd->pw_uid = pw_uidpolicy(cnf, id);
		pwd->pw_gid = pw_gidpolicy(args, pwd->pw_name, (gid_t) pwd->pw_uid);
		pwd->pw_change = pw_pwdpolicy(cnf, args);
		pwd->pw_expire = pw_exppolicy(cnf, args);
		pwd->pw_dir = pw_homepolicy(cnf, args, pwd->pw_name);
		pwd->pw_shell = pw_shellpolicy(cnf, args, NULL);
		lc = login_getpwclass(pwd);
		if (lc == NULL || login_setcryptfmt(lc, "sha512", NULL) == NULL)
			warn("setting crypt(3) format");
		login_close(lc);
		pwd->pw_passwd = pw_password(cnf, pwd->pw_name);
		edited = 1;

		if (pwd->pw_uid == 0 && strcmp(pwd->pw_name, "root") != 0)
			warnx("WARNING: new account `%s' has a uid of 0 (superuser access!)", pwd->pw_name);
	}

	/*
	 * Shared add/edit code
	 */
	if (conf.gecos != NULL) {
		if (strcmp(pwd->pw_gecos, conf.gecos) != 0) {
			pwd->pw_gecos = conf.gecos;
			edited = 1;
		}
	}

	if (conf.fd != -1)
		edited = set_passwd(pwd, mode == M_UPDATE);

	/*
	 * Special case: -N only displays & exits
	 */
	if (conf.dryrun)
		return print_user(pwd);

	if (mode == M_ADD) {
		edited = 1;	/* Always */
		rc = addpwent(pwd);
		if (rc == -1)
			errx(EX_IOERR, "user '%s' already exists",
			    pwd->pw_name);
		else if (rc != 0)
			err(EX_IOERR, "passwd file update");
		if (cnf->nispasswd && *cnf->nispasswd=='/') {
			rc = addnispwent(cnf->nispasswd, pwd);
			if (rc == -1)
				warnx("User '%s' already exists in NIS passwd", pwd->pw_name);
			else
				warn("NIS passwd update");
			/* NOTE: we treat NIS-only update errors as non-fatal */
		}
	} else if (mode == M_UPDATE && edited) /* Only updated this if required */
		perform_chgpwent(name, pwd);

	/*
	 * Ok, user is created or changed - now edit group file
	 */

	if (mode == M_ADD || getarg(args, 'G') != NULL) {
		int j;
		size_t i;
		/* First remove the user from all group */
		SETGRENT();
		while ((grp = GETGRENT()) != NULL) {
			char group[MAXLOGNAME];
			if (grp->gr_mem == NULL)
				continue;
			for (i = 0; grp->gr_mem[i] != NULL; i++) {
				if (strcmp(grp->gr_mem[i] , pwd->pw_name) != 0)
					continue;
				for (j = i; grp->gr_mem[j] != NULL ; j++)
					grp->gr_mem[j] = grp->gr_mem[j+1];
				strlcpy(group, grp->gr_name, MAXLOGNAME);
				chggrent(group, grp);
			}
		}
		ENDGRENT();

		/* now add to group where needed */
		for (i = 0; i < cnf->groups->sl_cur; i++) {
			grp = GETGRNAM(cnf->groups->sl_str[i]);
			grp = gr_add(grp, pwd->pw_name);
			/*
			 * grp can only be NULL in 2 cases:
			 * - the new member is already a member
			 * - a problem with memory occurs
			 * in both cases we want to skip now.
			 */
			if (grp == NULL)
				continue;
			chggrent(grp->gr_name, grp);
			free(grp);
		}
	}


	/* go get a current version of pwd */
	pwd = GETPWNAM(name);
	if (pwd == NULL) {
		/* This will fail when we rename, so special case that */
		if (mode == M_UPDATE && conf.newname != NULL) {
			name = conf.newname;		/* update new name */
			pwd = GETPWNAM(name);	/* refetch renamed rec */
		}
	}
	if (pwd == NULL)	/* can't go on without this */
		errx(EX_NOUSER, "user '%s' disappeared during update", name);

	grp = GETGRGID(pwd->pw_gid);
	pw_log(cnf, mode, W_USER, "%s(%u):%s(%u):%s:%s:%s",
	       pwd->pw_name, pwd->pw_uid,
	    grp ? grp->gr_name : "unknown", (grp ? grp->gr_gid : (uid_t)-1),
	       pwd->pw_gecos, pwd->pw_dir, pwd->pw_shell);

	/*
	 * If adding, let's touch and chown the user's mail file. This is not
	 * strictly necessary under BSD with a 0755 maildir but it also
	 * doesn't hurt anything to create the empty mailfile
	 */
	if (mode == M_ADD) {
		if (PWALTDIR() != PWF_ALT) {
			snprintf(path, sizeof(path), "%s/%s", _PATH_MAILDIR,
			    pwd->pw_name);
			close(openat(conf.rootfd, path +1, O_RDWR | O_CREAT,
			    0600));	/* Preserve contents & mtime */
			fchownat(conf.rootfd, path + 1, pwd->pw_uid,
			    pwd->pw_gid, AT_SYMLINK_NOFOLLOW);
		}
	}

	/*
	 * Let's create and populate the user's home directory. Note
	 * that this also `works' for editing users if -m is used, but
	 * existing files will *not* be overwritten.
	 */
	if (PWALTDIR() != PWF_ALT && getarg(args, 'm') != NULL && pwd->pw_dir &&
	    *pwd->pw_dir == '/' && pwd->pw_dir[1])
		create_and_populate_homedir(pwd);

	/*
	 * Finally, send mail to the new user as well, if we are asked to
	 */
	if (mode == M_ADD && !PWALTDIR() && cnf->newmail && *cnf->newmail && (fp = fopen(cnf->newmail, "r")) != NULL) {
		FILE           *pfp = popen(_PATH_SENDMAIL " -t", "w");
		
		if (pfp == NULL)
			warn("sendmail");
		else {
			fprintf(pfp, "From: root\n" "To: %s\n" "Subject: Welcome!\n\n", pwd->pw_name);
			while (fgets(line, sizeof(line), fp) != NULL) {
				/* Do substitutions? */
				fputs(line, pfp);
			}
			pclose(pfp);
			pw_log(cnf, mode, W_USER, "%s(%u) new user mail sent",
			    pwd->pw_name, pwd->pw_uid);
		}
		fclose(fp);
	}

	return EXIT_SUCCESS;
}


static          uid_t
pw_uidpolicy(struct userconf * cnf, long id)
{
	struct passwd  *pwd;
	uid_t           uid = (uid_t) - 1;

	/*
	 * Check the given uid, if any
	 */
	if (id > 0) {
		uid = (uid_t) id;

		if ((pwd = GETPWUID(uid)) != NULL && conf.checkduplicate)
			errx(EX_DATAERR, "uid `%u' has already been allocated", pwd->pw_uid);
	} else {
		struct bitmap   bm;

		/*
		 * We need to allocate the next available uid under one of
		 * two policies a) Grab the first unused uid b) Grab the
		 * highest possible unused uid
		 */
		if (cnf->min_uid >= cnf->max_uid) {	/* Sanity
							 * claus^H^H^H^Hheck */
			cnf->min_uid = 1000;
			cnf->max_uid = 32000;
		}
		bm = bm_alloc(cnf->max_uid - cnf->min_uid + 1);

		/*
		 * Now, let's fill the bitmap from the password file
		 */
		SETPWENT();
		while ((pwd = GETPWENT()) != NULL)
			if (pwd->pw_uid >= (uid_t) cnf->min_uid && pwd->pw_uid <= (uid_t) cnf->max_uid)
				bm_setbit(&bm, pwd->pw_uid - cnf->min_uid);
		ENDPWENT();

		/*
		 * Then apply the policy, with fallback to reuse if necessary
		 */
		if (cnf->reuse_uids || (uid = (uid_t) (bm_lastset(&bm) + cnf->min_uid + 1)) > cnf->max_uid)
			uid = (uid_t) (bm_firstunset(&bm) + cnf->min_uid);

		/*
		 * Another sanity check
		 */
		if (uid < cnf->min_uid || uid > cnf->max_uid)
			errx(EX_SOFTWARE, "unable to allocate a new uid - range fully used");
		bm_dealloc(&bm);
	}
	return uid;
}


static          uid_t
pw_gidpolicy(struct cargs * args, char *nam, gid_t prefer)
{
	struct group   *grp;
	gid_t           gid = (uid_t) - 1;
	struct carg    *a_gid = getarg(args, 'g');
	struct userconf	*cnf = conf.userconf;

	/*
	 * If no arg given, see if default can help out
	 */
	if (a_gid == NULL && cnf->default_group && *cnf->default_group)
		a_gid = addarg(args, 'g', cnf->default_group);

	/*
	 * Check the given gid, if any
	 */
	SETGRENT();
	if (a_gid != NULL) {
		if ((grp = GETGRNAM(a_gid->val)) == NULL) {
			gid = (gid_t) atol(a_gid->val);
			if ((gid == 0 && !isdigit((unsigned char)*a_gid->val)) || (grp = GETGRGID(gid)) == NULL)
				errx(EX_NOUSER, "group `%s' is not defined", a_gid->val);
		}
		gid = grp->gr_gid;
	} else if ((grp = GETGRNAM(nam)) != NULL &&
	    (grp->gr_mem == NULL || grp->gr_mem[0] == NULL)) {
		gid = grp->gr_gid;  /* Already created? Use it anyway... */
	} else {
		gid_t		grid = -1;

		/*
		 * We need to auto-create a group with the user's name. We
		 * can send all the appropriate output to our sister routine
		 * bit first see if we can create a group with gid==uid so we
		 * can keep the user and group ids in sync. We purposely do
		 * NOT check the gid range if we can force the sync. If the
		 * user's name dups an existing group, then the group add
		 * function will happily handle that case for us and exit.
		 */
		if (GETGRGID(prefer) == NULL)
			grid = prefer;
		if (conf.dryrun) {
			gid = pw_groupnext(cnf, true);
		} else {
			pw_group(M_ADD, nam, grid, NULL);
			if ((grp = GETGRNAM(nam)) != NULL)
				gid = grp->gr_gid;
		}
	}
	ENDGRENT();
	return gid;
}


static          time_t
pw_pwdpolicy(struct userconf * cnf, struct cargs * args)
{
	time_t          result = 0;
	time_t          now = time(NULL);
	struct carg    *arg = getarg(args, 'p');

	if (arg != NULL) {
		if ((result = parse_date(now, arg->val)) == now)
			errx(EX_DATAERR, "invalid date/time `%s'", arg->val);
	} else if (cnf->password_days > 0)
		result = now + ((long) cnf->password_days * 86400L);
	return result;
}


static          time_t
pw_exppolicy(struct userconf * cnf, struct cargs * args)
{
	time_t          result = 0;
	time_t          now = time(NULL);
	struct carg    *arg = getarg(args, 'e');

	if (arg != NULL) {
		if ((result = parse_date(now, arg->val)) == now)
			errx(EX_DATAERR, "invalid date/time `%s'", arg->val);
	} else if (cnf->expire_days > 0)
		result = now + ((long) cnf->expire_days * 86400L);
	return result;
}


static char    *
pw_homepolicy(struct userconf * cnf, struct cargs * args, char const * user)
{
	struct carg    *arg = getarg(args, 'd');
	static char     home[128];

	if (arg)
		return (arg->val);

	if (cnf->home == NULL || *cnf->home == '\0')
		errx(EX_CONFIG, "no base home directory set");
	snprintf(home, sizeof(home), "%s/%s", cnf->home, user);

	return (home);
}

static char    *
shell_path(char const * path, char *shells[], char *sh)
{
	if (sh != NULL && (*sh == '/' || *sh == '\0'))
		return sh;	/* specified full path or forced none */
	else {
		char           *p;
		char            paths[_UC_MAXLINE];

		/*
		 * We need to search paths
		 */
		strlcpy(paths, path, sizeof(paths));
		for (p = strtok(paths, ": \t\r\n"); p != NULL; p = strtok(NULL, ": \t\r\n")) {
			int             i;
			static char     shellpath[256];

			if (sh != NULL) {
				snprintf(shellpath, sizeof(shellpath), "%s/%s", p, sh);
				if (access(shellpath, X_OK) == 0)
					return shellpath;
			} else
				for (i = 0; i < _UC_MAXSHELLS && shells[i] != NULL; i++) {
					snprintf(shellpath, sizeof(shellpath), "%s/%s", p, shells[i]);
					if (access(shellpath, X_OK) == 0)
						return shellpath;
				}
		}
		if (sh == NULL)
			errx(EX_OSFILE, "can't find shell `%s' in shell paths", sh);
		errx(EX_CONFIG, "no default shell available or defined");
		return NULL;
	}
}


static char    *
pw_shellpolicy(struct userconf * cnf, struct cargs * args, char *newshell)
{
	char           *sh = newshell;
	struct carg    *arg = getarg(args, 's');

	if (newshell == NULL && arg != NULL)
		sh = arg->val;
	return shell_path(cnf->shelldir, cnf->shells, sh ? sh : cnf->shell_default);
}

#define	SALTSIZE	32

static char const chars[] = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ./";

char           *
pw_pwcrypt(char *password)
{
	int             i;
	char            salt[SALTSIZE + 1];
	char		*cryptpw;

	static char     buf[256];

	/*
	 * Calculate a salt value
	 */
	for (i = 0; i < SALTSIZE; i++)
		salt[i] = chars[arc4random_uniform(sizeof(chars) - 1)];
	salt[SALTSIZE] = '\0';

	cryptpw = crypt(password, salt);
	if (cryptpw == NULL)
		errx(EX_CONFIG, "crypt(3) failure");
	return strcpy(buf, cryptpw);
}


static char    *
pw_password(struct userconf * cnf, char const * user)
{
	int             i, l;
	char            pwbuf[32];

	switch (cnf->default_password) {
	case -1:		/* Random password */
		l = (arc4random() % 8 + 8);	/* 8 - 16 chars */
		for (i = 0; i < l; i++)
			pwbuf[i] = chars[arc4random_uniform(sizeof(chars)-1)];
		pwbuf[i] = '\0';

		/*
		 * We give this information back to the user
		 */
		if (conf.fd == -1 && !conf.dryrun) {
			if (isatty(STDOUT_FILENO))
				printf("Password for '%s' is: ", user);
			printf("%s\n", pwbuf);
			fflush(stdout);
		}
		break;

	case -2:		/* No password at all! */
		return "";

	case 0:		/* No login - default */
	default:
		return "*";

	case 1:		/* user's name */
		strlcpy(pwbuf, user, sizeof(pwbuf));
		break;
	}
	return pw_pwcrypt(pwbuf);
}

static int
pw_userdel(char *name, long id)
{
	struct passwd *pwd = NULL;
	char		 file[MAXPATHLEN];
	char		 home[MAXPATHLEN];
	uid_t		 uid;
	struct group	*gr, *grp;
	char		 grname[LOGNAMESIZE];
	int		 rc;
	struct stat	 st;

	if (id < 0 && name == NULL)
		errx(EX_DATAERR, "username or id required");

	pwd = (name != NULL) ? GETPWNAM(pw_checkname(name, 0)) : GETPWUID(id);
	if (pwd == NULL) {
		if (name == NULL)
			errx(EX_NOUSER, "no such uid `%ld'", id);
		errx(EX_NOUSER, "no such user `%s'", name);
	}
	uid = pwd->pw_uid;
	if (name == NULL)
		name = pwd->pw_name;

	if (strcmp(pwd->pw_name, "root") == 0)
		errx(EX_DATAERR, "cannot remove user 'root'");

		/* Remove opie record from /etc/opiekeys */

	if (PWALTDIR() != PWF_ALT)
		rmopie(pwd->pw_name);

	if (!PWALTDIR()) {
		/* Remove crontabs */
		snprintf(file, sizeof(file), "/var/cron/tabs/%s", pwd->pw_name);
		if (access(file, F_OK) == 0) {
			snprintf(file, sizeof(file), "crontab -u %s -r", pwd->pw_name);
			system(file);
		}
	}
	/*
	 * Save these for later, since contents of pwd may be
	 * invalidated by deletion
	 */
	snprintf(file, sizeof(file), "%s/%s", _PATH_MAILDIR, pwd->pw_name);
	strlcpy(home, pwd->pw_dir, sizeof(home));
	gr = GETGRGID(pwd->pw_gid);
	if (gr != NULL)
		strlcpy(grname, gr->gr_name, LOGNAMESIZE);
	else
		grname[0] = '\0';

	rc = delpwent(pwd);
	if (rc == -1)
		err(EX_IOERR, "user '%s' does not exist", pwd->pw_name);
	else if (rc != 0)
		err(EX_IOERR, "passwd update");

	if (conf.userconf->nispasswd && *conf.userconf->nispasswd=='/') {
		rc = delnispwent(conf.userconf->nispasswd, name);
		if (rc == -1)
			warnx("WARNING: user '%s' does not exist in NIS passwd",
			    pwd->pw_name);
		else if (rc != 0)
			warn("WARNING: NIS passwd update");
		/* non-fatal */
	}

	grp = GETGRNAM(name);
	if (grp != NULL &&
	    (grp->gr_mem == NULL || *grp->gr_mem == NULL) &&
	    strcmp(name, grname) == 0)
		delgrent(GETGRNAM(name));
	SETGRENT();
	while ((grp = GETGRENT()) != NULL) {
		int i, j;
		char group[MAXLOGNAME];
		if (grp->gr_mem == NULL)
			continue;

		for (i = 0; grp->gr_mem[i] != NULL; i++) {
			if (strcmp(grp->gr_mem[i], name) != 0)
				continue;

			for (j = i; grp->gr_mem[j] != NULL; j++)
				grp->gr_mem[j] = grp->gr_mem[j+1];
			strlcpy(group, grp->gr_name, MAXLOGNAME);
			chggrent(group, grp);
		}
	}
	ENDGRENT();

	pw_log(conf.userconf, M_DELETE, W_USER, "%s(%u) account removed", name,
	    uid);

	/* Remove mail file */
	if (PWALTDIR() != PWF_ALT)
		unlinkat(conf.rootfd, file + 1, 0);

		/* Remove at jobs */
	if (!PWALTDIR() && getpwuid(uid) == NULL)
		rmat(uid);

	/* Remove home directory and contents */
	if (PWALTDIR() != PWF_ALT && conf.deletehome && *home == '/' &&
	    getpwuid(uid) == NULL &&
	    fstatat(conf.rootfd, home + 1, &st, 0) != -1) {
		rm_r(conf.rootfd, home, uid);
		pw_log(conf.userconf, M_DELETE, W_USER, "%s(%u) home '%s' %s"
		    "removed", name, uid, home,
		     fstatat(conf.rootfd, home + 1, &st, 0) == -1 ? "" : "not "
		     "completely ");
	}

	return (EXIT_SUCCESS);
}

static int
print_user(struct passwd * pwd)
{
	if (!conf.pretty) {
		char            *buf;

		buf = conf.v7 ? pw_make_v7(pwd) : pw_make(pwd);
		printf("%s\n", buf);
		free(buf);
	} else {
		int		j;
		char           *p;
		struct group   *grp = GETGRGID(pwd->pw_gid);
		char            uname[60] = "User &", office[60] = "[None]",
		                wphone[60] = "[None]", hphone[60] = "[None]";
		char		acexpire[32] = "[None]", pwexpire[32] = "[None]";
		struct tm *    tptr;

		if ((p = strtok(pwd->pw_gecos, ",")) != NULL) {
			strlcpy(uname, p, sizeof(uname));
			if ((p = strtok(NULL, ",")) != NULL) {
				strlcpy(office, p, sizeof(office));
				if ((p = strtok(NULL, ",")) != NULL) {
					strlcpy(wphone, p, sizeof(wphone));
					if ((p = strtok(NULL, "")) != NULL) {
						strlcpy(hphone, p,
						    sizeof(hphone));
					}
				}
			}
		}
		/*
		 * Handle '&' in gecos field
		 */
		if ((p = strchr(uname, '&')) != NULL) {
			int             l = strlen(pwd->pw_name);
			int             m = strlen(p);

			memmove(p + l, p + 1, m);
			memmove(p, pwd->pw_name, l);
			*p = (char) toupper((unsigned char)*p);
		}
		if (pwd->pw_expire > (time_t)0 && (tptr = localtime(&pwd->pw_expire)) != NULL)
			strftime(acexpire, sizeof acexpire, "%c", tptr);
		if (pwd->pw_change > (time_t)0 && (tptr = localtime(&pwd->pw_change)) != NULL)
			strftime(pwexpire, sizeof pwexpire, "%c", tptr);
		printf("Login Name: %-15s   #%-12u Group: %-15s   #%u\n"
		       " Full Name: %s\n"
		       "      Home: %-26.26s      Class: %s\n"
		       "     Shell: %-26.26s     Office: %s\n"
		       "Work Phone: %-26.26s Home Phone: %s\n"
		       "Acc Expire: %-26.26s Pwd Expire: %s\n",
		       pwd->pw_name, pwd->pw_uid,
		       grp ? grp->gr_name : "(invalid)", pwd->pw_gid,
		       uname, pwd->pw_dir, pwd->pw_class,
		       pwd->pw_shell, office, wphone, hphone,
		       acexpire, pwexpire);
	        SETGRENT();
		j = 0;
		while ((grp=GETGRENT()) != NULL)
		{
			int     i = 0;
			if (grp->gr_mem != NULL) {
				while (grp->gr_mem[i] != NULL)
				{
					if (strcmp(grp->gr_mem[i], pwd->pw_name)==0)
					{
						printf(j++ == 0 ? "    Groups: %s" : ",%s", grp->gr_name);
						break;
					}
					++i;
				}
			}
		}
		ENDGRENT();
		printf("%s", j ? "\n" : "");
	}
	return EXIT_SUCCESS;
}

char *
pw_checkname(char *name, int gecos)
{
	char showch[8];
	const char *badchars, *ch, *showtype;
	int reject;

	ch = name;
	reject = 0;
	if (gecos) {
		/* See if the name is valid as a gecos (comment) field. */
		badchars = ":!@";
		showtype = "gecos field";
	} else {
		/* See if the name is valid as a userid or group. */
		badchars = " ,\t:+&#%$^()!@~*?<>=|\\/\"";
		showtype = "userid/group name";
		/* Userids and groups can not have a leading '-'. */
		if (*ch == '-')
			reject = 1;
	}
	if (!reject) {
		while (*ch) {
			if (strchr(badchars, *ch) != NULL || *ch < ' ' ||
			    *ch == 127) {
				reject = 1;
				break;
			}
			/* 8-bit characters are only allowed in GECOS fields */
			if (!gecos && (*ch & 0x80)) {
				reject = 1;
				break;
			}
			ch++;
		}
	}
	/*
	 * A `$' is allowed as the final character for userids and groups,
	 * mainly for the benefit of samba.
	 */
	if (reject && !gecos) {
		if (*ch == '$' && *(ch + 1) == '\0') {
			reject = 0;
			ch++;
		}
	}
	if (reject) {
		snprintf(showch, sizeof(showch), (*ch >= ' ' && *ch < 127)
		    ? "`%c'" : "0x%02x", *ch);
		errx(EX_DATAERR, "invalid character %s at position %td in %s",
		    showch, (ch - name), showtype);
	}
	if (!gecos && (ch - name) > LOGNAMESIZE)
		errx(EX_DATAERR, "name too long `%s' (max is %d)", name,
		    LOGNAMESIZE);

	return (name);
}


static void
rmat(uid_t uid)
{
	DIR            *d = opendir("/var/at/jobs");

	if (d != NULL) {
		struct dirent  *e;

		while ((e = readdir(d)) != NULL) {
			struct stat     st;

			if (strncmp(e->d_name, ".lock", 5) != 0 &&
			    stat(e->d_name, &st) == 0 &&
			    !S_ISDIR(st.st_mode) &&
			    st.st_uid == uid) {
				char            tmp[MAXPATHLEN];

				snprintf(tmp, sizeof(tmp), "/usr/bin/atrm %s", e->d_name);
				system(tmp);
			}
		}
		closedir(d);
	}
}

static void
rmopie(char const * name)
{
	char tmp[1014];
	FILE *fp;
	int fd;
	size_t len;
	off_t	atofs = 0;
	
	if ((fd = openat(conf.rootfd, "etc/opiekeys", O_RDWR)) == -1)
		return;

	fp = fdopen(fd, "r+");
	len = strlen(name);

	while (fgets(tmp, sizeof(tmp), fp) != NULL) {
		if (strncmp(name, tmp, len) == 0 && tmp[len]==' ') {
			/* Comment username out */
			if (fseek(fp, atofs, SEEK_SET) == 0)
				fwrite("#", 1, 1, fp);
			break;
		}
		atofs = ftell(fp);
	}
	/*
	 * If we got an error of any sort, don't update!
	 */
	fclose(fp);
}
