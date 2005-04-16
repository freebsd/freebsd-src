/*-
 * Copyright (c) 2002-2005 Networks Associates Technology, Inc.
 * All rights reserved.
 *
 * This software was developed for the FreeBSD Project by Network Associates
 * Laboratories, the Security Research Division of Network Associates, Inc.
 * under DARPA/SPAWAR contract N66001-01-C-8035 ("CBOSS"), as part of the
 * DARPA CHATS research program.
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
#include <sys/param.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/sysctl.h>

#include <security/mac_bsdextended/mac_bsdextended.h>

#include <grp.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ugidfw.h"

/*
 * Text format for rules: rules contain subjectand object elements, mode.
 * Each element takes the form "[not] [uid number] [gid number]".
 * The total form is "subject [element] object [element] mode [mode]".
 * At least * one of a uid or gid entry must be present; both may also be
 * present.
 */

#define	MIB	"security.mac.bsdextended"

int
bsde_rule_to_string(struct mac_bsdextended_rule *rule, char *buf, size_t buflen)
{
	struct group *grp;
	struct passwd *pwd;
	char *cur;
	size_t left, len;
	int anymode, unknownmode, truncated;

	cur = buf;
	left = buflen;
	truncated = 0;

	if (rule->mbr_subject.mbi_flags & (MBI_UID_DEFINED |
	    MBI_GID_DEFINED)) {
		len = snprintf(cur, left, "subject ");
		if (len < 0 || len > left)
			goto truncated;
		left -= len;
		cur += len;

		if (rule->mbr_subject.mbi_flags & MBI_NEGATED) {
			len = snprintf(cur, left, "not ");
			if (len < 0 || len > left)
				goto truncated;
			left -= len;
			cur += len;
		}
		if (rule->mbr_subject.mbi_flags & MBI_UID_DEFINED) {
			pwd = getpwuid(rule->mbr_subject.mbi_uid);
			if (pwd != NULL) {
				len = snprintf(cur, left, "uid %s ",
				    pwd->pw_name);
				if (len < 0 || len > left)
					goto truncated;
				left -= len;
				cur += len;
			} else {
				len = snprintf(cur, left, "uid %u ",
				    rule->mbr_subject.mbi_uid);
				if (len < 0 || len > left)
					goto truncated;
				left -= len;
				cur += len;
			}
		}
		if (rule->mbr_subject.mbi_flags & MBI_GID_DEFINED) {
			grp = getgrgid(rule->mbr_subject.mbi_gid);
			if (grp != NULL) {
				len = snprintf(cur, left, "gid %s ",
				    grp->gr_name);
				if (len < 0 || len > left)
					goto truncated;
				left -= len;
				cur += len;
			} else {
				len = snprintf(cur, left, "gid %u ",
				    rule->mbr_subject.mbi_gid);
				if (len < 0 || len > left)
					goto truncated;
				left -= len;
				cur += len;
			}
		}
	}
	if (rule->mbr_object.mbi_flags & (MBI_UID_DEFINED |
	    MBI_GID_DEFINED)) {
		len = snprintf(cur, left, "object ");
		if (len < 0 || len > left)
			goto truncated;
		left -= len;
		cur += len;

		if (rule->mbr_object.mbi_flags & MBI_NEGATED) {
			len = snprintf(cur, left, "not ");
			if (len < 0 || len > left)
				goto truncated;
			left -= len;
			cur += len;
		}
		if (rule->mbr_object.mbi_flags & MBI_UID_DEFINED) {
			pwd = getpwuid(rule->mbr_object.mbi_uid);
			if (pwd != NULL) {
				len = snprintf(cur, left, "uid %s ",
				    pwd->pw_name);
				if (len < 0 || len > left)
					goto truncated;
				left -= len;
				cur += len;
			} else {
				len = snprintf(cur, left, "uid %u ",
				    rule->mbr_object.mbi_uid);
				left -= len;
				cur += len;
			}
		}
		if (rule->mbr_object.mbi_flags & MBI_GID_DEFINED) {
			grp = getgrgid(rule->mbr_object.mbi_gid);
			if (grp != NULL) {
				len = snprintf(cur, left, "gid %s ",
				    grp->gr_name);
				if (len < 0 || len > left)
					goto truncated;
				left -= len;
				cur += len;
			} else {
				len = snprintf(cur, left, "gid %u ",
				    rule->mbr_object.mbi_gid);
				if (len < 0 || len > left)
					goto truncated;
				left -= len;
				cur += len;
			}
		}
	}

	len = snprintf(cur, left, "mode ");
	if (len < 0 || len > left)
		goto truncated;
	left -= len;
	cur += len;

	anymode = (rule->mbr_mode & MBI_ALLPERM);
	unknownmode = (rule->mbr_mode & ~MBI_ALLPERM);

	if (rule->mbr_mode & MBI_ADMIN) {
		len = snprintf(cur, left, "a");
		if (len < 0 || len > left)
			goto truncated;

		left -= len;
		cur += len;
	}
	if (rule->mbr_mode & MBI_READ) {
		len = snprintf(cur, left, "r");
		if (len < 0 || len > left)
			goto truncated;

		left -= len;
		cur += len;
	}
	if (rule->mbr_mode & MBI_STAT) {
		len = snprintf(cur, left, "s");
		if (len < 0 || len > left)
			goto truncated;

		left -= len;
		cur += len;
	}
	if (rule->mbr_mode & MBI_WRITE) {
		len = snprintf(cur, left, "w");
		if (len < 0 || len > left)
			goto truncated;

		left -= len;
		cur += len;
	}
	if (rule->mbr_mode & MBI_EXEC) {
		len = snprintf(cur, left, "x");
		if (len < 0 || len > left)
			goto truncated;

		left -= len;
		cur += len;
	}
	if (!anymode) {
		len = snprintf(cur, left, "n");
		if (len < 0 || len > left)
			goto truncated;

		left -= len;
		cur += len;
	}
	if (unknownmode) {
		len = snprintf(cur, left, "?");
		if (len < 0 || len > left)
			goto truncated;

		left -= len;
		cur += len;
	}

	return (0);

truncated:
	return (-1);
}

