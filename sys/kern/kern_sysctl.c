/*-
 * Copyright (c) 1982, 1986, 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Mike Karels at Berkeley Software Design, Inc.
 *
 * Quite extensively rewritten by Poul-Henning Kamp of the FreeBSD
 * project, to make these variables more userfriendly.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)kern_sysctl.c	8.4 (Berkeley) 4/14/94
 * $Id: kern_sysctl.c,v 1.52 1995/11/20 12:42:03 phk Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysproto.h>
#include <sys/kernel.h>
#include <sys/vnode.h>
#include <sys/unistd.h>
#include <sys/conf.h>
#include <sys/sysctl.h>
#include <sys/malloc.h>

/*
 * Locking and stats
 */
static struct sysctl_lock {
	int	sl_lock;
	int	sl_want;
	int	sl_locked;
} memlock;

static int sysctl_root SYSCTL_HANDLER_ARGS;

extern struct linker_set sysctl_;

/*
 * MIB definitions.  XXX Very few of these, if any, belong here.
 */
SYSCTL_NODE(, 0,	  sysctl, CTLFLAG_RW, 0,
	"Sysctl internal magic");
SYSCTL_NODE(, CTL_KERN,	  kern,   CTLFLAG_RW, 0,
	"High kernel, proc, limits &c");
SYSCTL_NODE(, CTL_VM,	  vm,     CTLFLAG_RW, 0,
	"Virtual memory");
SYSCTL_NODE(, CTL_FS,	  fs,     CTLFLAG_RW, 0,
	"File system");
SYSCTL_NODE(, CTL_NET,	  net,    CTLFLAG_RW, 0,
	"Network, (see socket.h)");
SYSCTL_NODE(, CTL_DEBUG,  debug,  CTLFLAG_RW, 0,
	"Debugging");
SYSCTL_NODE(, CTL_HW,	  hw,     CTLFLAG_RW, 0,
	"hardware");
SYSCTL_NODE(, CTL_MACHDEP, machdep, CTLFLAG_RW, 0,
	"machine dependent");
SYSCTL_NODE(, CTL_USER,	  user,   CTLFLAG_RW, 0,
	"user-level");

SYSCTL_STRING(_kern, KERN_OSRELEASE, osrelease, CTLFLAG_RD, osrelease, 0, "");

SYSCTL_INT(_kern, KERN_OSREV, osrevision, CTLFLAG_RD, 0, BSD, "");

SYSCTL_STRING(_kern, KERN_VERSION, version, CTLFLAG_RD, version, 0, "");

SYSCTL_STRING(_kern, KERN_OSTYPE, ostype, CTLFLAG_RD, ostype, 0, "");

extern int osreldate;
SYSCTL_INT(_kern, KERN_OSRELDATE, osreldate, CTLFLAG_RD, &osreldate, 0, "");

SYSCTL_INT(_kern, KERN_MAXPROC, maxproc, CTLFLAG_RD, &maxproc, 0, "");

SYSCTL_INT(_kern, KERN_MAXPROCPERUID, maxprocperuid,
	CTLFLAG_RD, &maxprocperuid, 0, "");

SYSCTL_INT(_kern, KERN_ARGMAX, argmax, CTLFLAG_RD, 0, ARG_MAX, "");

SYSCTL_INT(_kern, KERN_POSIX1, posix1version, CTLFLAG_RD, 0, _POSIX_VERSION, "");

SYSCTL_INT(_kern, KERN_NGROUPS, ngroups, CTLFLAG_RD, 0, NGROUPS_MAX, "");

SYSCTL_INT(_kern, KERN_JOB_CONTROL, job_control, CTLFLAG_RD, 0, 1, "");

#ifdef _POSIX_SAVED_IDS
SYSCTL_INT(_kern, KERN_SAVED_IDS, saved_ids, CTLFLAG_RD, 0, 1, "");
#else
SYSCTL_INT(_kern, KERN_SAVED_IDS, saved_ids, CTLFLAG_RD, 0, 0, "");
#endif

char kernelname[MAXPATHLEN] = "/kernel";	/* XXX bloat */

SYSCTL_STRING(_kern, KERN_BOOTFILE, bootfile,
	CTLFLAG_RW, kernelname, sizeof kernelname, "");

