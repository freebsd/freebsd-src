/*
 * Copyright (c) 2000 Markus Friedl.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "includes.h"
RCSID("$OpenBSD: auth.c,v 1.45 2002/09/20 18:41:29 stevesk Exp $");

#ifdef HAVE_LOGIN_H
#include <login.h>
#endif
#if defined(HAVE_SHADOW_H) && !defined(DISABLE_SHADOW)
#include <shadow.h>
#endif /* defined(HAVE_SHADOW_H) && !defined(DISABLE_SHADOW) */

#ifdef HAVE_LIBGEN_H
#include <libgen.h>
#endif

#include "xmalloc.h"
#include "match.h"
#include "groupaccess.h"
#include "log.h"
#include "servconf.h"
#include "auth.h"
#include "auth-options.h"
#include "canohost.h"
#include "buffer.h"
#include "bufaux.h"
#include "uidswap.h"
#include "tildexpand.h"
#include "misc.h"
#include "bufaux.h"
#include "packet.h"

/* import */
extern ServerOptions options;

/* Debugging messages */
Buffer auth_debug;
int auth_debug_init;

/*
 * Check if the user is allowed to log in via ssh. If user is listed
 * in DenyUsers or one of user's groups is listed in DenyGroups, false
 * will be returned. If AllowUsers isn't empty and user isn't listed
 * there, or if AllowGroups isn't empty and one of user's groups isn't
 * listed there, false will be returned.
 * If the user's shell is not executable, false will be returned.
 * Otherwise true is returned.
 */
