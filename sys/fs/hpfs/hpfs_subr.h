/*-
 * Copyright (c) 1998, 1999 Semen Ustimenko (semenu@FreeBSD.org)
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

#define	hpfs_bmmarkfree(hpmp, bn,bl) hpfs_bmmark(hpmp, bn, bl, 1)
#define	hpfs_bmmarkbusy(hpmp, bn,bl) hpfs_bmmark(hpmp, bn, bl, 0)

u_long		hpfs_checksum (u_int8_t *, int);

int		hpfs_bminit (struct hpfsmount *);
void		hpfs_bmdeinit (struct hpfsmount *);
int		hpfs_bmfblookup (struct hpfsmount *, lsn_t *);
int		hpfs_bmmark (struct hpfsmount *, lsn_t, u_long, int);
int		hpfs_bmlookup (struct hpfsmount *, u_long, lsn_t, u_long,
				lsn_t *, u_long *);

struct hpfs_args;
int		hpfs_cpinit (struct mount *, struct hpfsmount *);
int		hpfs_cpdeinit (struct hpfsmount *);
int		hpfs_cpload (struct hpfsmount *, struct cpiblk *,
			     struct cpdblk *);
int		hpfs_cpstrnnicmp (struct hpfsmount *, char *, int, u_int16_t,
				  char *, int, u_int16_t);
int		hpfs_cmpfname (struct hpfsmount *, char *, int,
			       char *, int, u_int16_t);

/* XXX Need unsigned conversion? */
#define	hpfs_u2d(hpmp, c)	((((u_char)(c))&0x80)?(hpmp->hpm_u2d[((u_char)(c))&0x7F]):((u_char)(c)))
#define hpfs_d2u(hpmp, c)	((((u_char)(c))&0x80)?(hpmp->hpm_d2u[((u_char)(c))&0x7F]):((u_char)(c)))
#define	hpfs_toupper(hpmp, c, cp)	((((u_char)(c))&0x80) ? ((u_char)((hpmp)->hpm_cpdblk[(cp)].b_upcase[((u_char)(c))&0x7F])) : ((((u_char)(c)) >= 'a' && ((u_char)(c)) <='z')?(((u_char)(c))-'a'+'A'):((u_char)(c))))


int		hpfs_truncate (struct hpfsnode *, u_long);
int		hpfs_extend (struct hpfsnode *, u_long);

int		hpfs_updateparent (struct hpfsnode *);
int		hpfs_update (struct hpfsnode *);

int		hpfs_validateparent (struct hpfsnode *);
struct timespec	hpfstimetounix (u_long);
int		hpfs_genlookupbyname (struct hpfsnode *, char *, int,
				      struct buf **, struct hpfsdirent **);

int		hpfs_makefnode (struct vnode *, struct vnode **,
				struct componentname *, struct vattr *);
int		hpfs_removefnode (struct vnode *, struct vnode *,
				struct componentname *);

int		hpfs_breadstruct (struct hpfsmount *, lsn_t, u_int, u_int32_t,
				 struct buf **);
#define	hpfs_breadalsec(hpmp, lsn, bpp) \
	hpfs_breadstruct(hpmp, lsn, DEV_BSIZE, AS_MAGIC, bpp)
#define	hpfs_breaddirblk(hpmp, lsn, bpp) \
	hpfs_breadstruct(hpmp, lsn, D_BSIZE, D_MAGIC, bpp)

#if 0
#define	hpfs_hplock(hp, p)						\
	lockmgr(&(hp)->h_intlock, LK_EXCLUSIVE, (p), curthread)
#define	hpfs_hpunlock(hp, p)						\
	lockmgr(&(hp)->h_intlock, LK_RELEASE, (p), curthread)
#endif

int		hpfs_hpbmap (struct hpfsnode *, daddr_t, daddr_t *, int *);
int		hpfs_truncatealblk (struct hpfsmount *, alblk_t *, lsn_t,int *);
int		hpfs_addextent (struct hpfsmount *, struct hpfsnode *, u_long);
