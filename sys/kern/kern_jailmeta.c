/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 SkunkWerks GmbH
 *
 * This software was developed by Igor Ostapenko <igoro@FreeBSD.org>
 * under sponsorship from SkunkWerks GmbH.
 */

#include <sys/param.h>
#include <sys/_bitset.h>
#include <sys/bitset.h>
#include <sys/lock.h>
#include <sys/sx.h>
#include <sys/kernel.h>
#include <sys/mount.h>
#include <sys/malloc.h>
#include <sys/jail.h>
#include <sys/osd.h>
#include <sys/proc.h>

/*
 * Buffer limit.
 *
 * The hard limit is the actual value used during setting or modification. The
 * soft limit is used solely by the security.jail.param.meta and .env sysctl. If
 * the hard limit is decreased, the soft limit may remain higher to ensure that
 * previously set meta strings can still be correctly interpreted by end-user
 * interfaces, such as jls(8).
 */

static uint32_t jm_maxbufsize_hard = 4096;
static uint32_t jm_maxbufsize_soft = 4096;

static int
jm_sysctl_meta_maxbufsize(SYSCTL_HANDLER_ARGS)
{
	int error;
	uint32_t newmax = 0;

	/* Reading only. */

	if (req->newptr == NULL) {
		sx_slock(&allprison_lock);
		error = SYSCTL_OUT(req, &jm_maxbufsize_hard,
		    sizeof(jm_maxbufsize_hard));
		sx_sunlock(&allprison_lock);

		return (error);
	}

	/* Reading and writing. */

	sx_xlock(&allprison_lock);

	error = SYSCTL_OUT(req, &jm_maxbufsize_hard,
	    sizeof(jm_maxbufsize_hard));
	if (error != 0)
		goto end;

	error = SYSCTL_IN(req, &newmax, sizeof(newmax));
	if (error != 0)
		goto end;

	jm_maxbufsize_hard = newmax;
	if (jm_maxbufsize_hard >= jm_maxbufsize_soft) {
		jm_maxbufsize_soft = jm_maxbufsize_hard;
	} else if (TAILQ_EMPTY(&allprison)) {
		/*
		 * For now, this is the simplest way to
		 * avoid O(n) iteration over all prisons in
		 * case of a large n.
		 */
		jm_maxbufsize_soft = jm_maxbufsize_hard;
	}

end:
	sx_xunlock(&allprison_lock);
	return (error);
}
SYSCTL_PROC(_security_jail, OID_AUTO, meta_maxbufsize,
    CTLTYPE_U32 | CTLFLAG_RW | CTLFLAG_MPSAFE, NULL, 0,
    jm_sysctl_meta_maxbufsize, "IU",
    "Maximum buffer size of each meta and env");


/* Jail parameter announcement. */

static int
jm_sysctl_param_meta(SYSCTL_HANDLER_ARGS)
{
	uint32_t soft;

	sx_slock(&allprison_lock);
	soft = jm_maxbufsize_soft;
	sx_sunlock(&allprison_lock);

	return (sysctl_jail_param(oidp, arg1, soft, req));
}
SYSCTL_PROC(_security_jail_param, OID_AUTO, meta,
    CTLTYPE_STRING | CTLFLAG_RW | CTLFLAG_MPSAFE, NULL, 0,
    jm_sysctl_param_meta, "A,keyvalue",
    "Jail meta information hidden from the jail");
SYSCTL_PROC(_security_jail_param, OID_AUTO, env,
    CTLTYPE_STRING | CTLFLAG_RW | CTLFLAG_MPSAFE, NULL, 0,
    jm_sysctl_param_meta, "A,keyvalue",
    "Jail meta information readable by the jail");


/* Generic OSD-based logic for any metadata buffer. */

struct meta {
	char *name;
	u_int osd_slot;
	osd_method_t methods[PR_MAXMETHOD];
};

/* A chain of hunks representing the final buffer after all manipulations. */
struct hunk {
	char *p;		/* a buf reference */
	size_t len;		/* number of bytes referred */
	char *owned;		/* must be freed */
	struct hunk *next;
};

static inline struct hunk *
jm_h_alloc(void)
{
	/* All fields are zeroed. */
	return (malloc(sizeof(struct hunk), M_PRISON, M_WAITOK | M_ZERO));
}

static inline struct hunk *
jm_h_prepend(struct hunk *h, char *p, size_t len)
{
	struct hunk *n;

	n = jm_h_alloc();
	n->p = p;
	n->len = len;
	n->next = h;
	return (n);
}