SYSCTL_INT(_hw, HW_NCPU, ncpu, CTLFLAG_RD, 0, 1, "");

SYSCTL_INT(_hw, HW_BYTEORDER, byteorder, CTLFLAG_RD, 0, BYTE_ORDER, "");

SYSCTL_INT(_hw, HW_PAGESIZE, pagesize, CTLFLAG_RD, 0, PAGE_SIZE, "");

char hostname[MAXHOSTNAMELEN];

SYSCTL_STRING(_kern, KERN_HOSTNAME, hostname, CTLFLAG_RW,
	hostname, sizeof(hostname), "");

int securelevel = -1;

static int
sysctl_kern_securelvl SYSCTL_HANDLER_ARGS
{
		int error, level;

		level = securelevel;
		error = sysctl_handle_int(oidp, &level, 0, req);
		if (error || !req->newptr)
			return (error);
		if (level < securelevel && req->p->p_pid != 1)
			return (EPERM);
		securelevel = level;
		return (error);
}

SYSCTL_PROC(_kern, KERN_SECURELVL, securelevel, CTLTYPE_INT|CTLFLAG_RW,
	0, 0, sysctl_kern_securelvl, "I", "");

char domainname[MAXHOSTNAMELEN];
SYSCTL_STRING(_kern, KERN_DOMAINNAME, domainname, CTLFLAG_RW,
	&domainname, sizeof(domainname), "");

long hostid;
/* Some trouble here, if sizeof (int) != sizeof (long) */
SYSCTL_INT(_kern, KERN_HOSTID, hostid, CTLFLAG_RW, &hostid, 0, "");

/* 
 * End of MIB definitions.
 */

/*
 * Initialization of the MIB tree.
 *
 * Order by number in each linker_set.
 */

static int
sysctl_order_cmp(const void *a, const void *b)
{
	const struct sysctl_oid **pa, **pb;

	pa = (const struct sysctl_oid **)a;
	pb = (const struct sysctl_oid **)b;
	if (*pa == NULL)
		return (1);
	if (*pb == NULL)
		return (-1);
	return ((*pa)->oid_number - (*pb)->oid_number);
}

static void
sysctl_order(void *arg)
{
	int j, k;
	struct linker_set *l = (struct linker_set *) arg;
	struct sysctl_oid **oidpp;

	/* First, find the highest oid we have */
	j = l->ls_length;
	oidpp = (struct sysctl_oid **) l->ls_items;
	for (k = 0; j--; oidpp++) 
		if (*oidpp && (*oidpp)->oid_number > k)
			k = (*oidpp)->oid_number;

	/* Next, replace all OID_AUTO oids with new numbers */
	j = l->ls_length;
	oidpp = (struct sysctl_oid **) l->ls_items;
	k += 100;
	for (; j--; oidpp++) 
		if (*oidpp && (*oidpp)->oid_number == OID_AUTO)
			(*oidpp)->oid_number = k++;

	/* Finally: sort by oid */
	j = l->ls_length;
	oidpp = (struct sysctl_oid **) l->ls_items;
	for (; j--; oidpp++) {
		if (!*oidpp)
			continue;
		if ((*oidpp)->oid_arg1 == arg) {
			*oidpp = 0;
			continue;
		}
		if (((*oidpp)->oid_kind & CTLTYPE) == CTLTYPE_NODE)
			if (!(*oidpp)->oid_handler)
				sysctl_order((*oidpp)->oid_arg1);
	}
	qsort(l->ls_items, l->ls_length, sizeof l->ls_items[0],
		sysctl_order_cmp);
}

SYSINIT(sysctl, SI_SUB_KMEM, SI_ORDER_ANY, sysctl_order, &sysctl_);

/*
 * "Staff-functions"
 *
 * {0,0}	printf the entire MIB-tree.
 * {0,1,...}	return the name of the "..." OID.
 * {0,2,...}	return the next OID.
 * {0,3}	return the OID of the name in "new"
 * {0,4,...}	return the format info for the "..." OID.
 */

