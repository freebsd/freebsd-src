/*-
 * Copyright (c) 2000 Robert N. M. Watson
 * All rights reserved.
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
/*
 * TrustedBSD Project - support for POSIX.1e process capabilities
 */

#include <sys/types.h>
#include "namespace.h"
#include <sys/capability.h>
#include "un-namespace.h"
#include <sys/errno.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const size_t CAP_MAX_BUF_LEN = 1024;
static const size_t CAP_MAX_SMALL_BUF_LEN = 64;

static const char *CAP_FLAGS[8] = {
	"",	/* 000 */
	"e",	/* 001 */
	"i",	/* 010 */
	"ei",	/* 011 */
	"p",	/* 100 */
	"ep",	/* 101 */
	"ip",	/* 110 */
	"eip",	/* 111 */
};

static const char *CAP_SEP = ": \t";
static const char *CAP_OPERATION = "=-+";

struct cap_info {
	char	*ci_name;
	cap_value_t	ci_cap;
};

static const struct cap_info cap_info_array[] = {
{"CAP_CHOWN", CAP_CHOWN},
{"CAP_DAC_EXECUTE", CAP_DAC_EXECUTE},
{"CAP_DAC_WRITE", CAP_DAC_WRITE},
{"CAP_DAC_READ_SEARCH", CAP_DAC_READ_SEARCH},
{"CAP_FOWNER", CAP_FOWNER},
{"CAP_FSETID", CAP_FSETID},
{"CAP_KILL", CAP_KILL},
{"CAP_LINK_DIR", CAP_LINK_DIR},
{"CAP_SETFCAP", CAP_SETFCAP},
{"CAP_SETGID", CAP_SETGID},
{"CAP_SETUID", CAP_SETUID},
{"CAP_MAC_DOWNGRADE", CAP_MAC_DOWNGRADE},
{"CAP_MAC_READ", CAP_MAC_READ},
{"CAP_MAC_RELABEL_SUBJ", CAP_MAC_RELABEL_SUBJ},
{"CAP_MAC_UPGRADE", CAP_MAC_UPGRADE},
{"CAP_MAC_WRITE", CAP_MAC_WRITE},
{"CAP_INF_NOFLOAT_OBJ", CAP_INF_NOFLOAT_OBJ},
{"CAP_INF_NOFLOAT_SUBJ", CAP_INF_NOFLOAT_SUBJ},
{"CAP_INF_RELABEL_OBJ", CAP_INF_RELABEL_OBJ},
{"CAP_INF_RELABEL_SUBJ", CAP_INF_RELABEL_SUBJ},
{"CAP_AUDIT_CONTROL", CAP_AUDIT_CONTROL},
{"CAP_AUDIT_WRITE", CAP_AUDIT_WRITE},
{"CAP_SETPCAP", CAP_SETPCAP},
{"CAP_SYS_SETFFLAG", CAP_SYS_SETFFLAG},
{"CAP_LINUX_IMMUTABLE", CAP_SYS_SETFFLAG},
{"CAP_NET_BIND_SERVICE", CAP_NET_BIND_SERVICE},
{"CAP_NET_BROADCAST", CAP_NET_BROADCAST},
{"CAP_NET_ADMIN", CAP_NET_ADMIN},
{"CAP_NET_RAW", CAP_NET_RAW},
{"CAP_IPC_LOCK", CAP_IPC_LOCK},
{"CAP_IPC_OWNER", CAP_IPC_OWNER},
{"CAP_SYS_MODULE", CAP_SYS_MODULE},
{"CAP_SYS_RAWIO", CAP_SYS_RAWIO},
{"CAP_SYS_CHROOT", CAP_SYS_CHROOT},
{"CAP_SYS_PTRACE", CAP_SYS_PTRACE},
{"CAP_SYS_PACCT", CAP_SYS_PACCT},
{"CAP_SYS_ADMIN", CAP_SYS_ADMIN},
{"CAP_SYS_BOOT", CAP_SYS_BOOT},
{"CAP_SYS_NICE", CAP_SYS_NICE},
{"CAP_SYS_RESOURCE", CAP_SYS_RESOURCE},
{"CAP_SYS_TIME", CAP_SYS_TIME},
{"CAP_SYS_TTY_CONFIG", CAP_SYS_TTY_CONFIG},
{"CAP_MKNOD", CAP_MKNOD},
{"", CAP_ALL_OFF},
{"all", CAP_ALL_ON},
};

static const int cap_info_array_len = sizeof(cap_info_array) /
    sizeof(cap_info_array[0]);

