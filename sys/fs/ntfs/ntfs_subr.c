/*-
 * Copyright (c) 1998, 1999 Semen Ustimenko
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
 *	$Id: ntfs_subr.c,v 1.9 1999/02/02 01:54:54 semen Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/namei.h>
#include <sys/proc.h>
#include <sys/kernel.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/buf.h>
#include <sys/file.h>
#include <sys/malloc.h>
#include <machine/clock.h>

#include <miscfs/specfs/specdev.h>

/* #define NTFS_DEBUG 1 */
#include <ntfs/ntfs.h>
#include <ntfs/ntfsmount.h>
#include <ntfs/ntfs_inode.h>
#include <ntfs/ntfs_subr.h>
#include <ntfs/ntfs_compr.h>

#if __FreeBSD_version >= 300000
MALLOC_DEFINE(M_NTFSNTVATTR, "NTFS vattr", "NTFS file attribute information");
MALLOC_DEFINE(M_NTFSRDATA, "NTFS res data", "NTFS resident data");
MALLOC_DEFINE(M_NTFSRUN, "NTFS vrun", "NTFS vrun storage");
MALLOC_DEFINE(M_NTFSDECOMP, "NTFS decomp", "NTFS decompression temporary");
#endif

int
ntfs_ntvattrrele(
		 struct ntvattr * vap)
{
	dprintf(("ntfs_ntvattrrele: ino: %d, type: 0x%x\n",
		 vap->va_ip->i_number, vap->va_type));

	vrele(NTTOV(vap->va_ip));

	return (0);
}

int
ntfs_ntvattrget(
		struct ntfsmount * ntmp,
		struct ntnode * ip,
		u_int32_t type,
		char *name,
		cn_t vcn,
		struct ntvattr ** vapp)
{
	int             error;
	struct ntvattr *vap;
	struct ntvattr *lvap = NULL;
	struct attr_attrlist *aalp;
	struct attr_attrlist *nextaalp;
	caddr_t         alpool;
	int             len, namelen;

	*vapp = NULL;

	if (name) {
		dprintf(("ntfs_ntvattrget: " \
			 "ino: %d, type: 0x%x, name: %s, vcn: %d\n", \
			 ip->i_number, type, name, (u_int32_t) vcn));
		namelen = strlen(name);
	} else {
		dprintf(("ntfs_ntvattrget: " \
			 "ino: %d, type: 0x%x, vcn: %d\n", \
			 ip->i_number, type, (u_int32_t) vcn));
		name = "";
		namelen = 0;
	}

	if((ip->i_flag & IN_LOADED) == 0) {
		dprintf(("ntfs_ntvattrget: node not loaded, ino: %d\n",
		       ip->i_number));
		error = ntfs_loadnode(ntmp,ip);
		if(error) {
			printf("ntfs_ntvattrget: FAILED TO LOAD INO: %d\n",
			       ip->i_number);
			return (error);
		}
	}

	for (vap = ip->i_vattrp; vap; vap = vap->va_nextp) {
		ddprintf(("type: 0x%x, vcn: %d - %d\n", \
			  vap->va_type, (u_int32_t) vap->va_vcnstart, \
			  (u_int32_t) vap->va_vcnend));
		if ((vap->va_type == type) &&
		    (vap->va_vcnstart <= vcn) && (vap->va_vcnend >= vcn) &&
		    (vap->va_namelen == namelen) &&
		    (!strncmp(name, vap->va_name, namelen))) {
			*vapp = vap;
#if __FreeBSD_version >= 300000
			VREF(NTTOV(vap->va_ip));
#else
			/*
			 * In RELENG_2_2 vref can call vfs_object_create(...)
			 * who calls vgetattr, who calls ntfs_getattr, who
			 * calls ntfs_ntvattrget, who calls vref... :-( This
			 * hack is to avoid it.   XXX
			 */
			NTTOV(vap->va_ip)->v_usecount++;
#endif
			return (0);
		}
		if (vap->va_type == NTFS_A_ATTRLIST)
			lvap = vap;
	}

	if (!lvap) {
		dprintf(("ntfs_ntvattrget: UNEXISTED ATTRIBUTE: " \
		       "ino: %d, type: 0x%x, name: %s, vcn: %d\n", \
		       ip->i_number, type, name, (u_int32_t) vcn));
		return (ENOENT);
	}
	/* Scan $ATTRIBUTE_LIST for requested attribute */
	len = lvap->va_datalen;
	MALLOC(alpool, caddr_t, len, M_TEMP, M_WAITOK);
	error = ntfs_breadntvattr_plain(ntmp, ip, lvap, 0, len, alpool, &len);
	if (error)
		goto out;

	aalp = (struct attr_attrlist *) alpool;
	nextaalp = NULL;

	while (len > 0) {
		dprintf(("ntfs_ntvattrget: " \
			 "attrlist: ino: %d, attr: 0x%x, vcn: %d\n", \
			 aalp->al_inumber, aalp->al_type, \
			 (u_int32_t) aalp->al_vcnstart));

		if (len > aalp->reclen) {
			nextaalp = NTFS_NEXTREC(aalp, struct attr_attrlist *);
		} else {
			nextaalp = NULL;
		}
		len -= aalp->reclen;

#define AALPCMP(aalp,type,name,namelen) (				\
  (aalp->al_type == type) && (aalp->al_namelen == namelen) &&		\
  !uastrcmp(aalp->al_name,aalp->al_namelen,name,namelen) )