static void
sysctl_sysctl_debug_dump_node(struct linker_set *l, int i)
{
	int j, k;
	struct sysctl_oid **oidpp;

	j = l->ls_length;
	oidpp = (struct sysctl_oid **) l->ls_items;
	for (; j--; oidpp++) {

		if (!*oidpp)
			continue;

		for (k=0; k<i; k++)
			printf(" ");

		if ((*oidpp)->oid_number > 100) {
			printf("Junk! %p  # %d  %s  k %x  a1 %p  a2 %x  h %p\n",
				*oidpp,
		 		(*oidpp)->oid_number, (*oidpp)->oid_name,
		 		(*oidpp)->oid_kind, (*oidpp)->oid_arg1,
		 		(*oidpp)->oid_arg2, (*oidpp)->oid_handler);
			continue;
		}
		printf("%d %s ", (*oidpp)->oid_number, (*oidpp)->oid_name);

		printf("%c%c",
			(*oidpp)->oid_kind & CTLFLAG_RD ? 'R':' ',
			(*oidpp)->oid_kind & CTLFLAG_WR ? 'W':' ');

		switch ((*oidpp)->oid_kind & CTLTYPE) {
			case CTLTYPE_NODE:
				if ((*oidpp)->oid_handler) {
					printf(" Node(proc)\n");
				} else {
					printf(" Node\n");
					sysctl_sysctl_debug_dump_node(
						(*oidpp)->oid_arg1, i+2);
				}
				break;
			case CTLTYPE_INT:    printf(" Int\n"); break;
			case CTLTYPE_STRING: printf(" String\n"); break;
			case CTLTYPE_QUAD:   printf(" Quad\n"); break;
			case CTLTYPE_OPAQUE: printf(" Opaque/struct\n"); break;
			default:	     printf("\n");
		}

	}
}

static int
sysctl_sysctl_debug SYSCTL_HANDLER_ARGS
{
	sysctl_sysctl_debug_dump_node(&sysctl_, 0);
	return ENOENT;
}

SYSCTL_PROC(_sysctl, 0, debug, CTLTYPE_STRING|CTLFLAG_RD,
	0, 0, sysctl_sysctl_debug, "-", "");

static int
sysctl_sysctl_name SYSCTL_HANDLER_ARGS
{
	int *name = (int *) arg1;
	u_int namelen = arg2;
	int i, j, error = 0;
	struct sysctl_oid **oidpp;
	struct linker_set *lsp = &sysctl_;
	char buf[10];

	while (namelen) {
		if (!lsp) {
			sprintf(buf,"%d",*name);
			if (req->oldidx)
				error = SYSCTL_OUT(req, ".", 1);
			if (!error)
				error = SYSCTL_OUT(req, buf, strlen(buf));
			if (error)
				return (error);
			namelen--;
			name++;
			continue;
		}
		oidpp = (struct sysctl_oid **) lsp->ls_items;
		j = lsp->ls_length;
		lsp = 0;
		for (i = 0; i < j; i++, oidpp++) {
			if (*oidpp && ((*oidpp)->oid_number != *name))
				continue;

			if (req->oldidx)
				error = SYSCTL_OUT(req, ".", 1);
			if (!error)
				error = SYSCTL_OUT(req, (*oidpp)->oid_name,
					strlen((*oidpp)->oid_name));
			if (error)
				return (error);

			namelen--;
			name++;

			if (((*oidpp)->oid_kind & CTLTYPE) != CTLTYPE_NODE)
				break;

			if ((*oidpp)->oid_handler)
				break;

			lsp = (struct linker_set*)(*oidpp)->oid_arg1;
			break;
		}
	}
	return (SYSCTL_OUT(req, "", 1));
}

SYSCTL_NODE(_sysctl, 1, name, CTLFLAG_RD, sysctl_sysctl_name, "");

