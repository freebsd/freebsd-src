/*	$Id: fat.h,v 1.3 1995/05/30 08:07:34 rgrimes Exp $ */
/*	$NetBSD: fat.h,v 1.4 1994/08/21 18:43:57 ws Exp $	*/

/*-
 * Copyright (C) 1994 Wolfgang Solfrank.
 * Copyright (C) 1994 TooLs GmbH.
 * All rights reserved.
 * Original code by Paul Popelka (paulp@uts.amdahl.com) (see below).
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
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * Written by Paul Popelka (paulp@uts.amdahl.com)
 *
 * You can do anything you want with this software, just don't say you wrote
 * it, and don't remove this notice.
 *
 * This software is provided "as is".
 *
 * The author supplies this software to be publicly redistributed on the
 * understanding that the author is not responsible for the correct
 * functioning of this software in any circumstances and is not liable for
 * any damages caused by this software.
 *
 * October 1992
 */

/*
 * Some useful cluster numbers.
 */
#define	MSDOSFSROOT	0	/* cluster 0 means the root dir */
#define	CLUST_FREE	0	/* cluster 0 also means a free cluster */
#define	MSDOSFSFREE	CLUST_FREE
#define	CLUST_FIRST	2	/* first legal cluster number */
#define	CLUST_RSRVS	0xfff0	/* start of reserved cluster range */
#define	CLUST_RSRVE	0xfff6	/* end of reserved cluster range */
#define	CLUST_BAD	0xfff7	/* a cluster with a defect */
#define	CLUST_EOFS	0xfff8	/* start of eof cluster range */
#define	CLUST_EOFE	0xffff	/* end of eof cluster range */

#define	FAT12_MASK	0x0fff	/* mask for 12 bit cluster numbers */
#define	FAT16_MASK	0xffff	/* mask for 16 bit cluster numbers */

/*
 * Return true if filesystem uses 12 bit fats. Microsoft Programmer's
 * Reference says if the maximum cluster number in a filesystem is greater
 * than 4086 then we've got a 16 bit fat filesystem.
 */
#define	FAT12(pmp)	(pmp->pm_maxcluster <= 4086)
#define	FAT16(pmp)	(pmp->pm_maxcluster >  4086)

#define	MSDOSFSEOF(cn)	(((cn) & 0xfff8) == 0xfff8)

#ifdef KERNEL
/*
 * These are the values for the function argument to the function
 * fatentry().
 */
#define	FAT_GET		0x0001	/* get a fat entry */
#define	FAT_SET		0x0002	/* set a fat entry */
#define	FAT_GET_AND_SET	(FAT_GET | FAT_SET)

/*
 * Flags to extendfile:
 */
#define	DE_CLEAR	1	/* Zero out the blocks allocated */

int pcbmap __P((struct denode *dep, u_long findcn, daddr_t *bnp, u_long *cnp));
int clusterfree __P((struct msdosfsmount *pmp, u_long cn, u_long *oldcnp));
int clusteralloc __P((struct msdosfsmount *pmp, u_long start, u_long count, u_long fillwith, u_long *retcluster, u_long *got));
int fatentry __P((int function, struct msdosfsmount *pmp, u_long cluster, u_long *oldcontents, u_long newcontents));
int freeclusterchain __P((struct msdosfsmount *pmp, u_long startchain));
int extendfile __P((struct denode *dep, u_long count, struct buf **bpp, u_long *ncp, int flags));
void fc_purge __P((struct denode *dep, u_int frcn));

int readep __P((struct msdosfsmount *pmp, u_long dirclu, u_long dirofs,  struct buf **bpp, struct direntry **epp));
int readde __P((struct denode *dep, struct buf **bpp, struct direntry **epp));
int deextend __P((struct denode *dep, off_t length, struct ucred *cred));
int fillinusemap __P((struct msdosfsmount *pmp));
int reinsert __P((struct denode *dep));
int dosdirempty __P((struct denode *dep));
int createde __P((struct denode *dep, struct denode *ddep, struct denode **depp));
int deupdat __P((struct denode *dep, struct timespec *tp, int waitfor));
int removede __P((struct denode *pdep, struct denode *dep));
int detrunc __P((struct denode *dep, u_long length, int flags, struct ucred *cred, struct proc *p));
int doscheckpath __P(( struct denode *source, struct denode *target));
#endif	/* KERNEL */