		if (AALPCMP(aalp, type, name, namelen) &&
		    (!nextaalp || (nextaalp->al_vcnstart > vcn) ||
		     !AALPCMP(nextaalp, type, name, namelen))) {
			struct vnode   *newvp;
			struct ntnode  *newip;

			dprintf(("ntfs_ntvattrget: attrbute in ino: %d\n",
				 aalp->al_inumber));

			error = VFS_VGET(ntmp->ntm_mountp, aalp->al_inumber,
					 &newvp);
			if (error) {
				printf("ntfs_ntvattrget: CAN'T VGET INO: %d\n",
				       aalp->al_inumber);
				goto out;
			}
			newip = VTONT(newvp);
			if(~newip->i_flag & IN_LOADED) {
				dprintf(("ntfs_ntvattrget: node not loaded," \
					 " ino: %d\n", newip->i_number));
				error = ntfs_loadnode(ntmp,ip);
				if(error) {
					printf("ntfs_ntvattrget: CAN'T LOAD " \
					       "INO: %d\n", newip->i_number);
					vput(newvp);
					goto out;
				}
			}
			for (vap = newip->i_vattrp; vap; vap = vap->va_nextp) {
				if ((vap->va_type == type) &&
				    (vap->va_vcnstart <= vcn) &&
				    (vap->va_vcnend >= vcn) &&
				    (vap->va_namelen == namelen) &&
				  (!strncmp(name, vap->va_name, namelen))) {
					*vapp = vap;
#if __FreeBSD_version >= 300000
					VREF(NTTOV(vap->va_ip));
#else
					/* See comment above */
					NTTOV(vap->va_ip)->v_usecount++;
#endif
					vput(newvp);
					error = 0;
					goto out;
				}
				if (vap->va_type == NTFS_A_ATTRLIST)
					lvap = vap;
			}
			printf("ntfs_ntvattrget: ATTRLIST ERROR.\n");
			vput(newvp);
			break;
		}
#undef AALPCMP
		aalp = nextaalp;
	}
	error = ENOENT;

	dprintf(("ntfs_ntvattrget: UNEXISTED ATTRIBUTE: " \
	       "ino: %d, type: 0x%x, name: %s, vcn: %d\n", \
	       ip->i_number, type, name, (u_int32_t) vcn));
out:
	FREE(alpool, M_TEMP);

	return (error);
}

int
ntfs_loadnode(
	      struct ntfsmount * ntmp,
	      struct ntnode * ip)
{
	struct filerec  *mfrp;
	daddr_t         bn;
	int		error,off;
	struct attr    *ap;
	struct ntvattr**vapp;

	dprintf(("ntfs_loadnode: loading ino: %d\n",ip->i_number));

	MALLOC(mfrp, struct filerec *, ntfs_bntob(ntmp->ntm_bpmftrec),
	       M_TEMP, M_WAITOK);

	if (ip->i_number < NTFS_SYSNODESNUM) {
		struct buf     *bp;

		dprintf(("ntfs_loadnode: read system node\n"));

		bn = ntfs_cntobn(ntmp->ntm_mftcn) +
			ntmp->ntm_bpmftrec * ip->i_number;

		error = bread(ntmp->ntm_devvp,
			      bn, ntfs_bntob(ntmp->ntm_bpmftrec),
			      NOCRED, &bp);
		if (error) {
			printf("ntfs_loadnode: BREAD FAILED\n");
			brelse(bp);
			goto out;
		}
		memcpy(mfrp, bp->b_data, ntfs_bntob(ntmp->ntm_bpmftrec));
		bqrelse(bp);
	} else {
		struct vnode   *vp;

		vp = ntmp->ntm_sysvn[NTFS_MFTINO];
		error = ntfs_breadattr(ntmp, VTONT(vp), NTFS_A_DATA, NULL,
			       ip->i_number * ntfs_bntob(ntmp->ntm_bpmftrec),
			       ntfs_bntob(ntmp->ntm_bpmftrec), mfrp);
		if (error) {
			printf("ntfs_loadnode: ntfs_breadattr failed\n");
			goto out;
		}
	}
	/* Check if magic and fixups are correct */
	error = ntfs_procfixups(ntmp, NTFS_FILEMAGIC, (caddr_t)mfrp,
				ntfs_bntob(ntmp->ntm_bpmftrec));
	if (error) {
		printf("ntfs_loadnode: BAD MFT RECORD %d\n",
		       (u_int32_t) ip->i_number);
		goto out;
	}

	dprintf(("ntfs_loadnode: load attrs for ino: %d\n",ip->i_number));
	off = mfrp->fr_attroff;
	ap = (struct attr *) ((caddr_t)mfrp + off);
	vapp = &ip->i_vattrp;
	while (ap->a_hdr.a_type != -1) {
		error = ntfs_attrtontvattr(ntmp, vapp, ap);
		if (error)
			break;
		(*vapp)->va_ip = ip;
		vapp = &((*vapp)->va_nextp);

		off += ap->a_hdr.reclen;
		ap = (struct attr *) ((caddr_t)mfrp + off);
	}
	if (error) {
		printf("ntfs_loadnode: failed to load attr ino: %d\n",
		       ip->i_number);
		goto out;
	}

	ip->i_mainrec = mfrp->fr_mainrec;
	ip->i_nlink = mfrp->fr_nlink;
	ip->i_frflag = mfrp->fr_flags;

	ip->i_flag |= IN_LOADED;

	if (ip->i_mainrec == 0) {
		struct ntvattr *vap;

		if (ntfs_ntvattrget(ntmp, ip, NTFS_A_NAME, NULL, 0, &vap) == 0){
			ip->i_times = vap->va_a_name->n_times;
			ip->i_pnumber = vap->va_a_name->n_pnumber;
			ip->i_fflag = vap->va_a_name->n_flag;

			ntfs_ntvattrrele(vap);
		}

		if ((ip->i_fflag & NTFS_FFLAG_DIR) && (ip->i_defattr == 0)) {
			struct ntvattr *irvap;

			ip->i_type = VDIR;	
			ip->i_defattr = NTFS_A_INDXROOT;
			ip->i_defattrname = "$I30";
			error = ntfs_ntvattrget(ntmp, ip,
						NTFS_A_INDXROOT, "$I30",
						0, &irvap);
			if(error == 0) {
				ip->i_dirblsz = irvap->va_a_iroot->ir_size;
				MALLOC(ip->i_dirblbuf, caddr_t,
				       max(irvap->va_datalen,ip->i_dirblsz),
				       M_NTFSDIR, M_WAITOK);

				ntfs_ntvattrrele(irvap);
			} 
			ip->i_size = 0;
			ip->i_allocated = 0;
			error = 0;
		} else {
			ip->i_type = VREG;	
			if(ip->i_defattr == 0) {
				ip->i_defattr = NTFS_A_DATA;
				ip->i_defattrname = NULL;
			}

			ntfs_filesize(ntmp, ip, &ip->i_size, &ip->i_allocated);
		}
	}