static int
sysctl_sysctl_next_ls (struct linker_set *lsp, int *name, u_int namelen, 
	int *next, int *len, int level, struct sysctl_oid **oidp)
{
	int i, j;
	struct sysctl_oid **oidpp;

	oidpp = (struct sysctl_oid **) lsp->ls_items;
	j = lsp->ls_length;
	*len = level;
	for (i = 0; i < j; i++, oidpp++) {
		if (!*oidpp)
			continue;

		*next = (*oidpp)->oid_number;
		*oidp = *oidpp;

		if (!namelen) {
			if (((*oidpp)->oid_kind & CTLTYPE) != CTLTYPE_NODE) 
				return 0;
			if ((*oidpp)->oid_handler) 
				/* We really should call the handler here...*/
				return 0;
			lsp = (struct linker_set*)(*oidpp)->oid_arg1;
			return (sysctl_sysctl_next_ls (lsp, 0, 0, next+1, 
				len, level+1, oidp));
		}

		if ((*oidpp)->oid_number < *name)
			continue;

		if ((*oidpp)->oid_number > *name) {
			if (((*oidpp)->oid_kind & CTLTYPE) != CTLTYPE_NODE)
				return 0;
			if ((*oidpp)->oid_handler)
				return 0;
			lsp = (struct linker_set*)(*oidpp)->oid_arg1;
			if (!sysctl_sysctl_next_ls (lsp, name+1, namelen-1, 
				next+1, len, level+1, oidp))
				return (0);
			namelen = 1;
			*len = level;
			continue;
		}
		if (((*oidpp)->oid_kind & CTLTYPE) != CTLTYPE_NODE)
			continue;

		if ((*oidpp)->oid_handler)
			continue;

		lsp = (struct linker_set*)(*oidpp)->oid_arg1;
		if (!sysctl_sysctl_next_ls (lsp, name+1, namelen-1, next+1, 
			len, level+1, oidp))
			return (0);
		namelen = 1;
		*len = level;
	}
	return 1;
}

static int
sysctl_sysctl_next SYSCTL_HANDLER_ARGS
{
	int *name = (int *) arg1;
	u_int namelen = arg2;
	int i, j, error;
	struct sysctl_oid *oid;
	struct linker_set *lsp = &sysctl_;
	int newoid[CTL_MAXNAME];

	i = sysctl_sysctl_next_ls (lsp, name, namelen, newoid, &j, 1, &oid);
	if (i)
		return ENOENT;
	error = SYSCTL_OUT(req, &oid->oid_kind, sizeof oid->oid_kind);
	if (!error)
		error =SYSCTL_OUT(req, newoid, j * sizeof (int));
	return (error);
}

SYSCTL_NODE(_sysctl, 2, next, CTLFLAG_RD, sysctl_sysctl_next, "");

static int
name2oid (char *name, int *oid, int *len, struct sysctl_oid **oidp)
{
	int i, j;
	struct sysctl_oid **oidpp;
	struct linker_set *lsp = &sysctl_;
	char *p;

	if (!*name)
		return ENOENT;

	p = name + strlen(name) - 1 ;
	if (*p == '.')
		*p = '\0';

	*len = 0;

	for (p = name; *p && *p != '.'; p++) 
		;
	i = *p;
	if (i == '.')
		*p = '\0';

	j = lsp->ls_length;
	oidpp = (struct sysctl_oid **) lsp->ls_items;

	while (j-- && *len < CTL_MAXNAME) {
		if (!*oidpp)
			continue;
		if (strcmp(name, (*oidpp)->oid_name)) {
			oidpp++;
			continue;
		}
		*oid++ = (*oidpp)->oid_number;
		(*len)++;

		if (!i) {
			if (oidp)
				*oidp = *oidpp;
			return (0);
		}

		if (((*oidpp)->oid_kind & CTLTYPE) != CTLTYPE_NODE)
			break;

		if ((*oidpp)->oid_handler)
			break;

		lsp = (struct linker_set*)(*oidpp)->oid_arg1;
		j = lsp->ls_length;
		oidpp = (struct sysctl_oid **)lsp->ls_items;
		name = p+1;
		for (p = name; *p && *p != '.'; p++) 
				;
		i = *p;
		if (i == '.')
			*p = '\0';
	}
	return ENOENT;
}

static int
sysctl_sysctl_name2oid SYSCTL_HANDLER_ARGS
{
	char *p;
	int error, oid[CTL_MAXNAME], len;
	struct sysctl_oid *op = 0;

	if (!req->newlen) 
		return ENOENT;

	p = malloc(req->newlen+1, M_SYSCTL, M_WAITOK);

	error = SYSCTL_IN(req, p, req->newlen);
	if (error) {
		free(p, M_SYSCTL);
		return (error);
	}

	p [req->newlen] = '\0';

	error = name2oid(p, oid, &len, &op);

	free(p, M_SYSCTL);

	if (error)
		return (error);

	error = SYSCTL_OUT(req, &op->oid_kind, sizeof op->oid_kind);
	if (!error)
		error = SYSCTL_OUT(req, oid, len * sizeof *oid);
	return (error);
}