int
bsde_parse_identity(int argc, char *argv[],
    struct mac_bsdextended_identity *identity, size_t buflen, char *errstr)
{
	struct group *grp;
	struct passwd *pwd;
	int uid_seen, gid_seen, not_seen;
	int current;
	char *endp;
	long value;
	uid_t uid;
	gid_t gid;
	size_t len;

	if (argc == 0) {
		len = snprintf(errstr, buflen, "Identity must not be empty");
		return (-1);
	}

	current = 0;

	/* First element might be "not". */
	if (strcmp("not", argv[0]) == 0) {
		not_seen = 1;
		current++;
	} else
		not_seen = 0;

	if (current >= argc) {
		len = snprintf(errstr, buflen, "Identity short");
		return (-1);
	}

	uid_seen = 0;
	uid = 0;
	gid_seen = 0;
	gid = 0;

	/* First phrase: uid [uid] or gid[gid]. */
	if (strcmp("uid", argv[current]) == 0) {
		if (current + 2 > argc) {
			len = snprintf(errstr, buflen, "uid short");
			return (-1);
		}
		pwd = getpwnam(argv[current+1]);
		if (pwd != NULL)
			uid = pwd->pw_uid;
		else {
			value = strtol(argv[current+1], &endp, 10);
			if (*endp != '\0') {
				len = snprintf(errstr, buflen,
				    "invalid uid: '%s'",
				    argv[current+1]);
				return (-1);
			}
			uid = value;
		}
		uid_seen = 1;
		current += 2;
	} else if (strcmp("gid", argv[current]) == 0) {
		if (current + 2 > argc) {
			len = snprintf(errstr, buflen, "gid short");
			return (-1);
		}
		grp = getgrnam(argv[current+1]);
		if (grp != NULL)
			gid = grp->gr_gid;
		else {
			value = strtol(argv[current+1], &endp, 10);
			if (*endp != '\0') {
				len = snprintf(errstr, buflen,
				    "invalid gid: '%s'",
				    argv[current+1]);
				return (-1);
			}
			gid = value;
		}
		gid_seen = 1;
		current += 2;
	} else {
		len = snprintf(errstr, buflen, "'%s' not expected",
		    argv[current]);
		return (-1);
	}