static inline void
jm_h_cut_line(struct hunk *h, char *begin)
{
	struct hunk *rem;
	char *end;

	/* Find the end of key=value. */
	for (end = begin; (end + 1) < (h->p + h->len); end++)
		if (*end == '\0' || *end == '\n')
			break;

	/* Pick up a non-empty remainder. */
	if ((end + 1) < (h->p + h->len) && *(end + 1) != '\0') {
		rem = jm_h_alloc();
		rem->p = end + 1;
		rem->len = h->p + h->len - rem->p;

		/* insert */
		rem->next = h->next;
		h->next = rem;
	}

	/* Shorten this hunk. */
	h->len = begin - h->p;
}

static inline void
jm_h_cut_occurrences(struct hunk *h, const char *key, size_t keylen)
{
	char *p = h->p;

#define nexthunk()					\
	do {						\
		h = h->next;				\
		p = (h == NULL) ? NULL : h->p;		\
	} while (0)

	while (p != NULL) {
		p = strnstr(p, key, h->len - (p - h->p));
		if (p == NULL) {
			nexthunk();
			continue;
		}
		if ((p == h->p || *(p - 1) == '\n') && p[keylen] == '=') {
			jm_h_cut_line(h, p);
			nexthunk();
			continue;
		}
		/* Continue with this hunk. */
		p += keylen;
		/* Empty? The next hunk then. */
		if ((p - h->p) >= h->len)
			nexthunk();
	}
}

static inline size_t
jm_h_len(struct hunk *h)
{
	size_t len = 0;
	while (h != NULL) {
		len += h->len;
		h = h->next;
	}
	return (len);
}

static inline void
jm_h_assemble(char *dst, struct hunk *h)
{
	while (h != NULL) {
		if (h->len > 0) {
			memcpy(dst, h->p, h->len);
			dst += h->len;
			/* If not the last hunk then concatenate with \n. */
			if (h->next != NULL && *(dst - 1) == '\0')
				*(dst - 1) = '\n';
		}
		h = h->next;
	}
}

static inline struct hunk *
jm_h_freechain(struct hunk *h)
{
	struct hunk *n = h;
	while (n != NULL) {
		h = n;
		n = h->next;
		free(h->owned, M_PRISON);
		free(h, M_PRISON);
	}

	return (NULL);
}