	if (NTTOV(ip)) {
		if (ip->i_number == NTFS_ROOTINO)
			NTTOV(ip)->v_flag |= VROOT;
		if (ip->i_number < NTFS_SYSNODESNUM)
			NTTOV(ip)->v_flag |= VSYSTEM;
		NTTOV(ip)->v_type = ip->i_type;	
	}
out:
	FREE(mfrp, M_TEMP);
	return (error);
}
		

int
ntfs_ntget(
	   struct ntfsmount * ntmp,
	   ino_t ino,
	   struct ntnode ** ipp)
{
	struct ntnode  *ip;

	dprintf(("ntfs_ntget: allocate ntnode %d\n", ino));
	*ipp = NULL;

	MALLOC(ip, struct ntnode *, sizeof(struct ntnode),
	       M_NTFSNODE, M_WAITOK);
	bzero((caddr_t) ip, sizeof(struct ntnode));

	/* Generic initialization */
	ip->i_mp = ntmp;
	ip->i_number = ino;
	ip->i_dev = ntmp->ntm_dev;
	ip->i_uid = ntmp->ntm_uid;
	ip->i_gid = ntmp->ntm_gid;
	ip->i_mode = ntmp->ntm_mode;

	/* Setup internal pointers */
	ip->i_vattrp = NULL;
	ip->i_devvp = ntmp->ntm_devvp;

	*ipp = ip;
	dprintf(("ntfs_ntget: allocated ntnode %d ok\n", ino));

	return (0);
}

void
ntfs_ntrele(
	    struct ntnode * ip)
{
	struct ntvattr *vap;

	dprintf(("ntfs_ntrele: rele ntnode %d\n", ip->i_number));
	while (ip->i_vattrp) {
		vap = ip->i_vattrp;
		ip->i_vattrp = vap->va_nextp;
		ntfs_freentvattr(vap);
	}
	if(ip->i_flag & IN_AATTRNAME) FREE(ip->i_defattrname,M_TEMP);
	dprintf(("ntfs_ntrele: rele ntnode %d ok\n", ip->i_number));
	FREE(ip, M_NTFSNODE);
}

void
ntfs_freentvattr(
		 struct ntvattr * vap)
{
	if (vap->va_flag & NTFS_AF_INRUN) {
		if (vap->va_vruncn)
			FREE(vap->va_vruncn, M_NTFSRUN);
		if (vap->va_vruncl)
			FREE(vap->va_vruncl, M_NTFSRUN);
	} else {
		if (vap->va_datap)
			FREE(vap->va_datap, M_NTFSRDATA);
	}
	FREE(vap, M_NTFSNTVATTR);
}

int
ntfs_attrtontvattr(
		   struct ntfsmount * ntmp,
		   struct ntvattr ** rvapp,
		   struct attr * rap)
{
	int             error, i;
	struct ntvattr *vap;

	error = 0;
	*rvapp = NULL;

	MALLOC(vap, struct ntvattr *, sizeof(*vap), M_NTFSNTVATTR, M_WAITOK);
	vap->va_ip = NULL;
	vap->va_flag = rap->a_hdr.a_flag;
	vap->va_type = rap->a_hdr.a_type;
	vap->va_compression = rap->a_hdr.a_compression;
	vap->va_nextp = NULL;
	vap->va_index = rap->a_hdr.a_index;

	ddprintf(("type: 0x%x, index: %d", vap->va_type, vap->va_index));

	vap->va_namelen = rap->a_hdr.a_namelen;
	if (rap->a_hdr.a_namelen) {
		wchar *unp = (wchar *) ((caddr_t) rap + rap->a_hdr.a_nameoff);
		ddprintf((", name:["));
		for (i = 0; i < vap->va_namelen; i++) {
			vap->va_name[i] = unp[i];
			ddprintf(("%c", vap->va_name[i]));
		}
		ddprintf(("]"));
	}
	if (vap->va_flag & NTFS_AF_INRUN) {
		ddprintf((", nonres."));
		vap->va_datalen = rap->a_nr.a_datalen;
		vap->va_allocated = rap->a_nr.a_allocated;
		vap->va_vcnstart = rap->a_nr.a_vcnstart;
		vap->va_vcnend = rap->a_nr.a_vcnend;
		vap->va_compressalg = rap->a_nr.a_compressalg;
		error = ntfs_runtovrun(&(vap->va_vruncn), &(vap->va_vruncl),
				       &(vap->va_vruncnt),
				       (caddr_t) rap + rap->a_nr.a_dataoff);
	} else {
		vap->va_compressalg = 0;
		ddprintf((", res."));
		vap->va_datalen = rap->a_r.a_datalen;
		vap->va_allocated = rap->a_r.a_datalen;
		vap->va_vcnstart = 0;
		vap->va_vcnend = ntfs_btocn(vap->va_allocated);
		MALLOC(vap->va_datap, caddr_t, vap->va_datalen,
		       M_NTFSRDATA, M_WAITOK);
		memcpy(vap->va_datap, (caddr_t) rap + rap->a_r.a_dataoff,
		       rap->a_r.a_datalen);
	}
	ddprintf((", len: %d", vap->va_datalen));

	if (error)
		FREE(vap, M_NTFSNTVATTR);
	else
		*rvapp = vap;

	ddprintf(("\n"));

	return (error);
}