SYSCTL_PROC(_sysctl, 3, name2oid, CTLFLAG_RW, 0, 0, 
	sysctl_sysctl_name2oid, "I", "");

static int
sysctl_sysctl_oidfmt SYSCTL_HANDLER_ARGS
{
	int *name = (int *) arg1;
	u_int namelen = arg2;
	int indx, j;
	struct sysctl_oid **oidpp;
	struct linker_set *lsp = &sysctl_;

	j = lsp->ls_length;
	oidpp = (struct sysctl_oid **) lsp->ls_items;

	indx = 0;
	while (j-- && indx < CTL_MAXNAME) {
		if (*oidpp && ((*oidpp)->oid_number == name[indx])) {
			indx++;
			if (((*oidpp)->oid_kind & CTLTYPE) == CTLTYPE_NODE) {
				if ((*oidpp)->oid_handler)
					goto found;
				if (indx == namelen)
					return ENOENT;
				lsp = (struct linker_set*)(*oidpp)->oid_arg1;
				j = lsp->ls_length;
				oidpp = (struct sysctl_oid **)lsp->ls_items;
			} else {
				if (indx != namelen)
					return EISDIR;
				goto found;
			}
		} else {
			oidpp++;
		}
	}
	return ENOENT;
found:
	if (!(*oidpp)->oid_fmt)
		return ENOENT;
	return (SYSCTL_OUT(req, (*oidpp)->oid_fmt, 
		strlen((*oidpp)->oid_fmt)+1));
}


SYSCTL_NODE(_sysctl, 4, oidfmt, CTLFLAG_RD, sysctl_sysctl_oidfmt, "");

/*
 * Default "handler" functions.
 */

/*
 * Handle an integer, signed or unsigned.
 * Two cases:
 *     a variable:  point arg1 at it.
 *     a constant:  pass it in arg2.
 */

int
sysctl_handle_int SYSCTL_HANDLER_ARGS
{
	int error = 0;

	if (arg1)
		error = SYSCTL_OUT(req, arg1, sizeof(int));
	else if (arg2)
		error = SYSCTL_OUT(req, &arg2, sizeof(int));

	if (error || !req->newptr)
		return (error);

	if (!arg1)
		error = EPERM;
	else
		error = SYSCTL_IN(req, arg1, sizeof(int));
	return (error);
}

/*
 * Handle our generic '\0' terminated 'C' string.
 * Two cases:
 * 	a variable string:  point arg1 at it, arg2 is max length.
 * 	a constant string:  point arg1 at it, arg2 is zero.
 */

int
sysctl_handle_string SYSCTL_HANDLER_ARGS
{
	int error=0;

	error = SYSCTL_OUT(req, arg1, strlen((char *)arg1)+1);

	if (error || !req->newptr || !arg2)
		return (error);

	if ((req->newlen - req->newidx) > arg2) {
		error = E2BIG;
	} else {
		arg2 = (req->newlen - req->newidx);
		error = SYSCTL_IN(req, arg1, arg2);
		((char *)arg1)[arg2] = '\0';
	}

	return (error);
}

/*
 * Handle any kind of opaque data.
 * arg1 points to it, arg2 is the size.
 */

int
sysctl_handle_opaque SYSCTL_HANDLER_ARGS
{
	int error;

	error = SYSCTL_OUT(req, arg1, arg2);

	if (error || !req->newptr)
		return (error);

	error = SYSCTL_IN(req, arg1, arg2);

	return (error);
}

/*
 * Transfer functions to/from kernel space.
 * XXX: rather untested at this point
 */
static int
sysctl_old_kernel(struct sysctl_req *req, void *p, int l)
{
	int i = 0;

	if (req->oldptr) {
		i = min(req->oldlen - req->oldidx, l);
		if (i > 0)
			bcopy(p, req->oldptr + req->oldidx, i);
	}
	req->oldidx += l;
	if (i != l)
		return (ENOMEM);
	return (0);

}