static const cap_value_t cap_list[] = {
CAP_CHOWN,
CAP_DAC_EXECUTE,
CAP_DAC_WRITE,
CAP_DAC_READ_SEARCH,
CAP_FOWNER,
CAP_FSETID,
CAP_KILL,
CAP_LINK_DIR,
CAP_SETFCAP,
CAP_SETGID,
CAP_SETUID,
CAP_MAC_DOWNGRADE,
CAP_MAC_READ,
CAP_MAC_RELABEL_SUBJ,
CAP_MAC_UPGRADE,
CAP_MAC_WRITE,
CAP_INF_NOFLOAT_OBJ,
CAP_INF_NOFLOAT_SUBJ,
CAP_INF_RELABEL_OBJ,
CAP_INF_RELABEL_SUBJ,
CAP_AUDIT_CONTROL,
CAP_AUDIT_WRITE,
CAP_SETPCAP,
CAP_SYS_SETFFLAG,
CAP_NET_BIND_SERVICE,
CAP_NET_BROADCAST,
CAP_NET_ADMIN,
CAP_NET_RAW,
CAP_IPC_LOCK,
CAP_IPC_OWNER,
CAP_SYS_MODULE,
CAP_SYS_RAWIO,
CAP_SYS_CHROOT,
CAP_SYS_PTRACE,
CAP_SYS_PACCT,
CAP_SYS_ADMIN,
CAP_SYS_BOOT,
CAP_SYS_NICE,
CAP_SYS_RESOURCE,
CAP_SYS_TIME,
CAP_SYS_TTY_CONFIG,
CAP_MKNOD,
};

static const int cap_list_len = sizeof(cap_list) / sizeof(cap_list[0]);

static void
cap_set(cap_t cap_p, cap_flag_t flags, cap_flag_value_t fvalue,
    cap_value_t cap_value)
{

	if (flags & CAP_EFFECTIVE) {
		if (fvalue == CAP_SET)
			cap_p->c_effective |= cap_value;
		else
			cap_p->c_effective &= ~cap_value;
	}
	if (flags & CAP_INHERITABLE) {
		if (fvalue == CAP_SET)
			cap_p->c_inheritable |= cap_value;
		else
			cap_p->c_inheritable &= ~cap_value;
	}
	if (flags & CAP_PERMITTED) {
		if (fvalue == CAP_SET)
			cap_p->c_permitted |= cap_value;
		else
			cap_p->c_permitted &= ~cap_value;
	}
}

static int
cap_is_set(cap_t cap_p, cap_flag_t cap_flag, cap_value_t cap_value)
{
	int	seen = 0;

	if (cap_flag & CAP_EFFECTIVE)
		seen |= (cap_p->c_effective & cap_value);
	if (cap_flag & CAP_INHERITABLE)
		seen |= (cap_p->c_inheritable & cap_value);
	if (cap_flag & CAP_PERMITTED)
		seen |= (cap_p->c_permitted & cap_value);

	return (seen);
}

static cap_flag_value_t
cap_value_to_flags(cap_t cap_p, cap_value_t cap_value)
{
	cap_flag_t	flags = 0;

	if (cap_p->c_effective & cap_value)
		flags |= CAP_EFFECTIVE;
	if (cap_p->c_inheritable & cap_value)
		flags |= CAP_INHERITABLE;
	if (cap_p->c_permitted & cap_value)
		flags |= CAP_PERMITTED;

	return (flags);
}

static const char *
cap_flags_to_string(cap_flag_t flags)
{

	return (CAP_FLAGS[flags]);
}

static int
cap_string_to_flags(const char *string, cap_flag_t *flags)
{
	const char	*c = string;

	*flags = 0;
	while (*c != '\0') {
		switch (*c) {
		case 'e':
			*flags |= CAP_EFFECTIVE;
			break;
		case 'i':
			*flags |= CAP_INHERITABLE;
			break;
		case 'p':
			*flags |= CAP_PERMITTED;
			break;
		default:
			return (EINVAL);
		}
		c++;
	}

	return (0);
}

static const char *
cap_to_string(cap_value_t cap)
{
	int	i;

	for (i = 0; i < cap_info_array_len; i++) {
		if (cap_info_array[i].ci_cap == cap)
			return (cap_info_array[i].ci_name);
	}

	return (NULL);
}

static int
cap_from_string(const char *string, cap_value_t *cap)
{
	int	i;

	for (i = 0; i < cap_info_array_len; i++) {
		if (!strcasecmp(cap_info_array[i].ci_name, string)) {
			*cap = cap_info_array[i].ci_cap;
			return (0);
		}
	}

	return (EINVAL);
}