	/* Onto optional second phrase. */
	if (current + 1 < argc) {
		/* Second phrase: uid [uid] or gid [gid], but not a repeat. */
		if (strcmp("uid", argv[current]) == 0) {
			if (uid_seen) {
				len = snprintf(errstr, buflen,
				    "Only one uid permitted per identity clause");
				return (-1);
			}
			if (current + 2 > argc) {
				len = snprintf(errstr, buflen, "uid short");
				return (-1);
			}
			pwd = getpwnam(argv[current+1]);
			if (pwd != NULL)
				uid = pwd->pw_uid;
			else {
				value = strtol(argv[current+1], &endp, 10);
				if (*endp != '\0') {
					len = snprintf(errstr, buflen,
					    "invalid uid: '%s'",
					    argv[current+1]);
					return (-1);
				}
				uid = value;
			}
			uid_seen = 1;
			current += 2;
		} else if (strcmp("gid", argv[current]) == 0) {
			if (gid_seen) {
				len = snprintf(errstr, buflen,
				    "Only one gid permitted per identity clause");
				return (-1);
			}
			if (current + 2 > argc) {
				len = snprintf(errstr, buflen, "gid short");
				return (-1);
			}
			grp = getgrnam(argv[current+1]);
			if (grp != NULL)
				gid = grp->gr_gid;
			else {
				value = strtol(argv[current+1], &endp, 10);
				if (*endp != '\0') {
					len = snprintf(errstr, buflen,
					    "invalid gid: '%s'",
					    argv[current+1]);
					return (-1);
				}
				gid = value;
			}
			gid_seen = 1;
			current += 2;
		} else {
			len = snprintf(errstr, buflen, "'%s' not expected",
			    argv[current]);
			return (-1);
		} 
	}

	if (current +1 < argc) {
		len = snprintf(errstr, buflen, "'%s' not expected",
		    argv[current]);
		return (-1);
	}

	/* Fill out the identity. */
	identity->mbi_flags = 0;

	if (not_seen)
		identity->mbi_flags |= MBI_NEGATED;

	if (uid_seen) {
		identity->mbi_flags |= MBI_UID_DEFINED;
		identity->mbi_uid = uid;
	} else
		identity->mbi_uid = 0;

	if (gid_seen) {
		identity->mbi_flags |= MBI_GID_DEFINED;
		identity->mbi_gid = gid;
	} else
		identity->mbi_gid = 0;

	return (0);
}

int
bsde_parse_mode(int argc, char *argv[], mode_t *mode, size_t buflen,
    char *errstr)
{
	size_t len;
	int i;

	if (argc == 0) {
		len = snprintf(errstr, buflen, "mode expects mode value");
		return (-1);
	}

	if (argc != 1) {
		len = snprintf(errstr, buflen, "'%s' unexpected", argv[1]);
		return (-1);
	}

	*mode = 0;
	for (i = 0; i < strlen(argv[0]); i++) {
		switch (argv[0][i]) {
		case 'a':
			*mode |= MBI_ADMIN;
			break;
		case 'r':
			*mode |= MBI_READ;
			break;
		case 's':
			*mode |= MBI_STAT;
			break;
		case 'w':
			*mode |= MBI_WRITE;
			break;
		case 'x':
			*mode |= MBI_EXEC;
			break;
		case 'n':
			/* ignore */
			break;
		default:
			len = snprintf(errstr, buflen, "Unknown mode letter: %c",
			    argv[0][i]);
			return (-1);
		} 
	}

	return (0);
}