static int
sysctl_new_kernel(struct sysctl_req *req, void *p, int l)
{
	if (!req->newptr)
		return 0;
	if (req->newlen - req->newidx < l)
		return (EINVAL);
	bcopy(req->newptr + req->newidx, p, l);
	req->newidx += l;
	return (0);
}

/*
 * Transfer function to/from user space.
 */
static int
sysctl_old_user(struct sysctl_req *req, void *p, int l)
{
	int error = 0, i = 0;

	if (req->lock == 1 && req->oldptr) {
		vslock(req->oldptr, req->oldlen);
		req->lock = 2;
	}
	if (req->oldptr) {
		i = min(req->oldlen - req->oldidx, l);
		if (i > 0)
			error  = copyout(p, req->oldptr + req->oldidx, i);
	}
	req->oldidx += l;
	if (error)
		return (error);
	if (req->oldptr && i < l)
		return (ENOMEM);
	return (0);
}

static int
sysctl_new_user(struct sysctl_req *req, void *p, int l)
{
	int error;

	if (!req->newptr)
		return 0;
	if (req->newlen - req->newidx < l)
		return (EINVAL);
	error = copyin(req->newptr + req->newidx, p, l);
	req->newidx += l;
	return (error);
}

/*
 * Traverse our tree, and find the right node, execute whatever it points
 * at, and return the resulting error code.
 */

int
sysctl_root SYSCTL_HANDLER_ARGS
{
	int *name = (int *) arg1;
	u_int namelen = arg2;
	int indx, i, j;
	struct sysctl_oid **oidpp;
	struct linker_set *lsp = &sysctl_;

	j = lsp->ls_length;
	oidpp = (struct sysctl_oid **) lsp->ls_items;

	indx = 0;
	while (j-- && indx < CTL_MAXNAME) {
		if (*oidpp && ((*oidpp)->oid_number == name[indx])) {
			indx++;
			if ((*oidpp)->oid_kind & CTLFLAG_NOLOCK)
				req->lock = 0;
			if (((*oidpp)->oid_kind & CTLTYPE) == CTLTYPE_NODE) {
				if ((*oidpp)->oid_handler)
					goto found;
				if (indx == namelen)
					return ENOENT;
				lsp = (struct linker_set*)(*oidpp)->oid_arg1;
				j = lsp->ls_length;
				oidpp = (struct sysctl_oid **)lsp->ls_items;
			} else {
				if (indx != namelen)
					return EISDIR;
				goto found;
			}
		} else {
			oidpp++;
		}
	}
	return ENOENT;
found:

	/* If writing isn't allowed */
	if (req->newptr && !((*oidpp)->oid_kind & CTLFLAG_WR))
		return (EPERM);

	if (!(*oidpp)->oid_handler)
		return EINVAL;

	if (((*oidpp)->oid_kind & CTLTYPE) == CTLTYPE_NODE) {
		i = ((*oidpp)->oid_handler) (*oidpp,
					name + indx, namelen - indx,
					req);
	} else {
		i = ((*oidpp)->oid_handler) (*oidpp,
					(*oidpp)->oid_arg1, (*oidpp)->oid_arg2,
					req);
	}
	return (i);
}

#ifndef _SYS_SYSPROTO_H_
struct sysctl_args {
	int	*name;
	u_int	namelen;
	void	*old;
	size_t	*oldlenp;
	void	*new;
	size_t	newlen;
};
#endif

int
__sysctl(struct proc *p, struct sysctl_args *uap, int *retval)
{
	int error, i, j, name[CTL_MAXNAME];

	if (uap->namelen > CTL_MAXNAME || uap->namelen < 2)
		return (EINVAL);

 	error = copyin(uap->name, &name, uap->namelen * sizeof(int));
 	if (error)
		return (error);

	error = userland_sysctl(p, name, uap->namelen,
		uap->old, uap->oldlenp, 0,
		uap->new, uap->newlen, &j);
	if (error && error != ENOMEM)
		return (error);
	if (uap->oldlenp) {
		i = copyout(&j, uap->oldlenp, sizeof(j));
		if (i)
			return (i);
	}
	return (error);
}