int
allowed_user(struct passwd * pw)
{
	struct stat st;
	const char *hostname = NULL, *ipaddr = NULL;
	char *shell;
	int i;
#ifdef WITH_AIXAUTHENTICATE
	char *loginmsg;
#endif /* WITH_AIXAUTHENTICATE */
#if !defined(USE_PAM) && defined(HAVE_SHADOW_H) && \
	!defined(DISABLE_SHADOW) && defined(HAS_SHADOW_EXPIRE)
	struct spwd *spw;

	/* Shouldn't be called if pw is NULL, but better safe than sorry... */
	if (!pw || !pw->pw_name)
		return 0;

#define	DAY		(24L * 60 * 60) /* 1 day in seconds */
	spw = getspnam(pw->pw_name);
	if (spw != NULL) {
		time_t today = time(NULL) / DAY;
		debug3("allowed_user: today %d sp_expire %d sp_lstchg %d"
		    " sp_max %d", (int)today, (int)spw->sp_expire,
		    (int)spw->sp_lstchg, (int)spw->sp_max);

		/*
		 * We assume account and password expiration occurs the
		 * day after the day specified.
		 */
		if (spw->sp_expire != -1 && today > spw->sp_expire) {
			log("Account %.100s has expired", pw->pw_name);
			return 0;
		}

		if (spw->sp_lstchg == 0) {
			log("User %.100s password has expired (root forced)",
			    pw->pw_name);
			return 0;
		}

		if (spw->sp_max != -1 &&
		    today > spw->sp_lstchg + spw->sp_max) {
			log("User %.100s password has expired (password aged)",
			    pw->pw_name);
			return 0;
		}
	}
#else
	/* Shouldn't be called if pw is NULL, but better safe than sorry... */
	if (!pw || !pw->pw_name)
		return 0;
#endif

	/*
	 * Get the shell from the password data.  An empty shell field is
	 * legal, and means /bin/sh.
	 */
	shell = (pw->pw_shell[0] == '\0') ? _PATH_BSHELL : pw->pw_shell;

	/* deny if shell does not exists or is not executable */
	if (stat(shell, &st) != 0) {
		log("User %.100s not allowed because shell %.100s does not exist",
		    pw->pw_name, shell);
		return 0;
	}
	if (S_ISREG(st.st_mode) == 0 ||
	    (st.st_mode & (S_IXOTH|S_IXUSR|S_IXGRP)) == 0) {
		log("User %.100s not allowed because shell %.100s is not executable",
		    pw->pw_name, shell);
		return 0;
	}

	if (options.num_deny_users > 0 || options.num_allow_users > 0) {
		hostname = get_canonical_hostname(options.verify_reverse_mapping);
		ipaddr = get_remote_ipaddr();
	}

	/* Return false if user is listed in DenyUsers */
	if (options.num_deny_users > 0) {
		for (i = 0; i < options.num_deny_users; i++)
			if (match_user(pw->pw_name, hostname, ipaddr,
			    options.deny_users[i])) {
				log("User %.100s not allowed because listed in DenyUsers",
				    pw->pw_name);
				return 0;
			}
	}
	/* Return false if AllowUsers isn't empty and user isn't listed there */
	if (options.num_allow_users > 0) {
		for (i = 0; i < options.num_allow_users; i++)
			if (match_user(pw->pw_name, hostname, ipaddr,
			    options.allow_users[i]))
				break;
		/* i < options.num_allow_users iff we break for loop */
		if (i >= options.num_allow_users) {
			log("User %.100s not allowed because not listed in AllowUsers",
			    pw->pw_name);
			return 0;
		}
	}
	if (options.num_deny_groups > 0 || options.num_allow_groups > 0) {
		/* Get the user's group access list (primary and supplementary) */
		if (ga_init(pw->pw_name, pw->pw_gid) == 0) {
			log("User %.100s not allowed because not in any group",
			    pw->pw_name);
			return 0;
		}

		/* Return false if one of user's groups is listed in DenyGroups */
		if (options.num_deny_groups > 0)
			if (ga_match(options.deny_groups,
			    options.num_deny_groups)) {
				ga_free();
				log("User %.100s not allowed because a group is listed in DenyGroups",
				    pw->pw_name);
				return 0;
			}
		/*
		 * Return false if AllowGroups isn't empty and one of user's groups
		 * isn't listed there
		 */
		if (options.num_allow_groups > 0)
			if (!ga_match(options.allow_groups,
			    options.num_allow_groups)) {
				ga_free();
				log("User %.100s not allowed because none of user's groups are listed in AllowGroups",
				    pw->pw_name);
				return 0;
			}
		ga_free();
	}

#ifdef WITH_AIXAUTHENTICATE
	if (loginrestrictions(pw->pw_name, S_RLOGIN, NULL, &loginmsg) != 0) {
		if (loginmsg && *loginmsg) {
			/* Remove embedded newlines (if any) */
			char *p;
			for (p = loginmsg; *p; p++) {
				if (*p == '\n')
					*p = ' ';
			}
			/* Remove trailing newline */
			*--p = '\0';
			log("Login restricted for %s: %.100s", pw->pw_name, loginmsg);
		}
		return 0;
	}
#endif /* WITH_AIXAUTHENTICATE */

	/* We found no reason not to let this user try to log on... */
	return 1;
}

Authctxt *
authctxt_new(void)
{
	Authctxt *authctxt = xmalloc(sizeof(*authctxt));
	memset(authctxt, 0, sizeof(*authctxt));
	return authctxt;
}

void
auth_log(Authctxt *authctxt, int authenticated, char *method, char *info)
{
	void (*authlog) (const char *fmt,...) = verbose;
	char *authmsg;

	/* Raise logging level */
	if (authenticated == 1 ||
	    !authctxt->valid ||
	    authctxt->failures >= AUTH_FAIL_LOG ||
	    strcmp(method, "password") == 0)
		authlog = log;

	if (authctxt->postponed)
		authmsg = "Postponed";
	else
		authmsg = authenticated ? "Accepted" : "Failed";

	authlog("%s %s for %s%.100s from %.200s port %d%s",
	    authmsg,
	    method,
	    authctxt->valid ? "" : "illegal user ",
	    authctxt->user,
	    get_remote_ipaddr(),
	    get_remote_port(),
	    info);

#ifdef WITH_AIXAUTHENTICATE
	if (authenticated == 0 && strcmp(method, "password") == 0)
	    loginfailed(authctxt->user,
		get_canonical_hostname(options.verify_reverse_mapping),
		"ssh");
#endif /* WITH_AIXAUTHENTICATE */

}