int
ntfs_runtovrun(
	       cn_t ** rcnp,
	       cn_t ** rclp,
	       u_int32_t * rcntp,
	       u_int8_t * run)
{
	u_int32_t       off;
	u_int32_t       sz, i;
	cn_t           *cn;
	cn_t           *cl;
	u_int32_t       cnt;
	u_int64_t       prev;

	off = 0;
	cnt = 0;
	i = 0;
	while (run[off]) {
		off += (run[off] & 0xF) + ((run[off] >> 4) & 0xF) + 1;
		cnt++;
	}
	MALLOC(cn, cn_t *, cnt * sizeof(cn_t), M_NTFSRUN, M_WAITOK);
	MALLOC(cl, cn_t *, cnt * sizeof(cn_t), M_NTFSRUN, M_WAITOK);

	off = 0;
	cnt = 0;
	prev = 0;
	while (run[off]) {
		u_int64_t       tmp;

		sz = run[off++];
		cl[cnt] = 0;

		for (i = 0; i < (sz & 0xF); i++)
			cl[cnt] += (u_int32_t) run[off++] << (i << 3);

		sz >>= 4;
		if (run[off + sz - 1] & 0x80) {
			tmp = ((u_int64_t) - 1) << (sz << 3);
			for (i = 0; i < sz; i++)
				tmp |= (u_int64_t) run[off++] << (i << 3);
		} else {
			tmp = 0;
			for (i = 0; i < sz; i++)
				tmp |= (u_int64_t) run[off++] << (i << 3);
		}
		if (tmp)
			prev = cn[cnt] = prev + tmp;
		else
			cn[cnt] = tmp;

		cnt++;
	}
	*rcnp = cn;
	*rclp = cl;
	*rcntp = cnt;
	return (0);
}


wchar
ntfs_toupper(
	     struct ntfsmount * ntmp,
	     wchar wc)
{
	return (ntmp->ntm_upcase[wc & 0xFF]);
}

int
ntfs_uustricmp(
	       struct ntfsmount * ntmp,
	       wchar * str1,
	       int str1len,
	       wchar * str2,
	       int str2len)
{
	int             i;
	int             res;

	for (i = 0; i < str1len && i < str2len; i++) {
		res = (int) ntfs_toupper(ntmp, str1[i]) -
			(int) ntfs_toupper(ntmp, str2[i]);
		if (res)
			return res;
	}
	return (str1len - str2len);
}

int
ntfs_uastricmp(
	       struct ntfsmount * ntmp,
	       wchar * str1,
	       int str1len,
	       char *str2,
	       int str2len)
{
	int             i;
	int             res;

	for (i = 0; i < str1len && i < str2len; i++) {
		res = (int) ntfs_toupper(ntmp, str1[i]) -
			(int) ntfs_toupper(ntmp, (wchar) str2[i]);
		if (res)
			return res;
	}
	return (str1len - str2len);
}

int
ntfs_uastrcmp(
	      struct ntfsmount * ntmp,
	      wchar * str1,
	      int str1len,
	      char *str2,
	      int str2len)
{
	int             i;
	int             res;

	for (i = 0; (i < str1len) && (i < str2len); i++) {
		res = ((int) str1[i]) - ((int) str2[i]);
		if (res)
			return res;
	}
	return (str1len - str2len);
}

int
ntfs_ntlookupattr(
		struct ntfsmount * ntmp,
		char * name,
		int namelen,
		int *type,
		char **attrname)
{
	char *sys;
	int syslen,i;
	struct ntvattrdef *adp;

	if (namelen == 0)
		return (0);

	if (name[0] == '$') {
		sys = name;
		for (syslen = 0; syslen < namelen; syslen++) {
			if(sys[syslen] == ':') {
				name++;
				namelen--;
				break;
			}
		}
		name += syslen;
		namelen -= syslen;

		adp = ntmp->ntm_ad;
		for (i = 0; i < ntmp->ntm_adnum; i++){
			if((syslen == adp->ad_namelen) && 
			   (!strncmp(sys,adp->ad_name,syslen))) {
				*type = adp->ad_type;
				if(namelen) {
					MALLOC((*attrname), char *, namelen,
						M_TEMP, M_WAITOK);
					memcpy((*attrname), name, namelen);
					(*attrname)[namelen] = '\0';
				}/* else 
					(*attrname) = NULL;*/
				return (0);
			}
			adp++;
		}
		return (ENOENT);
	}

	if(namelen) {
		MALLOC((*attrname), char *, namelen, M_TEMP, M_WAITOK);
		memcpy((*attrname), name, namelen);
		(*attrname)[namelen] = '\0';
	}

	return (0);
}
/*
 * Lookup specifed node for filename, matching cnp, return filled ntnode.
 */
int
ntfs_ntlookup(
	      struct ntfsmount * ntmp,
	      struct ntnode * ip,
	      struct componentname * cnp,
	      struct ntnode ** ipp)
{
	struct ntvattr *vap;	/* Root attribute */
	cn_t            cn;	/* VCN in current attribute */
	caddr_t         rdbuf;	/* Buffer to read directory's blocks  */
	u_int32_t       blsize;
	u_int32_t       rdsize;	/* Length of data to read from current block */
	struct attr_indexentry *iep;
	int             error, res, anamelen, fnamelen;
	char	       *fname,*aname;
	u_int32_t       aoff;
	struct ntnode  *nip;

	error = ntfs_ntvattrget(ntmp, ip, NTFS_A_INDXROOT, "$I30", 0, &vap);
	if (error || (vap->va_flag & NTFS_AF_INRUN))
		return (ENOTDIR);

	blsize = vap->va_a_iroot->ir_size;
	rdsize = vap->va_datalen;

	fname = cnp->cn_nameptr;
	aname = NULL;
	anamelen = 0;
	for (fnamelen = 0; fnamelen < cnp->cn_namelen; fnamelen++)
		if(fname[fnamelen] == ':') {
			aname = fname + fnamelen + 1;
			anamelen = cnp->cn_namelen - fnamelen - 1;
			dprintf(("ntfs_ntlookup: file %s (%d), attr: %s (%d)\n",
				fname, fnamelen, aname, anamelen));
			break;
		}

	dprintf(("ntfs_ntlookup: blocksize: %d, rdsize: %d\n", blsize, rdsize));

	MALLOC(rdbuf, caddr_t, blsize, M_TEMP, M_WAITOK);