char *
cap_to_text(cap_t cap_p, ssize_t *len_p)
{
	cap_value_t	cap_value;
	cap_flag_t	cap_flag, most_flag;
	const char	*flag_s, *value_s, *prefix_s;
	char	*buf, minibuf[CAP_MAX_SMALL_BUF_LEN], operation;

	int	num_effective, num_inheritable, num_permitted;
	int	most_effective, most_inheritable, most_permitted;
	int	count, any_so_far;

	buf = (char *)malloc(CAP_MAX_BUF_LEN);
	if (buf == NULL) {
		errno = ENOMEM;
		return (NULL);
	}
	buf[0] = '\0';

	/*
	 * For the sake of prettiness, first walk each flag to see if it's
	 * set for cap_list_len/2 or more.  If so, list it as a plus, and
	 * do the remainder as negative, as needed.  This will tend to
	 * collapse a lot of the common all= cases.
	 */
	num_effective = num_inheritable = num_permitted = 0;
	for (count = 0; count < cap_list_len; count++) {
		cap_value = cap_list[count];
		if (cap_is_set(cap_p, CAP_EFFECTIVE, cap_value))
			num_effective++;
		if (cap_is_set(cap_p, CAP_INHERITABLE, cap_value))
			num_inheritable++;
		if (cap_is_set(cap_p, CAP_PERMITTED, cap_value))
			num_permitted++;
	}

	most_effective = (num_effective > cap_list_len / 2);
	most_inheritable = (num_inheritable > cap_list_len / 2);
	most_permitted = (num_permitted > cap_list_len / 2);

	most_flag = 0;
	if (most_effective)
		most_flag |= CAP_EFFECTIVE;
	if (most_inheritable)
		most_flag |= CAP_INHERITABLE;
	if (most_permitted)
		most_flag |= CAP_PERMITTED;

	any_so_far = 0;
	if (most_flag != 0) {
		if ((strlcat(buf, "all=", CAP_MAX_BUF_LEN) >=
		    CAP_MAX_BUF_LEN) ||
		    (strlcat(buf, CAP_FLAGS[most_flag],
		    CAP_MAX_BUF_LEN) >= CAP_MAX_BUF_LEN)) {
			free(buf);
			errno = ENOMEM;
			return (NULL);
		}
		any_so_far = 1;
	}

	/*
	 * For each capability value, determine how that value relates
	 * to the most common case, and (depending on CAP_PRINT_RELATIVE)
	 * either print out the value's flag set relative to the most
	 * common, or its absolute flag set.
	 */
	for (count = 0; count < cap_list_len; count++) {
		cap_value = cap_list[count];
		cap_flag = cap_value_to_flags(cap_p, cap_value);
		/*
		 * Determine which, if any, flags need to be printed
		 * for this capability.  First, if the flags on the
		 * capability are equal to the "most" flags, just skip
		 * it.
		 */
		if (cap_flag == most_flag)
			continue;

#if CAP_PRINT_RELATIVE
		/*
		 * If the flags are a strict superset of the "most"
		 * flags, print it as a "+" case.  If they're a
		 * strict subset, print as a "-" case.  Otherwise,
		 * specify as an "=" case.
		 */
		if ((cap_flag | most_flag) == cap_flag) {
			/* Strict superset, use "+". */
			operation = '+';
			cap_flag = cap_flag & ~most_flag;
			flag_s = cap_flags_to_string(cap_flag);
		} else if ((cap_flag | most_flag) == most_flag) {
			/* Strict subset, use "-". */
			operation = '-';
			cap_flag = most_flag & ~cap_flag;
			flag_s = cap_flags_to_string(cap_flag);
		} else {
#endif
			/* Mixed, use an "=" case */
			operation = '=';
			flag_s = cap_flags_to_string(cap_flag);
#if CAP_PRINT_RELATIVE
		}
#endif
		/*
		 * Now assemble clause, and append to the string being
		 * built.
		 */
		if (any_so_far)
			prefix_s = ":";
		else
			prefix_s = "";
		value_s = cap_to_string(cap_value);
		if ((snprintf(minibuf, sizeof(minibuf), "%s%s%c%s", prefix_s,
		    value_s, operation, flag_s) >= sizeof(minibuf)) ||
		    (strlcat(buf, minibuf, CAP_MAX_BUF_LEN) >=
		    CAP_MAX_BUF_LEN)) {
			free(buf);
			errno = ENOMEM;
			return (NULL);
		}
	}

	if (len_p)
		*len_p = strlen(buf);
	return (buf);
}

