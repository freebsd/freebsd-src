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
#include <unistd.h>
#include <utmp.h>
#include <login_cap.h>
#if defined(USE_MD5RAND)
#include <md5.h>
#endif
#include "pw.h"
#include "bitmap.h"

#if (MAXLOGNAME-1) > UT_NAMESIZE
#define LOGNAMESIZE UT_NAMESIZE
#else
#define LOGNAMESIZE (MAXLOGNAME-1)
#endif

static		char locked_str[] = "*LOCKED*";

static int      print_user(struct passwd * pwd, int pretty, int v7);
static uid_t    pw_uidpolicy(struct userconf * cnf, struct cargs * args);
static uid_t    pw_gidpolicy(struct userconf * cnf, struct cargs * args, char *nam, gid_t prefer);
static time_t   pw_pwdpolicy(struct userconf * cnf, struct cargs * args);
static time_t   pw_exppolicy(struct userconf * cnf, struct cargs * args);
static char    *pw_homepolicy(struct userconf * cnf, struct cargs * args, char const * user);
static char    *pw_shellpolicy(struct userconf * cnf, struct cargs * args, char *newshell);
static char    *pw_password(struct userconf * cnf, struct cargs * args, char const * user);
static char    *shell_path(char const * path, char *shells[], char *sh);
static void     rmat(uid_t uid);
static void	rmskey(char const * name);

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
pw_user(struct userconf * cnf, int mode, struct cargs * args)
{
	int	        rc, edited = 0;
	char           *p = NULL;
	char					 *passtmp;
	struct carg    *a_name;
	struct carg    *a_uid;
	struct carg    *arg;
	struct passwd  *pwd = NULL;
	struct group   *grp;
	struct stat     st;
	char            line[_PASSWORD_LEN+1];
	FILE	       *fp;

	static struct passwd fakeuser =
	{
		NULL,
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


	/*
	 * With M_NEXT, we only need to return the
	 * next uid to stdout
	 */
	if (mode == M_NEXT)
	{
		uid_t next = pw_uidpolicy(cnf, args);
		if (getarg(args, 'q'))
			return next;
		printf("%ld:", (long)next);
		pw_group(cnf, mode, args);
		return EXIT_SUCCESS;
	}

	/*
	 * We can do all of the common legwork here
	 */

	if ((arg = getarg(args, 'b')) != NULL) {
		cnf->home = arg->val;
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
				strcpy(dbuf, "/usr");
				strncat(dbuf, cnf->home, MAXPATHLEN-5);
				if (mkdir(dbuf, 0755) != -1 || errno == EEXIST) {
					chown(dbuf, 0, 0);
					symlink(dbuf, cnf->home);
				}
				/* If this falls, fall back to old method */
			}
			p = strncpy(dbuf, cnf->home, sizeof dbuf);
			dbuf[MAXPATHLEN-1] = '\0';
			if (stat(dbuf, &st) == -1) {
				while ((p = strchr(++p, '/')) != NULL) {
					*p = '\0';
					if (stat(dbuf, &st) == -1) {
						if (mkdir(dbuf, 0755) == -1)
							goto direrr;
						chown(dbuf, 0, 0);
					} else if (!S_ISDIR(st.st_mode))
						errx(EX_OSFILE, "'%s' (root home parent) is not a directory", dbuf);
					*p = '/';
				}
			}
			if (stat(dbuf, &st) == -1) {
				if (mkdir(dbuf, 0755) == -1) {
				direrr:	err(EX_OSFILE, "mkdir '%s'", dbuf);
				}
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
		cnf->default_class = pw_checkname((u_char *)arg->val, 0);

	if ((arg = getarg(args, 'G')) != NULL && arg->val) {
		int i = 0;

		for (p = strtok(arg->val, ", \t"); p != NULL; p = strtok(NULL, ", \t")) {
			if ((grp = GETGRNAM(p)) == NULL) {
				if (!isdigit((unsigned char)*p) || (grp = GETGRGID((gid_t) atoi(p))) == NULL)
					errx(EX_NOUSER, "group `%s' does not exist", p);
			}
			if (extendarray(&cnf->groups, &cnf->numgroups, i + 2) != -1)
				cnf->groups[i++] = newstr(grp->gr_name);
		}
		while (i < cnf->numgroups)
			cnf->groups[i++] = NULL;
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
		if (getarg(args, 'n') != NULL)
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

		arg = getarg(args, 'C');
		if (write_userconfig(arg ? arg->val : NULL))
			return EXIT_SUCCESS;
		warn("config update");
		return EX_IOERR;
	}

	if (mode == M_PRINT && getarg(args, 'a')) {
		int             pretty = getarg(args, 'P') != NULL;
		int		v7 = getarg(args, '7') != NULL;

		SETPWENT();
		while ((pwd = GETPWENT()) != NULL)
			print_user(pwd, pretty, v7);
		ENDPWENT();
		return EXIT_SUCCESS;
	}

	if ((a_name = getarg(args, 'n')) != NULL)
		pwd = GETPWNAM(pw_checkname((u_char *)a_name->val, 0));
	a_uid = getarg(args, 'u');

	if (a_uid == NULL) {
		if (a_name == NULL)
			errx(EX_DATAERR, "user name or id required");

		/*
		 * Determine whether 'n' switch is name or uid - we don't
		 * really don't really care which we have, but we need to
		 * know.
		 */
		if (mode != M_ADD && pwd == NULL
		    && strspn(a_name->val, "0123456789") == strlen(a_name->val)
		    && atoi(a_name->val) > 0) {	/* Assume uid */
			(a_uid = a_name)->ch = 'u';
			a_name = NULL;
		}
	}

	/*
	 * Update, delete & print require that the user exists
	 */
	if (mode == M_UPDATE || mode == M_DELETE ||
	    mode == M_PRINT  || mode == M_LOCK   || mode == M_UNLOCK) {

		if (a_name == NULL && pwd == NULL)	/* Try harder */
			pwd = GETPWUID(atoi(a_uid->val));

		if (pwd == NULL) {
			if (mode == M_PRINT && getarg(args, 'F')) {
				fakeuser.pw_name = a_name ? a_name->val : "nouser";
				fakeuser.pw_uid = a_uid ? (uid_t) atol(a_uid->val) : -1;
				return print_user(&fakeuser,
						  getarg(args, 'P') != NULL,
						  getarg(args, '7') != NULL);
			}
			if (a_name == NULL)
				errx(EX_NOUSER, "no such uid `%s'", a_uid->val);
			errx(EX_NOUSER, "no such user `%s'", a_name->val);
		}

		if (a_name == NULL)	/* May be needed later */
			a_name = addarg(args, 'n', newstr(pwd->pw_name));

		/*
		 * The M_LOCK and M_UNLOCK functions simply add or remove
		 * a "*LOCKED*" prefix from in front of the password to
		 * prevent it decoding correctly, and therefore prevents
		 * access. Of course, this only prevents access via
		 * password authentication (not ssh, kerberos or any
		 * other method that does not use the UNIX password) but
		 * that is a known limitation.
		 */

		if (mode == M_LOCK) {
			if (strncmp(pwd->pw_passwd, locked_str, sizeof(locked_str)-1) == 0)
				errx(EX_DATAERR, "user '%s' is already locked", pwd->pw_name);
			passtmp = malloc(strlen(pwd->pw_passwd) + sizeof(locked_str));
			if (passtmp == NULL)	/* disaster */
				errx(EX_UNAVAILABLE, "out of memory");
			strcpy(passtmp, locked_str);
			strcat(passtmp, pwd->pw_passwd);
			pwd->pw_passwd = passtmp;
			edited = 1;
		} else if (mode == M_UNLOCK) {
			if (strncmp(pwd->pw_passwd, locked_str, sizeof(locked_str)-1) != 0)
				errx(EX_DATAERR, "user '%s' is not locked", pwd->pw_name);
			pwd->pw_passwd += sizeof(locked_str)-1;
			edited = 1;
		} else if (mode == M_DELETE) {
			/*
			 * Handle deletions now
			 */
			char            file[MAXPATHLEN];
			char            home[MAXPATHLEN];
			uid_t           uid = pwd->pw_uid;

			if (strcmp(pwd->pw_name, "root") == 0)
				errx(EX_DATAERR, "cannot remove user 'root'");

			if (!PWALTDIR()) {
				/*
				 * Remove skey record from /etc/skeykeys
		        	 */

				rmskey(pwd->pw_name);

				/*
				 * Remove crontabs
				 */
				sprintf(file, "/var/cron/tabs/%s", pwd->pw_name);
				if (access(file, F_OK) == 0) {
					sprintf(file, "crontab -u %s -r", pwd->pw_name);
					system(file);
				}
			}
			/*
			 * Save these for later, since contents of pwd may be
			 * invalidated by deletion
			 */
			sprintf(file, "%s/%s", _PATH_MAILDIR, pwd->pw_name);
			strncpy(home, pwd->pw_dir, sizeof home);
			home[sizeof home - 1] = '\0';

			rc = delpwent(pwd);
			if (rc == -1)
				err(EX_IOERR, "user '%s' does not exist", pwd->pw_name);
			else if (rc != 0) {
				warn("passwd update");
				return EX_IOERR;
			}

			if (cnf->nispasswd && *cnf->nispasswd=='/') {
				rc = delnispwent(cnf->nispasswd, a_name->val);
				if (rc == -1)
					warnx("WARNING: user '%s' does not exist in NIS passwd", pwd->pw_name);
				else if (rc != 0)
					warn("WARNING: NIS passwd update");
				/* non-fatal */
			}

			editgroups(a_name->val, NULL);

			pw_log(cnf, mode, W_USER, "%s(%ld) account removed", a_name->val, (long) uid);

			if (!PWALTDIR()) {
				/*
				 * Remove mail file
				 */
				remove(file);

				/*
				 * Remove at jobs
				 */
				if (getpwuid(uid) == NULL)
					rmat(uid);

				/*
				 * Remove home directory and contents
				 */
				if (getarg(args, 'r') != NULL && *home == '/' && getpwuid(uid) == NULL) {
					if (stat(home, &st) != -1) {
						rm_r(home, uid);
						pw_log(cnf, mode, W_USER, "%s(%ld) home '%s' %sremoved",
						       a_name->val, (long) uid, home,
						       stat(home, &st) == -1 ? "" : "not completely ");
					}
				}
			}
			return EXIT_SUCCESS;
		} else if (mode == M_PRINT)
			return print_user(pwd,
					  getarg(args, 'P') != NULL,
					  getarg(args, '7') != NULL);

		/*
		 * The rest is edit code
		 */
		if ((arg = getarg(args, 'l')) != NULL) {
			if (strcmp(pwd->pw_name, "root") == 0)
				errx(EX_DATAERR, "can't rename `root' account");
			pwd->pw_name = pw_checkname((u_char *)arg->val, 0);
			edited = 1;
		}

		if ((arg = getarg(args, 'u')) != NULL && isdigit((unsigned char)*arg->val)) {
			pwd->pw_uid = (uid_t) atol(arg->val);
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

				if (now == expire)
					errx(EX_DATAERR, "invalid password change date `%s'", arg->val);
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

				if (now == expire)
					errx(EX_DATAERR, "invalid account expiry date `%s'", arg->val);
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
			edited = strcmp(pwd->pw_dir, arg->val) != 0;
			if (stat(pwd->pw_dir = arg->val, &st) == -1) {
				if (getarg(args, 'm') == NULL && strcmp(pwd->pw_dir, "/nonexistent") != 0)
				  warnx("WARNING: home `%s' does not exist", pwd->pw_dir);
			} else if (!S_ISDIR(st.st_mode))
				warnx("WARNING: home `%s' is not a directory", pwd->pw_dir);
		}

		if ((arg = getarg(args, 'w')) != NULL && getarg(args, 'h') == NULL) {
			login_cap_t *lc;

			lc = login_getpwclass(pwd);
			if (lc == NULL ||
			    login_setcryptfmt(lc, "md5", NULL) == NULL)
				warn("setting crypt(3) format");
			login_close(lc);
			pwd->pw_passwd = pw_password(cnf, args, pwd->pw_name);
			edited = 1;
		}

	} else {
		login_cap_t *lc;

		/*
		 * Add code
		 */

		if (a_name == NULL)	/* Required */
			errx(EX_DATAERR, "login name required");
		else if ((pwd = GETPWNAM(a_name->val)) != NULL)	/* Exists */
			errx(EX_DATAERR, "login name `%s' already exists", a_name->val);

		/*
		 * Now, set up defaults for a new user
		 */
		pwd = &fakeuser;
		pwd->pw_name = a_name->val;
		pwd->pw_class = cnf->default_class ? cnf->default_class : "";
		pwd->pw_uid = pw_uidpolicy(cnf, args);
		pwd->pw_gid = pw_gidpolicy(cnf, args, pwd->pw_name, (gid_t) pwd->pw_uid);
		pwd->pw_change = pw_pwdpolicy(cnf, args);
		pwd->pw_expire = pw_exppolicy(cnf, args);
		pwd->pw_dir = pw_homepolicy(cnf, args, pwd->pw_name);
		pwd->pw_shell = pw_shellpolicy(cnf, args, NULL);
		lc = login_getpwclass(pwd);
		if (lc == NULL || login_setcryptfmt(lc, "md5", NULL) == NULL)
			warn("setting crypt(3) format");
		login_close(lc);
		pwd->pw_passwd = pw_password(cnf, args, pwd->pw_name);
		edited = 1;

		if (pwd->pw_uid == 0 && strcmp(pwd->pw_name, "root") != 0)
			warnx("WARNING: new account `%s' has a uid of 0 (superuser access!)", pwd->pw_name);
	}

	/*
	 * Shared add/edit code
	 */
	if ((arg = getarg(args, 'c')) != NULL) {
		char	*gecos = pw_checkname((u_char *)arg->val, 1);
		if (strcmp(pwd->pw_gecos, gecos) != 0) {
			pwd->pw_gecos = gecos;
			edited = 1;
		}
	}

	if ((arg = getarg(args, 'h')) != NULL) {
		if (strcmp(arg->val, "-") == 0) {
			if (!pwd->pw_passwd || *pwd->pw_passwd != '*') {
				pwd->pw_passwd = "*";	/* No access */
				edited = 1;
			}
		} else {
			int             fd = atoi(arg->val);
			int             b;
			int             istty = isatty(fd);
			struct termios  t;
			login_cap_t	*lc;

			if (istty) {
				if (tcgetattr(fd, &t) == -1)
					istty = 0;
				else {
					struct termios  n = t;

					/* Disable echo */
					n.c_lflag &= ~(ECHO);
					tcsetattr(fd, TCSANOW, &n);
					printf("%sassword for user %s:", (mode == M_UPDATE) ? "New p" : "P", pwd->pw_name);
					fflush(stdout);
				}
			}
			b = read(fd, line, sizeof(line) - 1);
			if (istty) {	/* Restore state */
				tcsetattr(fd, TCSANOW, &t);
				fputc('\n', stdout);
				fflush(stdout);
			}
			if (b < 0) {
				warn("-h file descriptor");
				return EX_IOERR;
			}
			line[b] = '\0';
			if ((p = strpbrk(line, " \t\r\n")) != NULL)
				*p = '\0';
			if (!*line)
				errx(EX_DATAERR, "empty password read on file descriptor %d", fd);
			lc = login_getpwclass(pwd);
			if (lc == NULL ||
			    login_setcryptfmt(lc, "md5", NULL) == NULL)
				warn("setting crypt(3) format");
			login_close(lc);
			pwd->pw_passwd = pw_pwcrypt(line);
			edited = 1;
		}
	}

	/*
	 * Special case: -N only displays & exits
	 */
	if (getarg(args, 'N') != NULL)
		return print_user(pwd,
				  getarg(args, 'P') != NULL,
				  getarg(args, '7') != NULL);

	if (mode == M_ADD) {
		edited = 1;	/* Always */
		rc = addpwent(pwd);
		if (rc == -1) {
			warnx("user '%s' already exists", pwd->pw_name);
			return EX_IOERR;
		} else if (rc != 0) {
			warn("passwd file update");
			return EX_IOERR;
		}
		if (cnf->nispasswd && *cnf->nispasswd=='/') {
			rc = addnispwent(cnf->nispasswd, pwd);
			if (rc == -1)
				warnx("User '%s' already exists in NIS passwd", pwd->pw_name);
			else
				warn("NIS passwd update");
			/* NOTE: we treat NIS-only update errors as non-fatal */
		}
	} else if (mode == M_UPDATE || mode == M_LOCK || mode == M_UNLOCK) {
		if (edited) {	/* Only updated this if required */
			rc = chgpwent(a_name->val, pwd);
			if (rc == -1) {
				warnx("user '%s' does not exist (NIS?)", pwd->pw_name);
				return EX_IOERR;
			} else if (rc != 0) {
				warn("passwd file update");
				return EX_IOERR;
			}
			if ( cnf->nispasswd && *cnf->nispasswd=='/') {
				rc = chgnispwent(cnf->nispasswd, a_name->val, pwd);
				if (rc == -1)
					warn("User '%s' not found in NIS passwd", pwd->pw_name);
				else
					warn("NIS passwd update");
				/* NOTE: NIS-only update errors are not fatal */
			}
		}
	}

	/*
	 * Ok, user is created or changed - now edit group file
	 */

	if (mode == M_ADD || getarg(args, 'G') != NULL)
		editgroups(pwd->pw_name, cnf->groups);

	/* go get a current version of pwd */
	pwd = GETPWNAM(a_name->val);
	if (pwd == NULL) {
		/* This will fail when we rename, so special case that */
		if (mode == M_UPDATE && (arg = getarg(args, 'l')) != NULL) {
			a_name->val = arg->val;		/* update new name */
			pwd = GETPWNAM(a_name->val);	/* refetch renamed rec */
		}
	}
	if (pwd == NULL)	/* can't go on without this */
		errx(EX_NOUSER, "user '%s' disappeared during update", a_name->val);

	grp = GETGRGID(pwd->pw_gid);
	pw_log(cnf, mode, W_USER, "%s(%ld):%s(%ld):%s:%s:%s",
	       pwd->pw_name, (long) pwd->pw_uid,
	    grp ? grp->gr_name : "unknown", (long) (grp ? grp->gr_gid : -1),
	       pwd->pw_gecos, pwd->pw_dir, pwd->pw_shell);

	/*
	 * If adding, let's touch and chown the user's mail file. This is not
	 * strictly necessary under BSD with a 0755 maildir but it also
	 * doesn't hurt anything to create the empty mailfile
	 */
	if (mode == M_ADD) {
		if (!PWALTDIR()) {
			sprintf(line, "%s/%s", _PATH_MAILDIR, pwd->pw_name);
			close(open(line, O_RDWR | O_CREAT, 0600));	/* Preserve contents &
									 * mtime */
			chown(line, pwd->pw_uid, pwd->pw_gid);
		}
	}

	/*
	 * Let's create and populate the user's home directory. Note
	 * that this also `works' for editing users if -m is used, but
	 * existing files will *not* be overwritten.
	 */
	if (!PWALTDIR() && getarg(args, 'm') != NULL && pwd->pw_dir && *pwd->pw_dir == '/' && pwd->pw_dir[1]) {
		copymkdir(pwd->pw_dir, cnf->dotdir, 0755, pwd->pw_uid, pwd->pw_gid);
		pw_log(cnf, mode, W_USER, "%s(%ld) home %s made",
		       pwd->pw_name, (long) pwd->pw_uid, pwd->pw_dir);
	}


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
			pw_log(cnf, mode, W_USER, "%s(%ld) new user mail sent",
			    pwd->pw_name, (long) pwd->pw_uid);
		}
		fclose(fp);
	}

	return EXIT_SUCCESS;
}