/*
 * This is used from various compatibility syscalls too.  That's why name
 * must be in kernel space.
 */
int
userland_sysctl(struct proc *p, int *name, u_int namelen, void *old, size_t *oldlenp, int inkernel, void *new, size_t newlen, int *retval)
{
	int error = 0;
	struct sysctl_req req;

	bzero(&req, sizeof req);

	req.p = p;

	if (new != NULL && (error = suser(p->p_ucred, &p->p_acflag)))
		return (error);

	if (oldlenp) {
		if (inkernel) {
			req.oldlen = *oldlenp;
		} else {
			error = copyin(oldlenp, &req.oldlen, sizeof(*oldlenp));
			if (error)
				return (error);
		}
	}

	if (old) {
		if (!useracc(old, req.oldlen, B_WRITE))
			return (EFAULT);
		req.oldptr= old;
	}

	if (newlen) {
		if (!useracc(new, req.newlen, B_READ))
			return (EFAULT);
		req.newlen = newlen;
		req.newptr = new;
	}

	req.oldfunc = sysctl_old_user;
	req.newfunc = sysctl_new_user;
	req.lock = 1;

	/* XXX this should probably be done in a general way */
	while (memlock.sl_lock) {
		memlock.sl_want = 1;
		(void) tsleep((caddr_t)&memlock, PRIBIO+1, "sysctl", 0);
		memlock.sl_locked++;
	}
	memlock.sl_lock = 1;

	error = sysctl_root(0, name, namelen, &req);

	if (req.lock == 2)
		vsunlock(req.oldptr, req.oldlen, B_WRITE);

	memlock.sl_lock = 0;

	if (memlock.sl_want) {
		memlock.sl_want = 0;
		wakeup((caddr_t)&memlock);
	}

	if (error && error != ENOMEM)
		return (error);

	if (retval) {
		if (req.oldptr && req.oldidx > req.oldlen)
			*retval = req.oldlen;
		else
			*retval = req.oldidx;
	}
	return (error);
}

#ifdef COMPAT_43
#include <sys/socket.h>
#define	KINFO_PROC		(0<<8)
#define	KINFO_RT		(1<<8)
#define	KINFO_VNODE		(2<<8)
#define	KINFO_FILE		(3<<8)
#define	KINFO_METER		(4<<8)
#define	KINFO_LOADAVG		(5<<8)
#define	KINFO_CLOCKRATE		(6<<8)

/* Non-standard BSDI extension - only present on their 4.3 net-2 releases */
#define	KINFO_BSDI_SYSINFO	(101<<8)

/*
 * XXX this is bloat, but I hope it's better here than on the potentially
 * limited kernel stack...  -Peter
 */

struct {
	int	bsdi_machine;		/* "i386" on BSD/386 */
/*      ^^^ this is an offset to the string, relative to the struct start */
	char	*pad0;
	long	pad1;
	long	pad2;
	long	pad3;
	u_long	pad4;
	u_long	pad5;
	u_long	pad6;

	int	bsdi_ostype;		/* "BSD/386" on BSD/386 */
	int	bsdi_osrelease;		/* "1.1" on BSD/386 */
	long	pad7;
	long	pad8;
	char	*pad9;

	long	pad10;
	long	pad11;
	int	pad12;
	long	pad13;
	quad_t	pad14;
	long	pad15;

	struct	timeval pad16;
	/* we dont set this, because BSDI's uname used gethostname() instead */
	int	bsdi_hostname;		/* hostname on BSD/386 */

	/* the actual string data is appended here */

} bsdi_si;
/*
 * this data is appended to the end of the bsdi_si structure during copyout.
 * The "char *" offsets are relative to the base of the bsdi_si struct.
 * This contains "FreeBSD\02.0-BUILT-nnnnnn\0i386\0", and these strings
 * should not exceed the length of the buffer here... (or else!! :-)
 */
char bsdi_strings[80];	/* It had better be less than this! */

#ifndef _SYS_SYSPROTO_H_
struct getkerninfo_args {
	int	op;
	char	*where;
	int	*size;
	int	arg;
};
#endif

