/*
 *  Written by Paul Popelka (paulp@uts.amdahl.com)
 *
 *  You can do anything you want with this software,
 *    just don't say you wrote it,
 *    and don't remove this notice.
 *
 *  This software is provided "as is".
 *
 *  The author supplies this software to be publicly
 *  redistributed on the understanding that the author
 *  is not responsible for the correct functioning of
 *  this software in any circumstances and is not liable
 *  for any damages caused by this software.
 *
 *  October 1992
 *
 *	$Id: fat.h,v 1.3 1993/11/07 17:51:15 wollman Exp $
 */

#ifndef _PCFS_FAT_H_
#define _PCFS_FAT_H_ 1

/*
 *  Some useful cluster numbers.
 */
#define	PCFSROOT	0	/* cluster 0 means the root dir		*/
#define	CLUST_FREE	0	/* cluster 0 also means a free cluster	*/
#define	PCFSFREE	CLUST_FREE
#define	CLUST_FIRST	2	/* first legal cluster number		*/
#define	CLUST_RSRVS	0xfff0	/* start of reserved cluster range	*/
#define	CLUST_RSRVE	0xfff6	/* end of reserved cluster range	*/
#define	CLUST_BAD	0xfff7	/* a cluster with a defect		*/
#define	CLUST_EOFS	0xfff8	/* start of eof cluster range		*/
#define	CLUST_EOFE	0xffff	/* end of eof cluster range		*/

#define	FAT12_MASK	0x0fff	/* mask for 12 bit cluster numbers	*/
#define	FAT16_MASK	0xffff	/* mask for 16 bit cluster numbers	*/

/*
 *  Return true if filesystem uses 12 bit fats.
 *  Microsoft Programmer's Reference says if the
 *  maximum cluster number in a filesystem is greater
 *  than 4086 then we've got a 16 bit fat filesystem.
 */
#define	FAT12(pmp)	(pmp->pm_maxcluster <= 4086)
#define	FAT16(pmp)	(pmp->pm_maxcluster >  4086)

#define	PCFSEOF(cn)	(((cn) & 0xfff8) == 0xfff8)

/*
 *  These are the values for the function argument to
 *  the function fatentry().
 */
#define	FAT_GET		0x0001		/* get a fat entry		*/
#define	FAT_SET		0x0002		/* set a fat entry		*/
#define	FAT_GET_AND_SET	(FAT_GET | FAT_SET)

#if defined(KERNEL)
int pcbmap __P((struct denode *dep,
		u_long findcn,
		daddr_t *bnp,
		u_long *cnp));
int clusterfree __P((struct pcfsmount *pmp, u_long cn, u_long *oldcnp));
int clusteralloc __P((struct pcfsmount *pmp, u_long *retcluster,
	u_long fillwith));
int fatentry __P((int function, struct pcfsmount *pmp,
	u_long cluster, u_long *oldcontents, u_long newcontents));
int freeclusterchain __P((struct pcfsmount *pmp, u_long startchain));
#endif /* defined(KERNEL) */
#endif /* _PCFS_FAT_H_ */