	error = ntfs_breadattr(ntmp, ip, NTFS_A_INDXROOT, "$I30",
			       0, rdsize, rdbuf);
	if (error)
		goto fail;

	aoff = sizeof(struct attr_indexroot);

	do {
		iep = (struct attr_indexentry *) (rdbuf + aoff);

		while (!(iep->ie_flag & NTFS_IEFLAG_LAST) && (rdsize > aoff)) {
			ddprintf(("scan: %d, %d\n",
				  (u_int32_t) iep->ie_number,
				  (u_int32_t) iep->ie_fnametype));
			res = ntfs_uastricmp(ntmp, iep->ie_fname,
					     iep->ie_fnamelen, fname,
					     fnamelen);
			if (res == 0) {
				/* Matched something (case ins.) */
				if (iep->ie_fnametype == 0 ||
				    !(ntmp->ntm_flag & NTFS_MFLAG_CASEINS))
					res = ntfs_uastrcmp(ntmp,
							    iep->ie_fname,
							    iep->ie_fnamelen,
							    fname,
							    fnamelen);
				if (res == 0) {
					error = ntfs_ntget(ntmp,
							   iep->ie_number,
							   &nip);
					if(error)
						goto fail;

					nip->i_fflag = iep->ie_fflag;
					nip->i_pnumber = iep->ie_fpnumber;
					nip->i_times = iep->ie_ftimes;

					if(nip->i_fflag & NTFS_FFLAG_DIR) {
						nip->i_type = VDIR;	
						nip->i_defattr = 0;
						nip->i_defattrname = NULL;
					} else {
						nip->i_type = VREG;	
						nip->i_defattr = NTFS_A_DATA;
						nip->i_defattrname = NULL;
					}
					if (aname) {
						error = ntfs_ntlookupattr(ntmp,
							aname, anamelen,
							&nip->i_defattr,
							&nip->i_defattrname);
						if (error) {
							ntfs_ntrele(nip);
							goto fail;
						}

						nip->i_type = VREG;	

						if (nip->i_defattrname)
							nip->i_flag |= IN_AATTRNAME;
					} else {
						/* Opening default attribute */
						nip->i_size = iep->ie_fsize;
						nip->i_allocated =
							iep->ie_fallocated;
						nip->i_flag |= IN_PRELOADED;
					}
					*ipp = nip;
					goto fail;
				}
			} else if (res > 0)
				break;

			aoff += iep->reclen;
			iep = (struct attr_indexentry *) (rdbuf + aoff);
		}

		/* Dive if possible */
		if (iep->ie_flag & NTFS_IEFLAG_SUBNODE) {
			dprintf(("ntfs_ntlookup: diving\n"));

			cn = *(cn_t *) (rdbuf + aoff +
					iep->reclen - sizeof(cn_t));
			rdsize = blsize;

			error = ntfs_breadattr(ntmp, ip, NTFS_A_INDX, "$I30",
					     ntfs_cntob(cn), rdsize, rdbuf);
			if (error)
				goto fail;

			error = ntfs_procfixups(ntmp, NTFS_INDXMAGIC,
						rdbuf, rdsize);
			if (error)
				goto fail;

			aoff = (((struct attr_indexalloc *) rdbuf)->ia_hdrsize +
				0x18);
		} else {
			dprintf(("ntfs_ntlookup: nowhere to dive :-(\n"));
			error = ENOENT;
			break;
		}
	} while (1);

	dprintf(("finish\n"));

fail:
	ntfs_ntvattrrele(vap);
	FREE(rdbuf, M_TEMP);
	return (error);
}

int
ntfs_isnamepermitted(
		     struct ntfsmount * ntmp,
		     struct attr_indexentry * iep)
{

	if (ntmp->ntm_flag & NTFS_MFLAG_ALLNAMES)
		return 1;

	switch (iep->ie_fnametype) {
	case 2:
		ddprintf(("ntfs_isnamepermitted: skiped DOS name\n"));
		return 0;
	case 0:
	case 1:
	case 3:
		return 1;
	default:
		printf("ntfs_isnamepermitted: " \
		       "WARNING! Unknown file name type: %d\n",
		       iep->ie_fnametype);
		break;
	}
	return 0;
}

/*
 * #undef dprintf #define dprintf(a) printf a
 */
int
ntfs_ntreaddir(
	       struct ntfsmount * ntmp,
	       struct ntnode * ip,
	       u_int32_t num,
	       struct attr_indexentry ** riepp)
{
	struct ntvattr *vap = NULL;	/* IndexRoot attribute */
	struct ntvattr *bmvap = NULL;	/* BitMap attribute */
	struct ntvattr *iavap = NULL;	/* IndexAllocation attribute */
	caddr_t         rdbuf;		/* Buffer to read directory's blocks  */
	u_char         *bmp = NULL;	/* Bitmap */
	u_int32_t       blsize;		/* Index allocation size (2048) */
	u_int32_t       rdsize;		/* Length of data to read */
	u_int32_t       attrnum;	/* Current attribute type */
	u_int32_t       cpbl = 1;	/* Clusters per directory block */
	u_int32_t       blnum;
	struct attr_indexentry *iep;
	int             error = ENOENT;
	u_int32_t       aoff, cnum;

	dprintf(("ntfs_ntreaddir: read ino: %d, num: %d\n", ip->i_number, num));
	error = ntfs_ntvattrget(ntmp, ip, NTFS_A_INDXROOT, "$I30", 0, &vap);
	if (error)
		return (ENOTDIR);

	blsize = ip->i_dirblsz;
	rdbuf = ip->i_dirblbuf;

	dprintf(("ntfs_ntreaddir: rdbuf: 0x%p, blsize: %d\n", rdbuf, blsize));