static          uid_t
pw_uidpolicy(struct userconf * cnf, struct cargs * args)
{
	struct passwd  *pwd;
	uid_t           uid = (uid_t) - 1;
	struct carg    *a_uid = getarg(args, 'u');

	/*
	 * Check the given uid, if any
	 */
	if (a_uid != NULL) {
		uid = (uid_t) atol(a_uid->val);

		if ((pwd = GETPWUID(uid)) != NULL && getarg(args, 'o') == NULL)
			errx(EX_DATAERR, "uid `%ld' has already been allocated", (long) pwd->pw_uid);
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
pw_gidpolicy(struct userconf * cnf, struct cargs * args, char *nam, gid_t prefer)
{
	struct group   *grp;
	gid_t           gid = (uid_t) - 1;
	struct carg    *a_gid = getarg(args, 'g');

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
	} else if ((grp = GETGRNAM(nam)) != NULL && grp->gr_mem[0] == NULL) {
		gid = grp->gr_gid;  /* Already created? Use it anyway... */
	} else {
		struct cargs    grpargs;
		char            tmp[32];

		LIST_INIT(&grpargs);
		addarg(&grpargs, 'n', nam);

		/*
		 * We need to auto-create a group with the user's name. We
		 * can send all the appropriate output to our sister routine
		 * bit first see if we can create a group with gid==uid so we
		 * can keep the user and group ids in sync. We purposely do
		 * NOT check the gid range if we can force the sync. If the
		 * user's name dups an existing group, then the group add
		 * function will happily handle that case for us and exit.
		 */
		if (GETGRGID(prefer) == NULL) {
			sprintf(tmp, "%lu", (unsigned long) prefer);
			addarg(&grpargs, 'g', tmp);
		}
		if (getarg(args, 'N'))
		{
			addarg(&grpargs, 'N', NULL);
			addarg(&grpargs, 'q', NULL);
			gid = pw_group(cnf, M_NEXT, &grpargs);
		}
		else
		{
			pw_group(cnf, M_ADD, &grpargs);
			if ((grp = GETGRNAM(nam)) != NULL)
				gid = grp->gr_gid;
		}
		a_gid = grpargs.lh_first;
		while (a_gid != NULL) {
			struct carg    *t = a_gid->list.le_next;
			LIST_REMOVE(a_gid, list);
			a_gid = t;
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

	if (arg)
		return arg->val;
	else {
		static char     home[128];

		if (cnf->home == NULL || *cnf->home == '\0')
			errx(EX_CONFIG, "no base home directory set");
		sprintf(home, "%s/%s", cnf->home, user);
		return home;
	}
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
		strncpy(paths, path, sizeof paths);
		paths[sizeof paths - 1] = '\0';
		for (p = strtok(paths, ": \t\r\n"); p != NULL; p = strtok(NULL, ": \t\r\n")) {
			int             i;
			static char     shellpath[256];

			if (sh != NULL) {
				sprintf(shellpath, "%s/%s", p, sh);
				if (access(shellpath, X_OK) == 0)
					return shellpath;
			} else
				for (i = 0; i < _UC_MAXSHELLS && shells[i] != NULL; i++) {
					sprintf(shellpath, "%s/%s", p, shells[i]);
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

static char const chars[] = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ.";

char           *
pw_pwcrypt(char *password)
{
	int             i;
	char            salt[12];

	static char     buf[256];

	/*
	 * Calculate a salt value
	 */
	for (i = 0; i < 8; i++)
		salt[i] = chars[arc4random() % 63];
	salt[i] = '\0';

	return strcpy(buf, crypt(password, salt));
}

#if defined(USE_MD5RAND)
u_char *
pw_getrand(u_char *buf, int len)	/* cryptographically secure rng */
{
	int i;
	for (i=0;i<len;i+=16) {
		u_char ubuf[16];

		MD5_CTX md5_ctx;
		struct timeval tv, tvo;
		struct rusage ru;
		int n=0;
		int t;

		MD5Init (&md5_ctx);
		t=getpid();
		MD5Update (&md5_ctx, (u_char*)&t, sizeof t);
		t=getppid();
		MD5Update (&md5_ctx, (u_char*)&t, sizeof t);
		gettimeofday (&tvo, NULL);
		do {
			getrusage (RUSAGE_SELF, &ru);
			MD5Update (&md5_ctx, (u_char*)&ru, sizeof ru);
			gettimeofday (&tv, NULL);
			MD5Update (&md5_ctx, (u_char*)&tv, sizeof tv);
		} while (n++<20 || tv.tv_usec-tvo.tv_usec<100*1000);
		MD5Final (ubuf, &md5_ctx);
		memcpy(buf+i, ubuf, MIN(16, len-i));
	}
	return buf;
}

#else	/* Portable version */

static u_char *
pw_getrand(u_char *buf, int len)
{
	int i;

	srandomdev();
	for (i = 0; i < len; i++) {
		unsigned long val = random();
		/* Use all bits in the random value */
		buf[i]=(u_char)((val >> 24) ^ (val >> 16) ^ (val >> 8) ^ val);
	}
	return buf;
}

#endif

static char    *
pw_password(struct userconf * cnf, struct cargs * args, char const * user)
{
	int             i, l;
	char            pwbuf[32];
	u_char		rndbuf[sizeof pwbuf];

	switch (cnf->default_password) {
	case -1:		/* Random password */
		l = (arc4random() % 8 + 8);	/* 8 - 16 chars */
		pw_getrand(rndbuf, l);
		for (i = 0; i < l; i++)
			pwbuf[i] = chars[rndbuf[i] % (sizeof(chars)-1)];
		pwbuf[i] = '\0';

		/*
		 * We give this information back to the user
		 */
		if (getarg(args, 'h') == NULL && getarg(args, 'N') == NULL) {
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
		strncpy(pwbuf, user, sizeof pwbuf);
		pwbuf[sizeof pwbuf - 1] = '\0';
		break;
	}
	return pw_pwcrypt(pwbuf);
}


static int
print_user(struct passwd * pwd, int pretty, int v7)
{
	if (!pretty) {
		char            buf[_UC_MAXLINE];

		fmtpwentry(buf, pwd, v7 ? PWF_PASSWD : PWF_STANDARD);
		fputs(buf, stdout);
	} else {
		int		j;
		char           *p;
		struct group   *grp = GETGRGID(pwd->pw_gid);
		char            uname[60] = "User &", office[60] = "[None]",
		                wphone[60] = "[None]", hphone[60] = "[None]";
		char		acexpire[32] = "[None]", pwexpire[32] = "[None]";
		struct tm *    tptr;

		if ((p = strtok(pwd->pw_gecos, ",")) != NULL) {
			strncpy(uname, p, sizeof uname);
			uname[sizeof uname - 1] = '\0';
			if ((p = strtok(NULL, ",")) != NULL) {
				strncpy(office, p, sizeof office);
				office[sizeof office - 1] = '\0';
				if ((p = strtok(NULL, ",")) != NULL) {
					strncpy(wphone, p, sizeof wphone);
					wphone[sizeof wphone - 1] = '\0';
					if ((p = strtok(NULL, "")) != NULL) {
						strncpy(hphone, p, sizeof hphone);
						hphone[sizeof hphone - 1] = '\0';
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
		printf("Login Name: %-15s   #%-12ld Group: %-15s   #%ld\n"
		       " Full Name: %s\n"
		       "      Home: %-26.26s      Class: %s\n"
		       "     Shell: %-26.26s     Office: %s\n"
		       "Work Phone: %-26.26s Home Phone: %s\n"
		       "Acc Expire: %-26.26s Pwd Expire: %s\n",
		       pwd->pw_name, (long) pwd->pw_uid,
		       grp ? grp->gr_name : "(invalid)", (long) pwd->pw_gid,
		       uname, pwd->pw_dir, pwd->pw_class,
		       pwd->pw_shell, office, wphone, hphone,
		       acexpire, pwexpire);
	        SETGRENT();
		j = 0;
		while ((grp=GETGRENT()) != NULL)
		{
			int     i = 0;
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
		ENDGRENT();
		printf("%s", j ? "\n" : "");
	}
	return EXIT_SUCCESS;
}

char    *
pw_checkname(u_char *name, int gecos)
{
	int             l = 0;
	char const     *notch = gecos ? ":!@" : " ,\t:+&#%$^()!@~*?<>=|\\/\"";

	while (name[l]) {
		if (strchr(notch, name[l]) != NULL || name[l] < ' ' || name[l] == 127 ||
			(!gecos && l==0 && name[l] == '-') ||	/* leading '-' */
			(!gecos && name[l] & 0x80))	/* 8-bit */
			errx(EX_DATAERR, (name[l] >= ' ' && name[l] < 127)
					    ? "invalid character `%c' in field"
					    : "invalid character 0x%02x in field",
					    name[l]);
		++l;
	}
	if (!gecos && l > LOGNAMESIZE)
		errx(EX_DATAERR, "name too long `%s'", name);
	return (char *)name;
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

				sprintf(tmp, "/usr/bin/atrm %s", e->d_name);
				system(tmp);
			}
		}
		closedir(d);
	}
}

static void
rmskey(char const * name)
{
	static const char etcskey[] = "/etc/skeykeys";
	FILE   *fp = fopen(etcskey, "r+");

	if (fp != NULL) {
		char	tmp[1024];
		off_t	atofs = 0;
		int	length = strlen(name);

		while (fgets(tmp, sizeof tmp, fp) != NULL) {
			if (strncmp(name, tmp, length) == 0 && tmp[length]==' ') {
				if (fseek(fp, atofs, SEEK_SET) == 0) {
					fwrite("#", 1, 1, fp);	/* Comment username out */
				}
				break;
			}
			atofs = ftell(fp);
		}
		/*
		 * If we got an error of any sort, don't update!
		 */
		fclose(fp);
	}
}