static int
jm_osd_method_set(void *obj, void *data, const struct meta *meta)
{
	struct prison *pr = obj;
	struct vfsoptlist *opts = data;
	struct vfsopt *opt;

	char *origosd;
	char *origosd_copy;
	char *oldosd;
	char *osd;
	size_t osdlen;
	struct hunk *h;
	char *key;
	size_t keylen;
	int error;
	int repeats = 0;
	bool repeat;

	sx_assert(&allprison_lock, SA_XLOCKED);

again:
	origosd = NULL;
	origosd_copy = NULL;
	osd = NULL;
	h = NULL;
	error = 0;
	repeat = false;
	TAILQ_FOREACH(opt, opts, link) {
		/* Look for options with <metaname> prefix. */
		if (strstr(opt->name, meta->name) != opt->name)
			continue;
		/* Consider only full <metaname> or <metaname>.* ones. */
		if (opt->name[strlen(meta->name)] != '.' &&
		    opt->name[strlen(meta->name)] != '\0')
			continue;
		opt->seen = 1;

		/* The very first preconditions. */
		if (opt->len < 0)
			continue;
		if (opt->len > jm_maxbufsize_hard) {
			error = EFBIG;
			break;
		}
		/* NULL-terminated strings are expected from vfsopt. */
		if (opt->value != NULL &&
		    ((char *)opt->value)[opt->len - 1] != '\0') {
			error = EINVAL;
			break;
		}

		/* Work with our own copy of existing metadata. */
		if (h == NULL) {
			h = jm_h_alloc(); /* zeroed */
			mtx_lock(&pr->pr_mtx);
			origosd = osd_jail_get(pr, meta->osd_slot);
			if (origosd != NULL) {
				origosd_copy = malloc(strlen(origosd) + 1,
				    M_PRISON, M_NOWAIT);
				if (origosd_copy == NULL)
					error = ENOMEM;
				else {
					h->p = origosd_copy;
					h->len = strlen(origosd) + 1;
					memcpy(h->p, origosd, h->len);
				}
			}
			mtx_unlock(&pr->pr_mtx);
			if (error != 0)
				break;
		}

		/* 1) Change the whole metadata. */
		if (strcmp(opt->name, meta->name) == 0) {
			if (opt->len > jm_maxbufsize_hard) {
				error = EFBIG;
				break;
			}
			h = jm_h_freechain(h);
			h = jm_h_prepend(h,
			    (opt->value != NULL) ? opt->value : "",
			    /* avoid empty NULL-terminated string */
			    (opt->len > 1) ? opt->len : 0);
			continue;
		}

		/* 2) Or add/replace/remove a specific key=value. */
		key = opt->name + strlen(meta->name) + 1;
		keylen = strlen(key);
		if (keylen < 1) {
			error = EINVAL;
			break;
		}
		jm_h_cut_occurrences(h, key, keylen);
		if (opt->value == NULL)
			continue; /* key removal */
		h = jm_h_prepend(h, NULL, 0);
		h->len = keylen + 1 + opt->len; /* key=value\0 */
		h->owned = malloc(h->len, M_PRISON, M_WAITOK | M_ZERO);
		h->p = h->owned;
		memcpy(h->p, key, keylen);
		h->p[keylen] = '=';
		memcpy(h->p + keylen + 1, opt->value, opt->len);
	}

	if (h == NULL || error != 0)
		goto end;

	/* Assemble the final contiguous buffer. */
	osdlen = jm_h_len(h);
	if (osdlen > jm_maxbufsize_hard) {
		error = EFBIG;
		goto end;
	}
	if (osdlen > 1) {
		osd = malloc(osdlen, M_PRISON, M_WAITOK);
		jm_h_assemble(osd, h);
		osd[osdlen - 1] = '\0'; /* sealed */
	}

	/* Compare and swap the buffers. */
	mtx_lock(&pr->pr_mtx);
	oldosd = osd_jail_get(pr, meta->osd_slot);
	if (oldosd == origosd) {
		error = osd_jail_set(pr, meta->osd_slot, osd);
	} else {
		/*
		 * The osd(9) framework requires protection only for pr_osd,
		 * which is covered by pr_mtx. Therefore, other code might
		 * legally alter jail metadata without allprison_lock. It
		 * means that here we could override data just added by other
		 * thread. This extra caution with retry mechanism aims to
		 * prevent user data loss in such potential cases.
		 */
		error = EAGAIN;
		repeat = true;
	}
	mtx_unlock(&pr->pr_mtx);
	if (error == 0)
		osd = oldosd;

end:
	jm_h_freechain(h);
	free(osd, M_PRISON);
	free(origosd_copy, M_PRISON);

	if (repeat && ++repeats < 3)
		goto again;

	return (error);
}

static int
jm_osd_method_get(void *obj, void *data, const struct meta *meta)
{
	struct prison *pr = obj;
	struct vfsoptlist *opts = data;
	struct vfsopt *opt;
	char *osd = NULL;
	char empty = '\0';
	int error = 0;
	bool locked = false;
	const char *key;
	size_t keylen;
	const char *p;

	sx_assert(&allprison_lock, SA_SLOCKED);

	TAILQ_FOREACH(opt, opts, link) {
		if (strstr(opt->name, meta->name) != opt->name)
			continue;
		if (opt->name[strlen(meta->name)] != '.' &&
		    opt->name[strlen(meta->name)] != '\0')
			continue;

		if (!locked) {
			mtx_lock(&pr->pr_mtx);
			locked = true;
			osd = osd_jail_get(pr, meta->osd_slot);
			if (osd == NULL)
				osd = &empty;
		}

		/* Provide full metadata. */
		if (strcmp(opt->name, meta->name) == 0) {
			if (strlcpy(opt->value, osd, opt->len) >= opt->len) {
				error = EINVAL;
				break;
			}
			opt->seen = 1;
			continue;
		}

		/* Extract a specific key=value. */
		p = osd;
		key = opt->name + strlen(meta->name) + 1;
		keylen = strlen(key);
		while ((p = strstr(p, key)) != NULL) {
			if ((p == osd || *(p - 1) == '\n')
			    && p[keylen] == '=') {
				if (strlcpy(opt->value, p + keylen + 1,
				    MIN(opt->len, strchr(p + keylen + 1, '\n') -
				    (p + keylen + 1) + 1)) >= opt->len) {
					error = EINVAL;
					break;
				}
				opt->seen = 1;
			}
			p += keylen;
		}
		if (error != 0)
			break;
	}

	if (locked)
		mtx_unlock(&pr->pr_mtx);

	return (error);
}