int
bsde_parse_rule(int argc, char *argv[], struct mac_bsdextended_rule *rule,
    size_t buflen, char *errstr)
{
	int subject, subject_elements, subject_elements_length;
	int object, object_elements, object_elements_length;
	int mode, mode_elements, mode_elements_length;
	int error, i;
	size_t len;

	bzero(rule, sizeof(*rule));

	if (argc < 1) {
		len = snprintf(errstr, buflen, "Rule must begin with subject");
		return (-1);
	}

	if (strcmp(argv[0], "subject") != 0) {
		len = snprintf(errstr, buflen, "Rule must begin with subject");
		return (-1);
	}
	subject = 0;
	subject_elements = 1;

	/* Search forward for object. */

	object = -1;
	for (i = 1; i < argc; i++)
		if (strcmp(argv[i], "object") == 0)
			object = i;

	if (object == -1) {
		len = snprintf(errstr, buflen, "Rule must contain an object");
		return (-1);
	}

	/* Search forward for mode. */
	mode = -1;
	for (i = object; i < argc; i++)
		if (strcmp(argv[i], "mode") == 0)
			mode = i;

	if (mode == -1) {
		len = snprintf(errstr, buflen, "Rule must contain mode");
		return (-1);
	}

	subject_elements_length = object - subject - 1;
	object_elements = object + 1;
	object_elements_length = mode - object_elements;
	mode_elements = mode + 1;
	mode_elements_length = argc - mode_elements;

	error = bsde_parse_identity(subject_elements_length,
	    argv + subject_elements, &rule->mbr_subject, buflen, errstr);
	if (error)
		return (-1);

	error = bsde_parse_identity(object_elements_length,
	    argv + object_elements, &rule->mbr_object, buflen, errstr);
	if (error)
		return (-1);

	error = bsde_parse_mode(mode_elements_length, argv + mode_elements,
	    &rule->mbr_mode, buflen, errstr);
	if (error)
		return (-1);

	return (0);
}

int
bsde_parse_rule_string(const char *string, struct mac_bsdextended_rule *rule,
    size_t buflen, char *errstr)
{
	char *stringdup, *stringp, *argv[20], **ap;
	int argc, error;

	stringp = stringdup = strdup(string);
	while (*stringp == ' ' || *stringp == '\t')
		stringp++;

	argc = 0;
	for (ap = argv; (*ap = strsep(&stringp, " \t")) != NULL;) {
		argc++;
		if (**ap != '\0')
			if (++ap >= &argv[20])
				break;
	}

	error = bsde_parse_rule(argc, argv, rule, buflen, errstr);

	free(stringdup);

	return (error);
}

int
bsde_get_mib(const char *string, int *name, size_t *namelen)
{
	size_t len;
	int error;

	len = *namelen;
	error = sysctlnametomib(string, name, &len);
	if (error)
		return (error);

	*namelen = len;
	return (0);
}

int
bsde_get_rule_count(size_t buflen, char *errstr)
{
	size_t len;
	int error;
	int rule_count;

	len = sizeof(rule_count);
	error = sysctlbyname(MIB ".rule_count", &rule_count, &len, NULL, 0);
	if (error) {
		len = snprintf(errstr, buflen, strerror(errno));
		return (-1);
	}
	if (len != sizeof(rule_count)) {
		len = snprintf(errstr, buflen, "Data error in %s.rule_count",
		    MIB);
		return (-1);
	}

	return (rule_count);
}

int
bsde_get_rule_slots(size_t buflen, char *errstr)
{
	size_t len;
	int error;
	int rule_slots;

	len = sizeof(rule_slots);
	error = sysctlbyname(MIB ".rule_slots", &rule_slots, &len, NULL, 0);
	if (error) {
		len = snprintf(errstr, buflen, strerror(errno));
		return (-1);
	}
	if (len != sizeof(rule_slots)) {
		len = snprintf(errstr, buflen, "Data error in %s.rule_slots",
		    MIB);
		return (-1);
	}

	return (rule_slots);
}