/*
 * Check whether root logins are disallowed.
 */
int
auth_root_allowed(char *method)
{
	switch (options.permit_root_login) {
	case PERMIT_YES:
		return 1;
		break;
	case PERMIT_NO_PASSWD:
		if (strcmp(method, "password") != 0)
			return 1;
		break;
	case PERMIT_FORCED_ONLY:
		if (forced_command) {
			log("Root login accepted for forced command.");
			return 1;
		}
		break;
	}
	log("ROOT LOGIN REFUSED FROM %.200s", get_remote_ipaddr());
	return 0;
}


/*
 * Given a template and a passwd structure, build a filename
 * by substituting % tokenised options. Currently, %% becomes '%',
 * %h becomes the home directory and %u the username.
 *
 * This returns a buffer allocated by xmalloc.
 */
char *
expand_filename(const char *filename, struct passwd *pw)
{
	Buffer buffer;
	char *file;
	const char *cp;

	/*
	 * Build the filename string in the buffer by making the appropriate
	 * substitutions to the given file name.
	 */
	buffer_init(&buffer);
	for (cp = filename; *cp; cp++) {
		if (cp[0] == '%' && cp[1] == '%') {
			buffer_append(&buffer, "%", 1);
			cp++;
			continue;
		}
		if (cp[0] == '%' && cp[1] == 'h') {
			buffer_append(&buffer, pw->pw_dir, strlen(pw->pw_dir));
			cp++;
			continue;
		}
		if (cp[0] == '%' && cp[1] == 'u') {
			buffer_append(&buffer, pw->pw_name,
			    strlen(pw->pw_name));
			cp++;
			continue;
		}
		buffer_append(&buffer, cp, 1);
	}
	buffer_append(&buffer, "\0", 1);

	/*
	 * Ensure that filename starts anchored. If not, be backward
	 * compatible and prepend the '%h/'
	 */
	file = xmalloc(MAXPATHLEN);
	cp = buffer_ptr(&buffer);
	if (*cp != '/')
		snprintf(file, MAXPATHLEN, "%s/%s", pw->pw_dir, cp);
	else
		strlcpy(file, cp, MAXPATHLEN);

	buffer_free(&buffer);
	return file;
}

char *
authorized_keys_file(struct passwd *pw)
{
	return expand_filename(options.authorized_keys_file, pw);
}

char *
authorized_keys_file2(struct passwd *pw)
{
	return expand_filename(options.authorized_keys_file2, pw);
}

/* return ok if key exists in sysfile or userfile */
HostStatus
check_key_in_hostfiles(struct passwd *pw, Key *key, const char *host,
    const char *sysfile, const char *userfile)
{
	Key *found;
	char *user_hostfile;
	struct stat st;
	HostStatus host_status;

	/* Check if we know the host and its host key. */
	found = key_new(key->type);
	host_status = check_host_in_hostfile(sysfile, host, key, found, NULL);

	if (host_status != HOST_OK && userfile != NULL) {
		user_hostfile = tilde_expand_filename(userfile, pw->pw_uid);
		if (options.strict_modes &&
		    (stat(user_hostfile, &st) == 0) &&
		    ((st.st_uid != 0 && st.st_uid != pw->pw_uid) ||
		    (st.st_mode & 022) != 0)) {
			log("Authentication refused for %.100s: "
			    "bad owner or modes for %.200s",
			    pw->pw_name, user_hostfile);
		} else {
			temporarily_use_uid(pw);
			host_status = check_host_in_hostfile(user_hostfile,
			    host, key, found, NULL);
			restore_uid();
		}
		xfree(user_hostfile);
	}
	key_free(found);

	debug2("check_key_in_hostfiles: key %s for %s", host_status == HOST_OK ?
	    "ok" : "not found", host);
	return host_status;
}