static int
jm_osd_method_check(void *obj __unused, void *data, const struct meta *meta)
{
	struct vfsoptlist *opts = data;
	struct vfsopt *opt;

	TAILQ_FOREACH(opt, opts, link) {
		if (strstr(opt->name, meta->name) != opt->name)
			continue;
		if (opt->name[strlen(meta->name)] != '.' &&
		    opt->name[strlen(meta->name)] != '\0')
			continue;
		opt->seen = 1;
	}

	return (0);
}

static void
jm_osd_destructor(void *osd)
{
	free(osd, M_PRISON);
}


/* OSD for "meta" param */

static struct meta meta;

static inline int
jm_osd_method_set_meta(void *obj, void *data)
{
	return (jm_osd_method_set(obj, data, &meta));
}

static inline int
jm_osd_method_get_meta(void *obj, void *data)
{
	return (jm_osd_method_get(obj, data, &meta));
}

static inline int
jm_osd_method_check_meta(void *obj, void *data)
{
	return (jm_osd_method_check(obj, data, &meta));
}

static struct meta meta = {
	.name = JAIL_META_PRIVATE,
	.osd_slot = 0,
	.methods = {
		[PR_METHOD_SET] =	jm_osd_method_set_meta,
		[PR_METHOD_GET] =	jm_osd_method_get_meta,
		[PR_METHOD_CHECK] =	jm_osd_method_check_meta,
	}
};


/* OSD for "env" param */

static struct meta env;

static inline int
jm_osd_method_set_env(void *obj, void *data)
{
	return (jm_osd_method_set(obj, data, &env));
}

static inline int
jm_osd_method_get_env(void *obj, void *data)
{
	return (jm_osd_method_get(obj, data, &env));
}

static inline int
jm_osd_method_check_env(void *obj, void *data)
{
	return (jm_osd_method_check(obj, data, &env));
}

static struct meta env = {
	.name = JAIL_META_SHARED,
	.osd_slot = 0,
	.methods = {
		[PR_METHOD_SET] =	jm_osd_method_set_env,
		[PR_METHOD_GET] =	jm_osd_method_get_env,
		[PR_METHOD_CHECK] =	jm_osd_method_check_env,
	}
};


/* A jail can read its "env". */

static int
jm_sysctl_env(SYSCTL_HANDLER_ARGS)
{
	struct prison *pr;
	char empty = '\0';
	char *tmpbuf;
	size_t outlen;
	int error = 0;

	pr = req->td->td_ucred->cr_prison;

	mtx_lock(&pr->pr_mtx);
	arg1 = osd_jail_get(pr, env.osd_slot);
	if (arg1 == NULL) {
		tmpbuf = &empty;
		outlen = 1;
	} else {
		outlen = strlen(arg1) + 1;
		if (req->oldptr != NULL) {
			tmpbuf = malloc(outlen, M_PRISON, M_NOWAIT);
			error = (tmpbuf == NULL) ? ENOMEM : 0;
			if (error == 0)
				memcpy(tmpbuf, arg1, outlen);
		}
	}
	mtx_unlock(&pr->pr_mtx);

	if (error != 0)
		return (error);

	if (req->oldptr == NULL)
		SYSCTL_OUT(req, NULL, outlen);
	else {
		SYSCTL_OUT(req, tmpbuf, outlen);
		if (tmpbuf != &empty)
			free(tmpbuf, M_PRISON);
	}

	return (error);
}
SYSCTL_PROC(_security_jail, OID_AUTO, env,
    CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_MPSAFE,
    0, 0, jm_sysctl_env, "A", "Meta information provided by parent jail");


/* Setup and tear down. */

static int
jm_sysinit(void *arg __unused)
{
	meta.osd_slot = osd_jail_register(jm_osd_destructor, meta.methods);
	env.osd_slot = osd_jail_register(jm_osd_destructor, env.methods);

	return (0);
}

static int
jm_sysuninit(void *arg __unused)
{
	osd_jail_deregister(meta.osd_slot);
	osd_jail_deregister(env.osd_slot);

	return (0);
}

SYSINIT(jailmeta, SI_SUB_DRIVERS, SI_ORDER_ANY, jm_sysinit, NULL);
SYSUNINIT(jailmeta, SI_SUB_DRIVERS, SI_ORDER_ANY, jm_sysuninit, NULL);
