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
 *	$FreeBSD$
 */

#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <paths.h>
#include <sys/param.h>
#include <dirent.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <utmp.h>
#if defined(USE_MD5RAND)
#include <md5.h>
#endif
#include "pw.h"
#include "bitmap.h"
#include "pwupd.h"

#if (MAXLOGNAME-1) > UT_NAMESIZE
#define LOGNAMESIZE UT_NAMESIZE
#else
#define LOGNAMESIZE (MAXLOGNAME-1)
#endif

static int      print_user(struct passwd * pwd, int pretty);
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
	int	        r, r1;
	char           *p = NULL;
	struct carg    *a_name;
	struct carg    *a_uid;
	struct carg    *arg;
	struct passwd  *pwd = NULL;
	struct group   *grp;
	struct stat     st;
	char            line[_PASSWORD_LEN+1];

	static struct passwd fakeuser =
	{
		NULL,
		"*",
		-1,
		-1,
		0,
		"",
		"User &",
		"/bin/sh",
		0,
		0
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
			cmderr(EX_DATAERR, "invalid base directory for home '%s'\n", cnf->home);

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
						cmderr(EX_OSFILE, "'%s' (root home parent) is not a directory\n", dbuf);
					*p = '/';
				}
			}
			if (stat(dbuf, &st) == -1) {
				if (mkdir(dbuf, 0755) == -1) {
				direrr:	cmderr(EX_OSFILE, "mkdir '%s': %s\n", dbuf, strerror(errno));
				}
				chown(dbuf, 0, 0);
			}
		} else if (!S_ISDIR(st.st_mode))
			cmderr(EX_OSFILE, "root home `%s' is not a directory\n", cnf->home);
	}


	if ((arg = getarg(args, 'e')) != NULL)
		cnf->expire_days = atoi(arg->val);

	if ((arg = getarg(args, 'y')) != NULL)
		cnf->nispasswd = arg->val;

	if ((arg = getarg(args, 'p')) != NULL && arg->val)
		cnf->password_days = atoi(arg->val);

	if ((arg = getarg(args, 'g')) != NULL) {
		p = arg->val;
		if ((grp = getgrnam(p)) == NULL) {
			if (!isdigit(*p) || (grp = getgrgid((gid_t) atoi(p))) == NULL)
				cmderr(EX_NOUSER, "group `%s' does not exist\n", p);
		}
		cnf->default_group = newstr(grp->gr_name);
	}
	if ((arg = getarg(args, 'L')) != NULL)
		cnf->default_class = pw_checkname((u_char *)arg->val, 0);

	if ((arg = getarg(args, 'G')) != NULL && arg->val) {
		int             i = 0;

		for (p = strtok(arg->val, ", \t"); p != NULL; p = strtok(NULL, ", \t")) {
			if ((grp = getgrnam(p)) == NULL) {
				if (!isdigit(*p) || (grp = getgrgid((gid_t) atoi(p))) == NULL)
					cmderr(EX_NOUSER, "group `%s' does not exist\n", p);
			}
			if (extendarray(&cnf->groups, &cnf->numgroups, i + 2) != -1)
				cnf->groups[i++] = newstr(grp->gr_name);
		}
		while (i < cnf->numgroups)
			cnf->groups[i++] = NULL;
	}
	if ((arg = getarg(args, 'k')) != NULL) {
		if (stat(cnf->dotdir = arg->val, &st) == -1 || S_ISDIR(st.st_mode))
			cmderr(EX_OSFILE, "skeleton `%s' is not a directory or does not exist\n", cnf->dotdir);
	}
	if ((arg = getarg(args, 's')) != NULL)
		cnf->shell_default = arg->val;

	if (mode == M_ADD && getarg(args, 'D')) {
		if (getarg(args, 'n') != NULL)
			cmderr(EX_DATAERR, "can't combine `-D' with `-n name'\n");
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
		if ((arg = getarg(args, 'w')) != NULL)
			cnf->default_password = boolean_val(arg->val, cnf->default_password);

		arg = getarg(args, 'C');
		if (write_userconfig(arg ? arg->val : NULL))
			return EXIT_SUCCESS;
		perror("config update");
		return EX_IOERR;
	}
	if (mode == M_PRINT && getarg(args, 'a')) {
		int             pretty = getarg(args, 'P') != NULL;

		setpwent();
		while ((pwd = getpwent()) != NULL)
			print_user(pwd, pretty);
		endpwent();
		return EXIT_SUCCESS;
	}
	if ((a_name = getarg(args, 'n')) != NULL)
		pwd = getpwnam(pw_checkname((u_char *)a_name->val, 0));
	a_uid = getarg(args, 'u');

	if (a_uid == NULL) {
		if (a_name == NULL)
			cmderr(EX_DATAERR, "user name or id required\n");

		/*
		 * Determine whether 'n' switch is name or uid - we don't
		 * really don't really care which we have, but we need to
		 * know.
		 */
		if (mode != M_ADD && pwd == NULL && isdigit(*a_name->val) && atoi(a_name->val) > 0) {	/* Assume uid */
			(a_uid = a_name)->ch = 'u';
			a_name = NULL;
		}
	}
	/*
	 * Update, delete & print require that the user exists
	 */
	if (mode == M_UPDATE || mode == M_DELETE || mode == M_PRINT) {
		if (a_name == NULL && pwd == NULL)	/* Try harder */
			pwd = getpwuid(atoi(a_uid->val));

		if (pwd == NULL) {
			if (mode == M_PRINT && getarg(args, 'F')) {
				fakeuser.pw_name = a_name ? a_name->val : "nouser";
				fakeuser.pw_uid = a_uid ? (uid_t) atol(a_uid->val) : -1;
				return print_user(&fakeuser, getarg(args, 'P') != NULL);
			}
			if (a_name == NULL)
				cmderr(EX_NOUSER, "no such uid `%s'\n", a_uid->val);
			cmderr(EX_NOUSER, "no such user `%s'\n", a_name->val);
		}
		if (a_name == NULL)	/* May be needed later */
			a_name = addarg(args, 'n', newstr(pwd->pw_name));

		/*
		 * Handle deletions now
		 */
		if (mode == M_DELETE) {
			char            file[MAXPATHLEN];
			char            home[MAXPATHLEN];
			uid_t           uid = pwd->pw_uid;

			if (strcmp(pwd->pw_name, "root") == 0)
				cmderr(EX_DATAERR, "cannot remove user 'root'\n");

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
			/*
			 * Save these for later, since contents of pwd may be
			 * invalidated by deletion
			 */
			sprintf(file, "%s/%s", _PATH_MAILDIR, pwd->pw_name);
			strncpy(home, pwd->pw_dir, sizeof home);
			home[sizeof home - 1] = '\0';

			if (!delpwent(pwd))
				cmderr(EX_IOERR, "Error updating passwd file: %s\n", strerror(errno));

			if (cnf->nispasswd && *cnf->nispasswd=='/' && !delnispwent(cnf->nispasswd, a_name->val))
				perror("WARNING: NIS passwd update");
				
			editgroups(a_name->val, NULL);

			pw_log(cnf, mode, W_USER, "%s(%ld) account removed", a_name->val, (long) uid);

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
			return EXIT_SUCCESS;
		} else if (mode == M_PRINT)
			return print_user(pwd, getarg(args, 'P') != NULL);

		/*
		 * The rest is edit code
		 */
		if ((arg = getarg(args, 'l')) != NULL) {
			if (strcmp(pwd->pw_name, "root") == 0)
				cmderr(EX_DATAERR, "can't rename `root' account\n");
			pwd->pw_name = pw_checkname((u_char *)arg->val, 0);
		}
		if ((arg = getarg(args, 'u')) != NULL && isdigit(*arg->val)) {
			pwd->pw_uid = (uid_t) atol(arg->val);
			if (pwd->pw_uid != 0 && strcmp(pwd->pw_name, "root") == 0)
				cmderr(EX_DATAERR, "can't change uid of `root' account\n");
			if (pwd->pw_uid == 0 && strcmp(pwd->pw_name, "root") != 0)
				fprintf(stderr, "WARNING: account `%s' will have a uid of 0 (superuser access!)\n", pwd->pw_name);
		}
		if ((arg = getarg(args, 'g')) != NULL && pwd->pw_uid != 0)	/* Already checked this */
			pwd->pw_gid = (gid_t) getgrnam(cnf->default_group)->gr_gid;

		if ((arg = getarg(args, 'p')) != NULL) {
			if (*arg->val == '\0' || strcmp(arg->val, "0") == 0)
				pwd->pw_change = 0;
			else {
				time_t          now = time(NULL);
				time_t          expire = parse_date(now, arg->val);

				if (now == expire)
					cmderr(EX_DATAERR, "Invalid password change date `%s'\n", arg->val);
				pwd->pw_change = expire;
			}
		}
		if ((arg = getarg(args, 'e')) != NULL) {
			if (*arg->val == '\0' || strcmp(arg->val, "0") == 0)
				pwd->pw_expire = 0;
			else {
				time_t          now = time(NULL);
				time_t          expire = parse_date(now, arg->val);

				if (now == expire)
					cmderr(EX_DATAERR, "Invalid account expiry date `%s'\n", arg->val);
				pwd->pw_expire = expire;
			}
		}
		if ((arg = getarg(args, 's')) != NULL)
			pwd->pw_shell = shell_path(cnf->shelldir, cnf->shells, arg->val);

		if (getarg(args, 'L'))
			pwd->pw_class = cnf->default_class;

		if ((arg  = getarg(args, 'd')) != NULL) {
			if (stat(pwd->pw_dir = arg->val, &st) == -1) {
				if (getarg(args, 'm') == NULL && strcmp(pwd->pw_dir, "/nonexistent") != 0)
				  fprintf(stderr, "WARNING: home `%s' does not exist\n", pwd->pw_dir);
			} else if (!S_ISDIR(st.st_mode))
				fprintf(stderr, "WARNING: home `%s' is not a directory\n", pwd->pw_dir);
		}

		if ((arg = getarg(args, 'w')) != NULL && getarg(args, 'h') == NULL)
			pwd->pw_passwd = pw_password(cnf, args, pwd->pw_name);

	} else {
		if (a_name == NULL)	/* Required */
			cmderr(EX_DATAERR, "login name required\n");
		else if ((pwd = getpwnam(a_name->val)) != NULL)	/* Exists */
			cmderr(EX_DATAERR, "login name `%s' already exists\n", a_name->val);

		/*
		 * Now, set up defaults for a new user
		 */
		pwd = &fakeuser;
		pwd->pw_name = a_name->val;
		pwd->pw_class = cnf->default_class ? cnf->default_class : "";
		pwd->pw_passwd = pw_password(cnf, args, pwd->pw_name);
		pwd->pw_uid = pw_uidpolicy(cnf, args);
		pwd->pw_gid = pw_gidpolicy(cnf, args, pwd->pw_name, (gid_t) pwd->pw_uid);
		pwd->pw_change = pw_pwdpolicy(cnf, args);
		pwd->pw_expire = pw_exppolicy(cnf, args);
		pwd->pw_dir = pw_homepolicy(cnf, args, pwd->pw_name);
		pwd->pw_shell = pw_shellpolicy(cnf, args, NULL);

		if (pwd->pw_uid == 0 && strcmp(pwd->pw_name, "root") != 0)
			fprintf(stderr, "WARNING: new account `%s' has a uid of 0 (superuser access!)\n", pwd->pw_name);
	}

	/*
	 * Shared add/edit code
	 */
	if ((arg = getarg(args, 'c')) != NULL)
		pwd->pw_gecos = pw_checkname((u_char *)arg->val, 1);

	if ((arg = getarg(args, 'h')) != NULL) {
		if (strcmp(arg->val, "-") == 0)
			pwd->pw_passwd = "*";	/* No access */
		else {
			int             fd = atoi(arg->val);
			int             b;
			int             istty = isatty(fd);
			struct termios  t;

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
				perror("-h file descriptor");
				return EX_IOERR;
			}
			line[b] = '\0';
			if ((p = strpbrk(line, " \t\r\n")) != NULL)
				*p = '\0';
			if (!*line)
				cmderr(EX_DATAERR, "empty password read on file descriptor %d\n", fd);
			pwd->pw_passwd = pw_pwcrypt(line);
		}
	}

	/*
	 * Special case: -N only displays & exits
	 */
	if (getarg(args, 'N') != NULL)
		return print_user(pwd, getarg(args, 'P') != NULL);

	r = r1 = 1;
	if (mode == M_ADD) {
		r = addpwent(pwd);
		if (r && cnf->nispasswd && *cnf->nispasswd=='/')
			r1 = addnispwent(cnf->nispasswd, pwd);
	} else if (mode == M_UPDATE) {
		r = chgpwent(a_name->val, pwd);
		if (r && cnf->nispasswd && *cnf->nispasswd=='/')
			r1 = chgnispwent(cnf->nispasswd, a_name->val, pwd);
	}

	if (!r) {
		perror("password update");
		return EX_IOERR;
	} else if (!r1) {
		perror("WARNING: NIS password update");
		/* Keep on trucking */
	}

	/*
	 * Ok, user is created or changed - now edit group file
	 */

	if (mode == M_ADD || getarg(args, 'G') != NULL)
		editgroups(pwd->pw_name, cnf->groups);

	/* pwd may have been invalidated */
	if ((pwd = getpwnam(a_name->val)) == NULL)
		cmderr(EX_NOUSER, "user '%s' disappeared during update\n", a_name->val);

	grp = getgrgid(pwd->pw_gid);
	pw_log(cnf, mode, W_USER, "%s(%ld):%s(%d):%s:%s:%s",
	       pwd->pw_name, (long) pwd->pw_uid,
	    grp ? grp->gr_name : "unknown", (long) (grp ? grp->gr_gid : -1),
	       pwd->pw_gecos, pwd->pw_dir, pwd->pw_shell);

	/*
	 * If adding, let's touch and chown the user's mail file. This is not
	 * strictly necessary under BSD with a 0755 maildir but it also
	 * doesn't hurt anything to create the empty mailfile
	 */
	if (mode == M_ADD) {
		FILE           *fp;

		sprintf(line, "%s/%s", _PATH_MAILDIR, pwd->pw_name);
		close(open(line, O_RDWR | O_CREAT, 0600));	/* Preserve contents &
								 * mtime */
		chown(line, pwd->pw_uid, pwd->pw_gid);

		/*
		 * Send mail to the new user as well, if we are asked to
		 */
		if (cnf->newmail && *cnf->newmail && (fp = fopen(cnf->newmail, "r")) != NULL) {
			FILE           *pfp = popen(_PATH_SENDMAIL " -t", "w");

			if (pfp == NULL)
				perror("sendmail");
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
	}
	/*
	 * Finally, let's create and populate the user's home directory. Note
	 * that this also `works' for editing users if -m is used, but
	 * existing files will *not* be overwritten.
	 */
	if (getarg(args, 'm') != NULL && pwd->pw_dir && *pwd->pw_dir == '/' && pwd->pw_dir[1]) {
		copymkdir(pwd->pw_dir, cnf->dotdir, 0755, pwd->pw_uid, pwd->pw_gid);
		pw_log(cnf, mode, W_USER, "%s(%ld) home %s made",
		       pwd->pw_name, (long) pwd->pw_uid, pwd->pw_dir);
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

		if ((pwd = getpwuid(uid)) != NULL && getarg(args, 'o') == NULL)
			cmderr(EX_DATAERR, "uid `%ld' has already been allocated\n", (long) pwd->pw_uid);
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
		setpwent();
		while ((pwd = getpwent()) != NULL)
			if (pwd->pw_uid >= (int) cnf->min_uid && pwd->pw_uid <= (int) cnf->max_uid)
				bm_setbit(&bm, pwd->pw_uid - cnf->min_uid);
		endpwent();

		/*
		 * Then apply the policy, with fallback to reuse if necessary
		 */
		if (cnf->reuse_uids || (uid = (uid_t) (bm_lastset(&bm) + cnf->min_uid + 1)) > cnf->max_uid)
			uid = (uid_t) (bm_firstunset(&bm) + cnf->min_uid);

		/*
		 * Another sanity check
		 */
		if (uid < cnf->min_uid || uid > cnf->max_uid)
			cmderr(EX_SOFTWARE, "unable to allocate a new uid - range fully used\n");
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
	setgrent();
	if (a_gid != NULL) {
		if ((grp = getgrnam(a_gid->val)) == NULL) {
			gid = (gid_t) atol(a_gid->val);
			if ((gid == 0 && !isdigit(*a_gid->val)) || (grp = getgrgid(gid)) == NULL)
				cmderr(EX_NOUSER, "group `%s' is not defined\n", a_gid->val);
		}
		gid = grp->gr_gid;
	} else if ((grp = getgrnam(nam)) != NULL && grp->gr_mem[0] == NULL) {
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
		if (getgrgid(prefer) == NULL) {
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
			if ((grp = getgrnam(nam)) != NULL)
				gid = grp->gr_gid;
		}
		a_gid = grpargs.lh_first;
		while (a_gid != NULL) {
			struct carg    *t = a_gid->list.le_next;
			LIST_REMOVE(a_gid, list);
			a_gid = t;
		}
	}
	endgrent();
	return gid;
}


static          time_t
pw_pwdpolicy(struct userconf * cnf, struct cargs * args)
{
	time_t          result = 0;
	time_t          now = time(NULL);
	struct carg    *arg = getarg(args, 'e');

	if (arg != NULL) {
		if ((result = parse_date(now, arg->val)) == now)
			cmderr(EX_DATAERR, "invalid date/time `%s'\n", arg->val);
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
			cmderr(EX_DATAERR, "invalid date/time `%s'\n", arg->val);
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
			cmderr(EX_CONFIG, "no base home directory set\n");
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
			cmderr(EX_OSFILE, "can't find shell `%s' in shell paths\n", sh);
		cmderr(EX_CONFIG, "no default shell available or defined\n");
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
	srandom((unsigned) (time(NULL) ^ getpid()));
	for (i = 0; i < 8; i++)
		salt[i] = chars[random() % 63];
	salt[i] = '\0';

	return strcpy(buf, crypt(password, salt));
}

#if defined(__FreeBSD__)

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
		memcpy(buf+i, ubuf, MIN(16, len-n));
	}
	return buf;
}