	if (vap->va_a_iroot->ir_flag & NTFS_IRFLAG_INDXALLOC) {
		error = ntfs_ntvattrget(ntmp, ip, NTFS_A_INDXBITMAP, "$I30",
					0, &bmvap);
		if (error) {
			error = ENOTDIR;
			goto fail;
		}
		MALLOC(bmp, u_char *, bmvap->va_datalen, M_TEMP, M_WAITOK);
		error = ntfs_breadattr(ntmp, ip, NTFS_A_INDXBITMAP, "$I30", 0,
				       bmvap->va_datalen, bmp);
		if (error)
			goto fail;

		error = ntfs_ntvattrget(ntmp, ip, NTFS_A_INDX, "$I30",
					0, &iavap);
		if (error) {
			error = ENOTDIR;
			goto fail;
		}
		cpbl = ntfs_btocn(blsize + ntfs_cntob(1) - 1);
		dprintf(("ntfs_ntreaddir: indexalloc: %d, cpbl: %d\n",
			 iavap->va_datalen, cpbl));
	} else {
		dprintf(("ntfs_ntreadidir: w/o BitMap and IndexAllocation\n"));
		iavap = bmvap = NULL;
		bmp = NULL;
	}

	/* Try use previous values */
	if ((ip->i_lastdnum < num) && (ip->i_lastdnum != 0)) {
		attrnum = ip->i_lastdattr;
		aoff = ip->i_lastdoff;
		blnum = ip->i_lastdblnum;
		cnum = ip->i_lastdnum;
	} else {
		attrnum = NTFS_A_INDXROOT;
		aoff = sizeof(struct attr_indexroot);
		blnum = 0;
		cnum = 0;
	}

	do {
		dprintf(("ntfs_ntreaddir: scan: 0x%x, %d, %d, %d, %d\n",
			 attrnum, (u_int32_t) blnum, cnum, num, aoff));
		rdsize = (attrnum == NTFS_A_INDXROOT) ? vap->va_datalen : blsize;
		error = ntfs_breadattr(ntmp, ip, attrnum, "$I30",
				   ntfs_cntob(blnum * cpbl), rdsize, rdbuf);
		if (error)
			goto fail;

		if (attrnum == NTFS_A_INDX) {
			error = ntfs_procfixups(ntmp, NTFS_INDXMAGIC,
						rdbuf, rdsize);
			if (error)
				goto fail;
		}
		if (aoff == 0)
			aoff = (attrnum == NTFS_A_INDX) ?
				(0x18 + ((struct attr_indexalloc *) rdbuf)->ia_hdrsize) :
				sizeof(struct attr_indexroot);

		iep = (struct attr_indexentry *) (rdbuf + aoff);
		while (!(iep->ie_flag & NTFS_IEFLAG_LAST) && (rdsize > aoff)) {
			if (ntfs_isnamepermitted(ntmp, iep)) {
				if (cnum >= num) {
					ip->i_lastdnum = cnum;
					ip->i_lastdoff = aoff;
					ip->i_lastdblnum = blnum;
					ip->i_lastdattr = attrnum;

					*riepp = iep;

					error = 0;
					goto fail;
				}
				cnum++;
			}
			aoff += iep->reclen;
			iep = (struct attr_indexentry *) (rdbuf + aoff);
		}

		if (iavap) {
			if (attrnum == NTFS_A_INDXROOT)
				blnum = 0;
			else
				blnum++;

			while (ntfs_cntob(blnum * cpbl) < iavap->va_datalen) {
				if (bmp[blnum >> 3] & (1 << (blnum & 3)))
					break;
				blnum++;
			}

			attrnum = NTFS_A_INDX;
			aoff = 0;
			if (ntfs_cntob(blnum * cpbl) >= iavap->va_datalen)
				break;
			dprintf(("ntfs_ntreaddir: blnum: %d\n", (u_int32_t) blnum));
		}
	} while (iavap);

	*riepp = NULL;
	ip->i_lastdnum = 0;

fail:
	if (vap)
		ntfs_ntvattrrele(vap);
	if (bmvap)
		ntfs_ntvattrrele(bmvap);
	if (iavap)
		ntfs_ntvattrrele(iavap);
	if (bmp)
		FREE(bmp, M_TEMP);
	return (error);
}
/*
 * #undef dprintf #define dprintf(a)
 */

struct timespec
ntfs_nttimetounix(
		  u_int64_t nt)
{
	struct timespec t;

	/* WindowNT times are in 100 ns and from 1601 Jan 1 */
	t.tv_nsec = (nt % (1000 * 1000 * 10)) * 100;
	t.tv_sec = nt / (1000 * 1000 * 10) -
		369LL * 365LL * 24LL * 60LL * 60LL -
		89LL * 1LL * 24LL * 60LL * 60LL;
	return (t);
}

int
ntfs_times(
	   struct ntfsmount * ntmp,
	   struct ntnode * ip,
	   ntfs_times_t * tm)
{
	struct ntvattr *vap;
	int             error;

	dprintf(("ntfs_times: ino: %d...\n", ip->i_number));
	error = ntfs_ntvattrget(ntmp, ip, NTFS_A_NAME, NULL, 0, &vap);
	if (error)
		return (error);
	*tm = vap->va_a_name->n_times;
	ntfs_ntvattrrele(vap);

	return (0);
}

int
ntfs_filesize(
	      struct ntfsmount * ntmp,
	      struct ntnode * ip,
	      u_int64_t * size,
	      u_int64_t * bytes)
{
	struct ntvattr *vap;
	u_int64_t       sz, bn;
	int             error;

	dprintf(("ntfs_filesize: ino: %d\n", ip->i_number));
	error = ntfs_ntvattrget(ntmp, ip, ip->i_defattr, ip->i_defattrname,
				0, &vap);
	if (error)
		return (error);
	bn = vap->va_allocated;
	sz = vap->va_datalen;

	dprintf(("ntfs_filesize: %d bytes (%d bytes allocated)\n",
		(u_int32_t) sz, (u_int32_t) bn));

	if (size)
		*size = sz;
	if (bytes)
		*bytes = bn;

	ntfs_ntvattrrele(vap);

	return (0);
}