/*
 * Check a given file for security. This is defined as all components
 * of the path to the file must be owned by either the owner of
 * of the file or root and no directories must be group or world writable.
 *
 * XXX Should any specific check be done for sym links ?
 *
 * Takes an open file descriptor, the file name, a uid and and
 * error buffer plus max size as arguments.
 *
 * Returns 0 on success and -1 on failure
 */
int
secure_filename(FILE *f, const char *file, struct passwd *pw,
    char *err, size_t errlen)
{
	uid_t uid = pw->pw_uid;
	char buf[MAXPATHLEN], homedir[MAXPATHLEN];
	char *cp;
	struct stat st;

	if (realpath(file, buf) == NULL) {
		snprintf(err, errlen, "realpath %s failed: %s", file,
		    strerror(errno));
		return -1;
	}
	if (realpath(pw->pw_dir, homedir) == NULL) {
		snprintf(err, errlen, "realpath %s failed: %s", pw->pw_dir,
		    strerror(errno));
		return -1;
	}

	/* check the open file to avoid races */
	if (fstat(fileno(f), &st) < 0 ||
	    (st.st_uid != 0 && st.st_uid != uid) ||
	    (st.st_mode & 022) != 0) {
		snprintf(err, errlen, "bad ownership or modes for file %s",
		    buf);
		return -1;
	}

	/* for each component of the canonical path, walking upwards */
	for (;;) {
		if ((cp = dirname(buf)) == NULL) {
			snprintf(err, errlen, "dirname() failed");
			return -1;
		}
		strlcpy(buf, cp, sizeof(buf));

		debug3("secure_filename: checking '%s'", buf);
		if (stat(buf, &st) < 0 ||
		    (st.st_uid != 0 && st.st_uid != uid) ||
		    (st.st_mode & 022) != 0) {
			snprintf(err, errlen,
			    "bad ownership or modes for directory %s", buf);
			return -1;
		}

		/* If are passed the homedir then we can stop */
		if (strcmp(homedir, buf) == 0) {
			debug3("secure_filename: terminating check at '%s'",
			    buf);
			break;
		}
		/*
		 * dirname should always complete with a "/" path,
		 * but we can be paranoid and check for "." too
		 */
		if ((strcmp("/", buf) == 0) || (strcmp(".", buf) == 0))
			break;
	}
	return 0;
}

struct passwd *
getpwnamallow(const char *user)
{
#ifdef HAVE_LOGIN_CAP
	extern login_cap_t *lc;
#ifdef BSD_AUTH
	auth_session_t *as;
#endif
#endif
	struct passwd *pw;

	pw = getpwnam(user);
	if (pw == NULL) {
		log("Illegal user %.100s from %.100s",
		    user, get_remote_ipaddr());
		return (NULL);
	}
	if (!allowed_user(pw))
		return (NULL);
#ifdef HAVE_LOGIN_CAP
	if ((lc = login_getclass(pw->pw_class)) == NULL) {
		debug("unable to get login class: %s", user);
		return (NULL);
	}
#ifdef BSD_AUTH
	if ((as = auth_open()) == NULL || auth_setpwd(as, pw) != 0 ||
	    auth_approval(as, lc, pw->pw_name, "ssh") <= 0) {
		debug("Approval failure for %s", user);
		pw = NULL;
	}
	if (as != NULL)
		auth_close(as);
#endif
#endif
	if (pw != NULL)
		return (pwcopy(pw));
	return (NULL);
}

void
auth_debug_add(const char *fmt,...)
{
	char buf[1024];
	va_list args;

	if (!auth_debug_init)
		return;

	va_start(args, fmt);
	vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);
	buffer_put_cstring(&auth_debug, buf);
}

void
auth_debug_send(void)
{
	char *msg;

	if (!auth_debug_init)
		return;
	while (buffer_len(&auth_debug)) {
		msg = buffer_get_string(&auth_debug, NULL);
		packet_send_debug("%s", msg);
		xfree(msg);
	}
}

void
auth_debug_reset(void)
{
	if (auth_debug_init)
		buffer_clear(&auth_debug);
	else {
		buffer_init(&auth_debug);
		auth_debug_init = 1;
	}
}