#else	/* Use random device (preferred) */

static u_char *
pw_getrand(u_char *buf, int len)
{
	int		fd;
	fd = open("/dev/urandom", O_RDONLY);
	if (fd==-1)
		cmderr(EX_OSFILE, "can't open /dev/urandom: %s\n", strerror(errno));
	else if (read(fd, buf, len)!=len)
		cmderr(EX_IOERR, "read error on /dev/urandom\n");
	close(fd);
	return buf;
}

#endif

#else	/* Portable version */

static u_char *
pw_getrand(u_char *buf, int len)
{
	int i;

	for (i = 0; i < len; i++) {
		unsigned val = random();
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
		srandom((unsigned) (time(NULL) ^ getpid()));
		l = (random() % 8 + 8);	/* 8 - 16 chars */
		pw_getrand(rndbuf, l);
		for (i = 0; i < l; i++)
			pwbuf[i] = chars[rndbuf[i] % sizeof(chars)];
		pwbuf[i] = '\0';

		/*
		 * We give this information back to the user
		 */
		if (getarg(args, 'h') == NULL && getarg(args, 'N') == NULL) {
			if (isatty(1))
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
print_user(struct passwd * pwd, int pretty)
{
	if (!pretty) {
		char            buf[_UC_MAXLINE];

		fmtpwent(buf, pwd);
		fputs(buf, stdout);
	} else {
		int		j;
		char           *p;
		struct group   *grp = getgrgid(pwd->pw_gid);
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
			*p = (char) toupper(*p);
		}
		if (pwd->pw_expire > (time_t)0 && (tptr = localtime(&pwd->pw_expire)) != NULL)
		  strftime(acexpire, sizeof acexpire, "%c", tptr);
		if (pwd->pw_change > (time_t)9 && (tptr = localtime(&pwd->pw_change)) != NULL)
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
	        setgrent();
		j = 0;
		while ((grp=getgrent()) != NULL)
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
		endgrent();
		printf("%s\n", j ? "\n" : "");
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
			cmderr(EX_DATAERR, (name[l] >= ' ' && name[l] < 127)
					    ? "invalid character `%c' in field\n"
					    : "invalid character 0x%02x in field\n",
					    name[l]);
		++l;
	}
	if (!gecos && l > LOGNAMESIZE)
		cmderr(EX_DATAERR, "name too long `%s'\n", name);
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