int
ntfs_breadntvattr_plain(
			struct ntfsmount * ntmp,
			struct ntnode * ip,
			struct ntvattr * vap,
			off_t roff,
			size_t rsize,
			void *rdata,
			size_t * initp)
{
	int             error = 0;
	int             off;

	*initp = 0;
	if (vap->va_flag & NTFS_AF_INRUN) {
		int             cnt;
		cn_t            ccn, ccl, cn, left, cl;
		caddr_t         data = rdata;
		struct buf     *bp;
		size_t          tocopy;

		ddprintf(("ntfs_breadntvattr_plain: data in run: %d chains\n",
			 vap->va_vruncnt));

		off = roff;
		left = rsize;
		ccl = 0;
		ccn = 0;
		cnt = 0;
		while (left && (cnt < vap->va_vruncnt)) {
			ccn = vap->va_vruncn[cnt];
			ccl = vap->va_vruncl[cnt];

			ddprintf(("ntfs_breadntvattr_plain: " \
				 "left %d, cn: 0x%x, cl: %d, off: %d\n", \
				 (u_int32_t) left, (u_int32_t) ccn, \
				 (u_int32_t) ccl, (u_int32_t) off));

			if (ntfs_cntob(ccl) < off) {
				off -= ntfs_cntob(ccl);
				cnt++;
				continue;
			}
			if (ccn || ip->i_number == NTFS_BOOTINO) { /* XXX */
				ccl -= ntfs_btocn(off);
				cn = ccn + ntfs_btocn(off);
				off = ntfs_btocnoff(off);

				while (left && ccl) {
					tocopy = min(left,
						  min(ntfs_cntob(ccl) - off,
						      MAXBSIZE - off));
					cl = ntfs_btocl(tocopy + off);
					ddprintf(("ntfs_breadntvattr_plain: " \
						"read: cn: 0x%x cl: %d, " \
						"off: %d len: %d, left: %d\n",
						(u_int32_t) cn, 
						(u_int32_t) cl, 
						(u_int32_t) off, 
						(u_int32_t) tocopy, 
						(u_int32_t) left));
					error = bread(ntmp->ntm_devvp,
						      ntfs_cntobn(cn),
						      ntfs_cntob(cl),
						      NOCRED, &bp);
					if (error) {
						brelse(bp);
						return (error);
					}
					memcpy(data, bp->b_data + off, tocopy);
					brelse(bp);
					data = data + tocopy;
					*initp += tocopy;
					off = 0;
					left -= tocopy;
					cn += cl;
					ccl -= cl;
				}
			} else {
				tocopy = min(left, ntfs_cntob(ccl) - off);
				ddprintf(("ntfs_breadntvattr_plain: "
					"sparce: ccn: 0x%x ccl: %d, off: %d, " \
					" len: %d, left: %d\n", 
					(u_int32_t) ccn, (u_int32_t) ccl, 
					(u_int32_t) off, (u_int32_t) tocopy, 
					(u_int32_t) left));
				left -= tocopy;
				off = 0;
				bzero(data, tocopy);
				data = data + tocopy;
			}
			cnt++;
		}
		if (left) {
			printf("ntfs_breadntvattr_plain: POSSIBLE RUN ERROR\n");
			error = E2BIG;
		}
	} else {
		ddprintf(("ntfs_breadnvattr_plain: data is in mft record\n"));
		memcpy(rdata, vap->va_datap + roff, rsize);
		*initp += rsize;
	}

	return (error);
}

int
ntfs_breadattr_plain(
		     struct ntfsmount * ntmp,
		     struct ntnode * ip,
		     u_int32_t attrnum,	
		     char *attrname,
		     off_t roff,
		     size_t rsize,
		     void *rdata,
		     size_t * initp)
{
	size_t          init;
	int             error = 0;
	off_t           off = roff, left = rsize, toread;
	caddr_t         data = rdata;
	struct ntvattr *vap;
	*initp = 0;

	while (left) {
		error = ntfs_ntvattrget(ntmp, ip, attrnum, attrname,
					ntfs_btocn(off), &vap);
		if (error)
			return (error);
		toread = min(left, ntfs_cntob(vap->va_vcnend + 1) - off);
		ddprintf(("ntfs_breadattr_plain: o: %d, s: %d (%d - %d)\n",
			 (u_int32_t) off, (u_int32_t) toread,
			 (u_int32_t) vap->va_vcnstart,
			 (u_int32_t) vap->va_vcnend));
		error = ntfs_breadntvattr_plain(ntmp, ip, vap,
					 off - ntfs_cntob(vap->va_vcnstart),
					 toread, data, &init);
		if (error) {
			printf("ntfs_breadattr_plain: " \
			       "ntfs_breadntvattr_plain failed: o: %d, s: %d\n",
			       (u_int32_t) off, (u_int32_t) toread);
			printf("ntfs_breadattr_plain: attrib: %d - %d\n",
			       (u_int32_t) vap->va_vcnstart, 
			       (u_int32_t) vap->va_vcnend);
			ntfs_ntvattrrele(vap);
			break;
		}
		ntfs_ntvattrrele(vap);
		left -= toread;
		off += toread;
		data = data + toread;
		*initp += init;
	}

	return (error);
}