cap_t
cap_from_text(const char *buf_p)
{
	cap_value_t	cap_value_v, cap_value_set_v;
	cap_flag_t	cap_action_v;
	cap_t	cap;
	char	*mybuf, *cur;
	char	*clause_s, *cap_value_s, *cap_value_list_s;
	char	*cap_action_list_s, *cap_action_s;
	char	*next_operation_p, operation, next_operation;

	cap = cap_init();
	if (cap == NULL)
		return ((cap_t)NULL);

	mybuf = strdup(buf_p);
	if (mybuf == NULL) {
		errno = ENOMEM;
		goto err1;
	}

	/*
	 * clase [SEP clause [SEP clause ...]]
	 * Split into "clauses", which are separated by a : or whitespace.
	 *
	 * clause = [caplist]actionlist
	 * caplist = capabilityname[,capabilityname[, ...]]
	 * actionlist = op[flags][op[flags]]
	 * Split clauses into a (possibly null) capability name list, and a
	 * set of one or more {op,flags} pairs.
	 *
	 * Each assignment is then applied to a running "state" to
	 * produce an end-result in the internal representation.
	 * Parsing failure at any time releases resources and results
	 * in EINVAL.
	 */
	cur = mybuf;
	while ((clause_s = strsep(&cur, CAP_SEP)) != NULL) {
		/*
		 * Identify and NULL the first operation so that we
		 * can parse the capability name list, but save
		 * for later when we iterate over the operation list.
		 */
		cap_action_list_s = clause_s;
		next_operation_p = strpbrk(cap_action_list_s, CAP_OPERATION);
		if (next_operation_p == NULL)
			goto err2;
		operation = *next_operation_p;
		cap_value_list_s = strsep(&cap_action_list_s, CAP_OPERATION);
		if (cap_value_list_s == NULL || cap_action_list_s == NULL)
			goto err2;
		/*
		 * cap_value_list_s now points at the NULL-terminated list
		 * of capability values, if any.
		 * cap_action_list_s now points to the NULL-terminated list
		 * of actions.
		 *
		 * First, parse the value list to generate a value set
		 * refering to the combined contents of the value list.
		 */
		cap_value_set_v = 0;
		while ((cap_value_s = strsep(&cap_value_list_s, ",")) != NULL) {
			/*
			 * Convert value string into internal representation.
			 * Reject if not a valid capability identifier.
			 */
			if (cap_from_string(cap_value_s, &cap_value_v))
				goto err2;
			cap_value_set_v |= cap_value_v;
		}

		/*
		 * While the current operation is non-0, parse its flags,
		 * apply the actions, and then repeat.  The first set
		 * is assured above when the capability list is split off.
		 */
		while (operation != 0) {
			/*
			 * Identify and save the next operation, then NULL
			 * it to find the end of the current flags.
			 */
			next_operation_p = strpbrk(cap_action_list_s,
			    CAP_OPERATION);
			if (next_operation_p)
				next_operation = *next_operation_p;
			else
				next_operation = 0;
			cap_action_s = strsep(&cap_action_list_s,
			    CAP_OPERATION);
			/*
			 * Convert string form of flags to internal
			 * representation, reject if not possible.
			 */
			if (cap_string_to_flags(cap_action_s, &cap_action_v))
				goto err2;

			/*
			 * Now, based on operation apply actionlist flags
			 * to the capability value set built earlier from
			 * the capability list.
			 */
			switch (operation) {
			case '=':
				/*
				 * Remove current flags for the value set,
				 * replace with new flags.
				 *
				 * Spec requires that an "=" operation with
				 * no value set be treated as an "=" operation
				 * with a value set equivilent to "all".
				 */
				if (cap_value_set_v == CAP_ALL_OFF) {
					cap_set(cap, CAP_EFFECTIVE|
					    CAP_INHERITABLE|CAP_PERMITTED,
					    CAP_CLEAR, CAP_ALL_ON);
					cap_set(cap, cap_action_v, CAP_SET,
					    CAP_ALL_ON);
				} else {
					cap_set(cap, CAP_EFFECTIVE|
					    CAP_INHERITABLE|CAP_PERMITTED,
					    CAP_CLEAR, cap_value_set_v);
					cap_set(cap, cap_action_v, CAP_SET,
					    cap_value_set_v);
				}
				break;
			case '+':
				/*
				 * Add current flags to value set.
				 *
				 * Spec requires that a "+" operation with
				 * no value set be rejected.
				 */
				if (cap_value_set_v == CAP_ALL_OFF)
					goto err2;
				cap_set(cap, cap_action_v, CAP_SET,
				    cap_value_set_v);
				break;
			case '-':
				/*
				 * Subtract current flags from value set.
				 *
				 * Spec requires that a "-" operation with
				 * no value set be treated as a "-" operation
				 * with a value set equivilent to "all".
				 */
				if (cap_value_set_v == CAP_ALL_OFF)
					cap_set(cap, cap_action_v, CAP_CLEAR,
					    CAP_ALL_ON);
				else
					cap_set(cap, cap_action_v, CAP_CLEAR,
					    cap_value_set_v);
				break;
			default:
				goto err2;
			}
			operation = next_operation;
		}
	}

	return (cap);
 err2:
	errno = EINVAL;
	free(mybuf);
 err1:
	cap_free(cap);
	return ((cap_t)NULL);
}