/*
 * Returns 0 for success;
 * Returns -1 for failure;
 * Returns -2 for not present
 */
int
bsde_get_rule(int rulenum, struct mac_bsdextended_rule *rule, size_t errlen,
    char *errstr)
{
	int name[10];
	size_t len, size;
	int error;

	len = 10;
	error = bsde_get_mib(MIB ".rules", name, &len);
	if (error) {
		len = snprintf(errstr, errlen, "%s: %s", MIB ".rules",
		    strerror(errno));
		return (-1);
	}

	size = sizeof(*rule);
	name[len] = rulenum;
	len++;
	error = sysctl(name, len, rule, &size, NULL, 0);
	if (error  == -1 && errno == ENOENT)
		return (-2);
	if (error) {
		len = snprintf(errstr, errlen, "%s.%d: %s", MIB ".rules",
		    rulenum, strerror(errno));
		return (-1);
	} else if (size != sizeof(*rule)) {
		len = snprintf(errstr, errlen, "Data error in %s.%d: %s",
		    MIB ".rules", rulenum, strerror(errno));
		return (-1);
	}

	return (0);
}

int
bsde_delete_rule(int rulenum, size_t buflen, char *errstr)
{
	struct mac_bsdextended_rule rule;
	int name[10];
	size_t len, size;
	int error;

	len = 10;
	error = bsde_get_mib(MIB ".rules", name, &len);
	if (error) {
		len = snprintf(errstr, buflen, "%s: %s", MIB ".rules",
		    strerror(errno));
		return (-1);
	}

	name[len] = rulenum;
	len++;

	size = sizeof(rule);
	error = sysctl(name, len, NULL, NULL, &rule, 0);
	if (error) {
		len = snprintf(errstr, buflen, "%s.%d: %s", MIB ".rules",
		    rulenum, strerror(errno));
		return (-1);
	}

	return (0);
}

int
bsde_set_rule(int rulenum, struct mac_bsdextended_rule *rule, size_t buflen,
    char *errstr)
{
	int name[10];
	size_t len, size;
	int error;

	len = 10;
	error = bsde_get_mib(MIB ".rules", name, &len);
	if (error) {
		len = snprintf(errstr, buflen, "%s: %s", MIB ".rules",
		    strerror(errno));
		return (-1);
	}

	name[len] = rulenum;
	len++;

	size = sizeof(*rule);
	error = sysctl(name, len, NULL, NULL, rule, size);
	if (error) {
		len = snprintf(errstr, buflen, "%s.%d: %s", MIB ".rules",
		    rulenum, strerror(errno));
		return (-1);
	}

	return (0);
}

int
bsde_add_rule(int *rulenum, struct mac_bsdextended_rule *rule, size_t buflen,
    char *errstr)
{
	char charstr[BUFSIZ];
	int name[10];
	size_t len, size;
	int error, rule_slots;

	len = 10;
	error = bsde_get_mib(MIB ".rules", name, &len);
	if (error) {
		len = snprintf(errstr, buflen, "%s: %s", MIB ".rules",
		    strerror(errno));
		return (-1);
	}

	rule_slots = bsde_get_rule_slots(BUFSIZ, charstr);
	if (rule_slots == -1) {
		len = snprintf(errstr, buflen, "unable to get rule slots: %s",
		    strerror(errno));
		return (-1);
	}

	name[len] = rule_slots;
	len++;

	size = sizeof(*rule);
	error = sysctl(name, len, NULL, NULL, rule, size);
	if (error) {
		len = snprintf(errstr, buflen, "%s.%d: %s", MIB ".rules",
		    rule_slots, strerror(errno));
		return (-1);
	}

	if (rulenum != NULL)
		*rulenum = rule_slots;

	return (0);
}