int
ntfs_breadattr(
	       struct ntfsmount * ntmp,
	       struct ntnode * ip,
	       u_int32_t attrnum,
	       char *attrname,
	       off_t roff,
	       size_t rsize,
	       void *rdata)
{
	int             error = 0;
	struct ntvattr *vap;
	size_t          init;

	ddprintf(("ntfs_breadattr: reading %d: 0x%x, from %d size %d bytes\n",
	       ip->i_number, attrnum, (u_int32_t) roff, (u_int32_t) rsize));

	error = ntfs_ntvattrget(ntmp, ip, attrnum, attrname, 0, &vap);
	if (error)
		return (error);

	if ((roff > vap->va_datalen) ||
	    (roff + rsize > vap->va_datalen)) {
		ddprintf(("ntfs_breadattr: offset too big\n"));
		ntfs_ntvattrrele(vap);
		return (E2BIG);
	}
	if (vap->va_compression && vap->va_compressalg) {
		u_int8_t       *cup;
		u_int8_t       *uup;
		off_t           off = roff, left = rsize, tocopy;
		caddr_t         data = rdata;
		cn_t            cn;

		ddprintf(("ntfs_ntreadattr: compression: %d\n",
			 vap->va_compressalg));

		MALLOC(cup, u_int8_t *, ntfs_cntob(NTFS_COMPUNIT_CL),
		       M_NTFSDECOMP, M_WAITOK);
		MALLOC(uup, u_int8_t *, ntfs_cntob(NTFS_COMPUNIT_CL),
		       M_NTFSDECOMP, M_WAITOK);

		cn = (ntfs_btocn(roff)) & (~(NTFS_COMPUNIT_CL - 1));
		off = roff - ntfs_cntob(cn);

		while (left) {
			error = ntfs_breadattr_plain(ntmp, ip, attrnum,
						  attrname, ntfs_cntob(cn),
					          ntfs_cntob(NTFS_COMPUNIT_CL),
						  cup, &init);
			if (error)
				break;

			tocopy = min(left, ntfs_cntob(NTFS_COMPUNIT_CL) - off);

			if (init == ntfs_cntob(NTFS_COMPUNIT_CL)) {
				memcpy(data, cup + off, tocopy);
			} else if (init == 0) {
				bzero(data, tocopy);
			} else {
				error = ntfs_uncompunit(ntmp, uup, cup);
				if (error)
					break;
				memcpy(data, uup + off, tocopy);
			}

			left -= tocopy;
			data = data + tocopy;
			off += tocopy - ntfs_cntob(NTFS_COMPUNIT_CL);
			cn += NTFS_COMPUNIT_CL;
		}

		FREE(uup, M_NTFSDECOMP);
		FREE(cup, M_NTFSDECOMP);
	} else
		error = ntfs_breadattr_plain(ntmp, ip, attrnum, attrname,
					     roff, rsize, rdata, &init);
	ntfs_ntvattrrele(vap);
	return (error);
}

int
ntfs_parserun(
	      cn_t * cn,
	      cn_t * cl,
	      u_int8_t * run,
	      size_t len,
	      int *off)
{
	u_int8_t        sz;
	int             i;

	if (NULL == run) {
		printf("ntfs_runtocn: run == NULL\n");
		return (EINVAL);
	}
	sz = run[(*off)++];
	if (0 == sz) {
		printf("ntfs_parserun: trying to go out of run\n");
		return (E2BIG);
	}
	*cl = 0;
	if ((sz & 0xF) > 8 || (*off) + (sz & 0xF) > len) {
		printf("ntfs_parserun: " \
		       "bad run: length too big: %02x (%x < %x + sz)\n",
		       sz, len, *off);
		return (EINVAL);
	}
	for (i = 0; i < (sz & 0xF); i++)
		*cl += (u_int32_t) run[(*off)++] << (i << 3);

	sz >>= 4;
	if ((sz & 0xF) > 8 || (*off) + (sz & 0xF) > len) {
		printf("ntfs_parserun: " \
		       "bad run: offset too big: %02x (%x < %x + sz)\n",
		       sz, len, *off);
		return (EINVAL);
	}
	for (i = 0; i < (sz & 0xF); i++)
		*cn += (u_int32_t) run[(*off)++] << (i << 3);

	return (0);
}

int
ntfs_procfixups(
		struct ntfsmount * ntmp,
		u_int32_t magic,
		caddr_t buf,
		size_t len)
{
	struct fixuphdr *fhp = (struct fixuphdr *) buf;
	int             i;
	u_int16_t       fixup;
	u_int16_t      *fxp;
	u_int16_t      *cfxp;

	if (fhp->fh_magic != magic) {
		printf("ntfs_procfixups: magic doesn't match: %08x != %08x\n",
		       fhp->fh_magic, magic);
		return (EINVAL);
	}
	if ((fhp->fh_fnum - 1) * ntmp->ntm_bps != len) {
		printf("ntfs_procfixups: " \
		       "bad fixups number: %d for %d bytes block\n", 
		       fhp->fh_fnum, len);
		return (EINVAL);
	}
	if (fhp->fh_foff >= ntmp->ntm_spc * ntmp->ntm_mftrecsz * ntmp->ntm_bps) {
		printf("ntfs_procfixups: invalid offset: %x", fhp->fh_foff);
		return (EINVAL);
	}
	fxp = (u_int16_t *) (buf + fhp->fh_foff);
	cfxp = (u_int16_t *) (buf + ntmp->ntm_bps - 2);
	fixup = *fxp++;
	for (i = 1; i < fhp->fh_fnum; i++, fxp++) {
		if (*cfxp != fixup) {
			printf("ntfs_procfixups: fixup %d doesn't match\n", i);
			return (EINVAL);
		}
		*cfxp = *fxp;
		((caddr_t) cfxp) += ntmp->ntm_bps;
	}
	return (0);
}

int
ntfs_runtocn(
	     cn_t * cn,	
	     struct ntfsmount * ntmp,
	     u_int8_t * run,
	     size_t len,
	     cn_t vcn)
{
	cn_t            ccn = 0;
	cn_t            ccl = 0;
	int             off = 0;
	int             error = 0;

#if NTFS_DEBUG
	int             i;
	printf("ntfs_runtocn: " \
	       "run: 0x%p, %d bytes, vcn:%d\n", run, len, (u_int32_t) vcn);
	printf("ntfs_runtocn: run: ");
	for (i = 0; i < len; i++)
		printf("0x%02x ", run[i]);
	printf("\n");
#endif

	if (NULL == run) {
		printf("ntfs_runtocn: run == NULL\n");
		return (EINVAL);
	}
	do {
		if (run[off] == 0) {
			printf("ntfs_runtocn: vcn too big\n");
			return (E2BIG);
		}
		vcn -= ccl;
		error = ntfs_parserun(&ccn, &ccl, run, len, &off);
		if (error) {
			printf("ntfs_runtocn: ntfs_parserun failed\n");
			return (error);
		}
	} while (ccl <= vcn);
	*cn = ccn + vcn;
	return (0);
}