int
ogetkerninfo(struct proc *p, struct getkerninfo_args *uap, int *retval)
{
	int error, name[6];
	u_int size;

	switch (uap->op & 0xff00) {

	case KINFO_RT:
		name[0] = CTL_NET;
		name[1] = PF_ROUTE;
		name[2] = 0;
		name[3] = (uap->op & 0xff0000) >> 16;
		name[4] = uap->op & 0xff;
		name[5] = uap->arg;
		error = userland_sysctl(p, name, 6, uap->where, uap->size,
			0, 0, 0, &size);
		break;

	case KINFO_VNODE:
		name[0] = CTL_KERN;
		name[1] = KERN_VNODE;
		error = userland_sysctl(p, name, 2, uap->where, uap->size,
			0, 0, 0, &size);
		break;

	case KINFO_PROC:
		name[0] = CTL_KERN;
		name[1] = KERN_PROC;
		name[2] = uap->op & 0xff;
		name[3] = uap->arg;
		error = userland_sysctl(p, name, 4, uap->where, uap->size,
			0, 0, 0, &size);
		break;

	case KINFO_FILE:
		name[0] = CTL_KERN;
		name[1] = KERN_FILE;
		error = userland_sysctl(p, name, 2, uap->where, uap->size,
			0, 0, 0, &size);
		break;

	case KINFO_METER:
		name[0] = CTL_VM;
		name[1] = VM_METER;
		error = userland_sysctl(p, name, 2, uap->where, uap->size,
			0, 0, 0, &size);
		break;

	case KINFO_LOADAVG:
		name[0] = CTL_VM;
		name[1] = VM_LOADAVG;
		error = userland_sysctl(p, name, 2, uap->where, uap->size,
			0, 0, 0, &size);
		break;

	case KINFO_CLOCKRATE:
		name[0] = CTL_KERN;
		name[1] = KERN_CLOCKRATE;
		error = userland_sysctl(p, name, 2, uap->where, uap->size,
			0, 0, 0, &size);
		break;

	case KINFO_BSDI_SYSINFO: {
		/*
		 * this is pretty crude, but it's just enough for uname()
		 * from BSDI's 1.x libc to work.
		 *
		 * In particular, it doesn't return the same results when
		 * the supplied buffer is too small.  BSDI's version apparently
		 * will return the amount copied, and set the *size to how
		 * much was needed.  The emulation framework here isn't capable
		 * of that, so we just set both to the amount copied.
		 * BSDI's 2.x product apparently fails with ENOMEM in this
		 * scenario.
		 */

		u_int needed;
		u_int left;
		char *s;

		bzero((char *)&bsdi_si, sizeof(bsdi_si));
		bzero(bsdi_strings, sizeof(bsdi_strings));

		s = bsdi_strings;

		bsdi_si.bsdi_ostype = (s - bsdi_strings) + sizeof(bsdi_si);
		strcpy(s, ostype);
		s += strlen(s) + 1;

		bsdi_si.bsdi_osrelease = (s - bsdi_strings) + sizeof(bsdi_si);
		strcpy(s, osrelease);
		s += strlen(s) + 1;

		bsdi_si.bsdi_machine = (s - bsdi_strings) + sizeof(bsdi_si);
		strcpy(s, machine);
		s += strlen(s) + 1;

		needed = sizeof(bsdi_si) + (s - bsdi_strings);

		if (uap->where == NULL) {
			/* process is asking how much buffer to supply.. */
			size = needed;
			error = 0;
			break;
		}


		/* if too much buffer supplied, trim it down */
		if (size > needed)
			size = needed;

		/* how much of the buffer is remaining */
		left = size;

		if ((error = copyout((char *)&bsdi_si, uap->where, left)) != 0)
			break;

		/* is there any point in continuing? */
		if (left > sizeof(bsdi_si)) {
			left -= sizeof(bsdi_si);
			error = copyout(&bsdi_strings,
					uap->where + sizeof(bsdi_si), left);
		}
		break;
	}

	default:
		return (EOPNOTSUPP);
	}
	if (error)
		return (error);
	*retval = size;
	if (uap->size)
		error = copyout((caddr_t)&size, (caddr_t)uap->size,
		    sizeof(size));
	return (error);
}
#endif /* COMPAT_43 */
