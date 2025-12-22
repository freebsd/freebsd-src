/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2009 Rick Macklem, University of Guelph
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
 */

#include <sys/cdefs.h>
#include <fs/nfs/nfsport.h>

extern int nfsrv_useacl;

static int nfsrv_acemasktoperm(u_int32_t acetype, u_int32_t mask, int owner,
    __enum_uint8(vtype) type, acl_perm_t *permp);

/*
 * Handle xdr for an NFSv4 ace.
 */
int
nfsrv_dissectace(struct nfsrv_descript *nd, struct acl_entry *acep,
    bool server, int *aceerrp, int *acesizep)
{
	u_int32_t *tl;
	int len, gotid = 0, owner = 0, error = 0, aceerr = 0;
	u_char *name, namestr[NFSV4_SMALLSTR + 1];
	u_int32_t flag, mask, acetype;
	gid_t gid;
	uid_t uid;

	*aceerrp = 0;
	acep->ae_flags = 0;
	NFSM_DISSECT(tl, u_int32_t *, 4 * NFSX_UNSIGNED);
	acetype = fxdr_unsigned(u_int32_t, *tl++);
	flag = fxdr_unsigned(u_int32_t, *tl++);
	mask = fxdr_unsigned(u_int32_t, *tl++);
	len = fxdr_unsigned(int, *tl);
	/*
	 * The RFCs do not specify a limit to the length of the "who", but
	 * NFSV4_OPAQUELIMIT (1024) should be sufficient.
	 */
	if (len < 0 || len > NFSV4_OPAQUELIMIT) {
		error = NFSERR_BADXDR;
		goto nfsmout;
	} else if (len == 0) {
		/* Netapp filers return a 0 length who for nil users */
		acep->ae_tag = ACL_EVERYONE;	/* Avoid panics. */
		acep->ae_id = ACL_UNDEFINED_ID;
		acep->ae_perm = (acl_perm_t)0;
		acep->ae_entry_type = ACL_ENTRY_TYPE_DENY;
		if (acesizep)
			*acesizep = 4 * NFSX_UNSIGNED;
		error = 0;
		goto nfsmout;
	}
	if (len > NFSV4_SMALLSTR)
		name = malloc(len + 1, M_NFSSTRING, M_WAITOK);
	else
		name = namestr;
	error = nfsrv_mtostr(nd, name, len);
	if (error) {
		if (len > NFSV4_SMALLSTR)
			free(name, M_NFSSTRING);
		goto nfsmout;
	}
	if (len == 6) {
		if (!NFSBCMP(name, "OWNER@", 6)) {
			acep->ae_tag = ACL_USER_OBJ;
			acep->ae_id = ACL_UNDEFINED_ID;
			owner = 1;
			gotid = 1;
		} else if (!NFSBCMP(name, "GROUP@", 6)) {
			acep->ae_tag = ACL_GROUP_OBJ;
			acep->ae_id = ACL_UNDEFINED_ID;
			gotid = 1;
		}
	} else if (len == 9 && !NFSBCMP(name, "EVERYONE@", 9)) {
		acep->ae_tag = ACL_EVERYONE;
		acep->ae_id = ACL_UNDEFINED_ID;
		gotid = 1;
	}
	if (gotid == 0) {
		if (flag & NFSV4ACE_IDENTIFIERGROUP) {
			acep->ae_tag = ACL_GROUP;
			aceerr = nfsv4_strtogid(nd, name, len, &gid);
			if (aceerr == 0)
				acep->ae_id = (uid_t)gid;
		} else {
			acep->ae_tag = ACL_USER;
			aceerr = nfsv4_strtouid(nd, name, len, &uid);
			if (aceerr == 0)
				acep->ae_id = uid;
		}
	}
	if (len > NFSV4_SMALLSTR)
		free(name, M_NFSSTRING);

	if (aceerr == 0) {
		/*
		 * Handle the flags.
		 */
		flag &= ~NFSV4ACE_IDENTIFIERGROUP;
		if (flag & NFSV4ACE_FILEINHERIT) {
			flag &= ~NFSV4ACE_FILEINHERIT;
			acep->ae_flags |= ACL_ENTRY_FILE_INHERIT;
		}
		if (flag & NFSV4ACE_DIRECTORYINHERIT) {
			flag &= ~NFSV4ACE_DIRECTORYINHERIT;
			acep->ae_flags |= ACL_ENTRY_DIRECTORY_INHERIT;
		}
		if (flag & NFSV4ACE_NOPROPAGATEINHERIT) {
			flag &= ~NFSV4ACE_NOPROPAGATEINHERIT;
			acep->ae_flags |= ACL_ENTRY_NO_PROPAGATE_INHERIT;
		}
		if (flag & NFSV4ACE_INHERITONLY) {
			flag &= ~NFSV4ACE_INHERITONLY;
			acep->ae_flags |= ACL_ENTRY_INHERIT_ONLY;
		}
		if (flag & NFSV4ACE_SUCCESSFULACCESS) {
			flag &= ~NFSV4ACE_SUCCESSFULACCESS;
			acep->ae_flags |= ACL_ENTRY_SUCCESSFUL_ACCESS;
		}
		if (flag & NFSV4ACE_FAILEDACCESS) {
			flag &= ~NFSV4ACE_FAILEDACCESS;
			acep->ae_flags |= ACL_ENTRY_FAILED_ACCESS;
		}
		/*
		 * Set ae_entry_type.
		 */
		if (acetype == NFSV4ACE_ALLOWEDTYPE)
			acep->ae_entry_type = ACL_ENTRY_TYPE_ALLOW;
		else if (acetype == NFSV4ACE_DENIEDTYPE)
			acep->ae_entry_type = ACL_ENTRY_TYPE_DENY;
		else if (!server && acetype == NFSV4ACE_AUDITTYPE)
			acep->ae_entry_type = ACL_ENTRY_TYPE_AUDIT;
		else if (!server && acetype == NFSV4ACE_ALARMTYPE)
			acep->ae_entry_type = ACL_ENTRY_TYPE_ALARM;
		else
			aceerr = NFSERR_ATTRNOTSUPP;
	}

	/*
	 * Now, check for unsupported flag bits.
	 */
	if (aceerr == 0 && flag != 0)
		aceerr = NFSERR_ATTRNOTSUPP;

	/*
	 * And turn the mask into perm bits.
	 */
	if (aceerr == 0)
		aceerr = nfsrv_acemasktoperm(acetype, mask, owner, VREG,
		    &acep->ae_perm);
	*aceerrp = aceerr;
	if (acesizep)
		*acesizep = NFSM_RNDUP(len) + (4 * NFSX_UNSIGNED);
	error = 0;
nfsmout:
	NFSEXITCODE(error);
	return (error);
}

static acl_tag_t nfsv4_to_posixacltag[NFSV4_POSIXACL_TAG_OTHER + 1] =
    { ACL_UNDEFINED_TAG, ACL_USER_OBJ, ACL_USER, ACL_GROUP_OBJ,
    ACL_GROUP, ACL_MASK, ACL_OTHER };

/*
 * Handle xdr for a POSIX draft ace.
 */
int
nfsrv_dissectposixace(struct nfsrv_descript *nd, struct acl_entry *acep,
    bool server, int *aceerrp, int *acesizep)
{
	uint32_t *tl, tag;
	int len, error = 0, aceerr = 0;
	u_char *name, namestr[NFSV4_SMALLSTR + 1];
	gid_t gid;
	uid_t uid;

	*aceerrp = 0;
	NFSM_DISSECT(tl, uint32_t *, 3 * NFSX_UNSIGNED);
	tag = fxdr_unsigned(uint32_t, *tl++);
	acep->ae_perm = fxdr_unsigned(acl_perm_t, *tl++);
	len = fxdr_unsigned(int, *tl);
	/*
	 * The RFCs do not specify a limit to the length of the "who", but
	 * NFSV4_OPAQUELIMIT (1024) should be sufficient.
	 */
	if (len < 0 || len > NFSV4_OPAQUELIMIT) {
		error = NFSERR_BADXDR;
		goto nfsmout;
	}
	if (tag < NFSV4_POSIXACL_TAG_USER_OBJ ||
	    tag > NFSV4_POSIXACL_TAG_OTHER) {
		error = NFSERR_ATTRNOTSUPP;
		goto nfsmout;
	}
	acep->ae_tag = nfsv4_to_posixacltag[tag];
	if (len > NFSV4_SMALLSTR)
		name = malloc(len + 1, M_NFSSTRING, M_WAITOK);
	else
		name = namestr;
	if (len > 0)
		error = nfsrv_mtostr(nd, name, len);
	if (error != 0) {
		if (len > NFSV4_SMALLSTR)
			free(name, M_NFSSTRING);
		goto nfsmout;
	}
	switch (acep->ae_tag) {
	case ACL_USER:
		aceerr = nfsv4_strtouid(nd, name, len, &uid);
		if (aceerr == 0)
			acep->ae_id = uid;
		break;
	case ACL_GROUP:
		aceerr = nfsv4_strtogid(nd, name, len, &gid);
		if (aceerr == 0)
			acep->ae_id = (uid_t)gid;
		break;
	case ACL_USER_OBJ:
	case ACL_GROUP_OBJ:
	case ACL_MASK:
	case ACL_OTHER:
		break;
	default:
		aceerr = NFSERR_ATTRNOTSUPP;
	}
	if (len > NFSV4_SMALLSTR)
		free(name, M_NFSSTRING);

	*aceerrp = aceerr;
	if (acesizep != NULL)
		*acesizep = NFSM_RNDUP(len) + (3 * NFSX_UNSIGNED);
	error = 0;
nfsmout:
	NFSEXITCODE(error);
	return (error);
}

/*
 * Turn an NFSv4 ace mask into R/W/X flag bits.
 */
static int
nfsrv_acemasktoperm(u_int32_t acetype, u_int32_t mask, int owner,
    __enum_uint8(vtype) type, acl_perm_t *permp)
{
	acl_perm_t perm = 0x0;
	int error = 0;

	if (mask & NFSV4ACE_READDATA) {
		mask &= ~NFSV4ACE_READDATA;
		perm |= ACL_READ_DATA;
	}
	if (mask & NFSV4ACE_LISTDIRECTORY) {
		mask &= ~NFSV4ACE_LISTDIRECTORY;
		perm |= ACL_LIST_DIRECTORY;
	}
	if (mask & NFSV4ACE_WRITEDATA) {
		mask &= ~NFSV4ACE_WRITEDATA;
		perm |= ACL_WRITE_DATA;
	}
	if (mask & NFSV4ACE_ADDFILE) {
		mask &= ~NFSV4ACE_ADDFILE;
		perm |= ACL_ADD_FILE;
	}
	if (mask & NFSV4ACE_APPENDDATA) {
		mask &= ~NFSV4ACE_APPENDDATA;
		perm |= ACL_APPEND_DATA;
	}
	if (mask & NFSV4ACE_ADDSUBDIRECTORY) {
		mask &= ~NFSV4ACE_ADDSUBDIRECTORY;
		perm |= ACL_ADD_SUBDIRECTORY;
	}
	if (mask & NFSV4ACE_READNAMEDATTR) {
		mask &= ~NFSV4ACE_READNAMEDATTR;
		perm |= ACL_READ_NAMED_ATTRS;
	}
	if (mask & NFSV4ACE_WRITENAMEDATTR) {
		mask &= ~NFSV4ACE_WRITENAMEDATTR;
		perm |= ACL_WRITE_NAMED_ATTRS;
	}
	if (mask & NFSV4ACE_EXECUTE) {
		mask &= ~NFSV4ACE_EXECUTE;
		perm |= ACL_EXECUTE;
	}
	if (mask & NFSV4ACE_SEARCH) {
		mask &= ~NFSV4ACE_SEARCH;
		perm |= ACL_EXECUTE;
	}
	if (mask & NFSV4ACE_DELETECHILD) {
		mask &= ~NFSV4ACE_DELETECHILD;
		perm |= ACL_DELETE_CHILD;
	}
	if (mask & NFSV4ACE_READATTRIBUTES) {
		mask &= ~NFSV4ACE_READATTRIBUTES;
		perm |= ACL_READ_ATTRIBUTES;
	}
	if (mask & NFSV4ACE_WRITEATTRIBUTES) {
		mask &= ~NFSV4ACE_WRITEATTRIBUTES;
		perm |= ACL_WRITE_ATTRIBUTES;
	}
	if (mask & NFSV4ACE_DELETE) {
		mask &= ~NFSV4ACE_DELETE;
		perm |= ACL_DELETE;
	}
	if (mask & NFSV4ACE_READACL) {
		mask &= ~NFSV4ACE_READACL;
		perm |= ACL_READ_ACL;
	}
	if (mask & NFSV4ACE_WRITEACL) {
		mask &= ~NFSV4ACE_WRITEACL;
		perm |= ACL_WRITE_ACL;
	}
	if (mask & NFSV4ACE_WRITEOWNER) {
		mask &= ~NFSV4ACE_WRITEOWNER;
		perm |= ACL_WRITE_OWNER;
	}
	if (mask & NFSV4ACE_SYNCHRONIZE) {
		mask &= ~NFSV4ACE_SYNCHRONIZE;
		perm |= ACL_SYNCHRONIZE;
	}
	if (mask != 0) {
		error = NFSERR_ATTRNOTSUPP;
		goto out;
	}
	*permp = perm;

out:
	NFSEXITCODE(error);
	return (error);
}

/* local functions */
static int nfsrv_buildace(struct nfsrv_descript *, u_char *, int,
    __enum_uint8(vtype), int, int, struct acl_entry *);
static int nfsrv_buildposixace(struct nfsrv_descript *, u_char *, int,
    struct acl_entry *);

/*
 * This function builds an NFSv4 ace.
 */
static int
nfsrv_buildace(struct nfsrv_descript *nd, u_char *name, int namelen,
    __enum_uint8(vtype) type, int group, int owner, struct acl_entry *ace)
{
	u_int32_t *tl, aceflag = 0x0, acemask = 0x0, acetype;
	int full_len;

	full_len = NFSM_RNDUP(namelen);
	NFSM_BUILD(tl, u_int32_t *, 4 * NFSX_UNSIGNED + full_len);

	/*
	 * Fill in the ace type.
	 */
	if (ace->ae_entry_type & ACL_ENTRY_TYPE_ALLOW)
		acetype = NFSV4ACE_ALLOWEDTYPE;
	else if (ace->ae_entry_type & ACL_ENTRY_TYPE_DENY)
		acetype = NFSV4ACE_DENIEDTYPE;
	else if (ace->ae_entry_type & ACL_ENTRY_TYPE_AUDIT)
		acetype = NFSV4ACE_AUDITTYPE;
	else
		acetype = NFSV4ACE_ALARMTYPE;
	*tl++ = txdr_unsigned(acetype);

	/*
	 * Set the flag bits from the ACL.
	 */
	if (ace->ae_flags & ACL_ENTRY_FILE_INHERIT)
		aceflag |= NFSV4ACE_FILEINHERIT;
	if (ace->ae_flags & ACL_ENTRY_DIRECTORY_INHERIT)
		aceflag |= NFSV4ACE_DIRECTORYINHERIT;
	if (ace->ae_flags & ACL_ENTRY_NO_PROPAGATE_INHERIT)
		aceflag |= NFSV4ACE_NOPROPAGATEINHERIT;
	if (ace->ae_flags & ACL_ENTRY_INHERIT_ONLY)
		aceflag |= NFSV4ACE_INHERITONLY;
	if (ace->ae_flags & ACL_ENTRY_SUCCESSFUL_ACCESS)
		aceflag |= NFSV4ACE_SUCCESSFULACCESS;
	if (ace->ae_flags & ACL_ENTRY_FAILED_ACCESS)
		aceflag |= NFSV4ACE_FAILEDACCESS;
	if (group)
		aceflag |= NFSV4ACE_IDENTIFIERGROUP;
	*tl++ = txdr_unsigned(aceflag);
	if (type == VDIR) {
		if (ace->ae_perm & ACL_LIST_DIRECTORY)
			acemask |= NFSV4ACE_LISTDIRECTORY;
		if (ace->ae_perm & ACL_ADD_FILE)
			acemask |= NFSV4ACE_ADDFILE;
		if (ace->ae_perm & ACL_ADD_SUBDIRECTORY)
			acemask |= NFSV4ACE_ADDSUBDIRECTORY;
		if (ace->ae_perm & ACL_READ_NAMED_ATTRS)
			acemask |= NFSV4ACE_READNAMEDATTR;
		if (ace->ae_perm & ACL_WRITE_NAMED_ATTRS)
			acemask |= NFSV4ACE_WRITENAMEDATTR;
		if (ace->ae_perm & ACL_EXECUTE)
			acemask |= NFSV4ACE_SEARCH;
		if (ace->ae_perm & ACL_DELETE_CHILD)
			acemask |= NFSV4ACE_DELETECHILD;
		if (ace->ae_perm & ACL_READ_ATTRIBUTES)
			acemask |= NFSV4ACE_READATTRIBUTES;
		if (ace->ae_perm & ACL_WRITE_ATTRIBUTES)
			acemask |= NFSV4ACE_WRITEATTRIBUTES;
		if (ace->ae_perm & ACL_DELETE)
			acemask |= NFSV4ACE_DELETE;
		if (ace->ae_perm & ACL_READ_ACL)
			acemask |= NFSV4ACE_READACL;
		if (ace->ae_perm & ACL_WRITE_ACL)
			acemask |= NFSV4ACE_WRITEACL;
		if (ace->ae_perm & ACL_WRITE_OWNER)
			acemask |= NFSV4ACE_WRITEOWNER;
		if (ace->ae_perm & ACL_SYNCHRONIZE)
			acemask |= NFSV4ACE_SYNCHRONIZE;
	} else {
		acemask = nfs_aceperm(ace->ae_perm);
	}
	*tl++ = txdr_unsigned(acemask);
	*tl++ = txdr_unsigned(namelen);
	if (full_len - namelen)
		*(tl + (namelen / NFSX_UNSIGNED)) = 0x0;
	NFSBCOPY(name, (caddr_t)tl, namelen);
	return (full_len + 4 * NFSX_UNSIGNED);
}

/*
 * Convert ae_perm to NFSv4 ACL acemask4 for regular files.
 */
uint32_t
nfs_aceperm(acl_perm_t ae_perm)
{
	uint32_t acemask = 0x0;

	if (ae_perm & ACL_READ_DATA)
		acemask |= NFSV4ACE_READDATA;
	if (ae_perm & ACL_WRITE_DATA)
		acemask |= NFSV4ACE_WRITEDATA;
	if (ae_perm & ACL_APPEND_DATA)
		acemask |= NFSV4ACE_APPENDDATA;
	if (ae_perm & ACL_READ_NAMED_ATTRS)
		acemask |= NFSV4ACE_READNAMEDATTR;
	if (ae_perm & ACL_WRITE_NAMED_ATTRS)
		acemask |= NFSV4ACE_WRITENAMEDATTR;
	if (ae_perm & ACL_EXECUTE)
		acemask |= NFSV4ACE_EXECUTE;
	if (ae_perm & ACL_READ_ATTRIBUTES)
		acemask |= NFSV4ACE_READATTRIBUTES;
	if (ae_perm & ACL_WRITE_ATTRIBUTES)
		acemask |= NFSV4ACE_WRITEATTRIBUTES;
	if (ae_perm & ACL_DELETE)
		acemask |= NFSV4ACE_DELETE;
	if (ae_perm & ACL_READ_ACL)
		acemask |= NFSV4ACE_READACL;
	if (ae_perm & ACL_WRITE_ACL)
		acemask |= NFSV4ACE_WRITEACL;
	if (ae_perm & ACL_WRITE_OWNER)
		acemask |= NFSV4ACE_WRITEOWNER;
	if (ae_perm & ACL_SYNCHRONIZE)
		acemask |= NFSV4ACE_SYNCHRONIZE;
	return (acemask);
}

/*
 * This function builds a POSIX draft ace.
 */
static int
nfsrv_buildposixace(struct nfsrv_descript *nd, u_char *name, int namelen,
    struct acl_entry *ace)
{
	uint32_t *tl;
	int full_len;

	full_len = NFSM_RNDUP(namelen);
	NFSM_BUILD(tl, uint32_t *, 3 * NFSX_UNSIGNED + full_len);

	/*
	 * Fill in the ace tag.
	 */
	switch (ace->ae_tag) {
	case ACL_USER_OBJ:
		*tl++ = txdr_unsigned(NFSV4_POSIXACL_TAG_USER_OBJ);
		break;
	case ACL_USER:
		*tl++ = txdr_unsigned(NFSV4_POSIXACL_TAG_USER);
		break;
	case ACL_GROUP_OBJ:
		*tl++ = txdr_unsigned(NFSV4_POSIXACL_TAG_GROUP_OBJ);
		break;
	case ACL_GROUP:
		*tl++ = txdr_unsigned(NFSV4_POSIXACL_TAG_GROUP);
		break;
	case ACL_MASK:
		*tl++ = txdr_unsigned(NFSV4_POSIXACL_TAG_MASK);
		break;
	case ACL_OTHER:
		*tl++ = txdr_unsigned(NFSV4_POSIXACL_TAG_OTHER);
		break;
	default:
		printf("nfsrv_buildposixace: bad ae_tag 0x%x\n", ace->ae_tag);
		*tl++ = txdr_unsigned(0);
	}

	/*
	 * Fill in the permission bits.
	 */
	*tl++ = txdr_unsigned(ace->ae_perm);
	*tl++ = txdr_unsigned(namelen);
	if (namelen > 0) {
		if (full_len - namelen)
			*(tl + (namelen / NFSX_UNSIGNED)) = 0x0;
		memcpy(tl, name, namelen);
	}
	return (full_len + 3 * NFSX_UNSIGNED);
}

/*
 * Build an NFSv4 ACL.
 */
int
nfsrv_buildacl(struct nfsrv_descript *nd, NFSACL_T *aclp, __enum_uint8(vtype) type,
    NFSPROC_T *p)
{
	int i, entrycnt = 0, retlen;
	u_int32_t *entrycntp;
	int isowner, isgroup, namelen, malloced;
	u_char *name, namestr[NFSV4_SMALLSTR];

	NFSM_BUILD(entrycntp, u_int32_t *, NFSX_UNSIGNED);
	retlen = NFSX_UNSIGNED;
	/*
	 * Loop through the acl entries, building each one.
	 */
	for (i = 0; i < aclp->acl_cnt; i++) {
		isowner = isgroup = malloced = 0;
		switch (aclp->acl_entry[i].ae_tag) {
		case ACL_USER_OBJ:
			isowner = 1;
			name = "OWNER@";
			namelen = 6;
			break;
		case ACL_GROUP_OBJ:
			isgroup = 1;
			name = "GROUP@";
			namelen = 6;
			break;
		case ACL_EVERYONE:
			name = "EVERYONE@";
			namelen = 9;
			break;
		case ACL_USER:
			name = namestr;
			nfsv4_uidtostr(aclp->acl_entry[i].ae_id, &name,
			    &namelen);
			if (name != namestr)
				malloced = 1;
			break;
		case ACL_GROUP:
			isgroup = 1;
			name = namestr;
			nfsv4_gidtostr((gid_t)aclp->acl_entry[i].ae_id, &name,
			    &namelen);
			if (name != namestr)
				malloced = 1;
			break;
		default:
			continue;
		}
		retlen += nfsrv_buildace(nd, name, namelen, type, isgroup,
		    isowner, &aclp->acl_entry[i]);
		entrycnt++;
		if (malloced)
			free(name, M_NFSSTRING);
	}
	*entrycntp = txdr_unsigned(entrycnt);
	return (retlen);
}

/*
 * Build a POSIX draft ACL.
 */
int
nfsrv_buildposixacl(struct nfsrv_descript *nd, NFSACL_T *aclp,
    acl_type_t acltype)
{
	int i, entrycnt = 0, retlen;
	uint32_t *entrycntp;
	unsigned int cnt;
	int namelen;
	u_char *name, namestr[NFSV4_SMALLSTR];
	bool malloced;

	NFSM_BUILD(entrycntp, uint32_t *, NFSX_UNSIGNED);
	retlen = NFSX_UNSIGNED;
	cnt = 0;
	if (aclp != NULL)
		cnt = aclp->acl_cnt;
	/*
	 * Loop through the acl entries, building each one.
	 */
	for (i = 0; i < cnt; i++) {
		malloced = false;
		switch (aclp->acl_entry[i].ae_tag) {
		case ACL_USER_OBJ:
		case ACL_GROUP_OBJ:
		case ACL_OTHER:
		case ACL_MASK:
			namelen = 0;
			break;
		case ACL_USER:
			name = namestr;
			nfsv4_uidtostr(aclp->acl_entry[i].ae_id, &name,
			    &namelen);
			if (name != namestr)
				malloced = true;
			break;
		case ACL_GROUP:
			name = namestr;
			nfsv4_gidtostr((gid_t)aclp->acl_entry[i].ae_id, &name,
			    &namelen);
			if (name != namestr)
				malloced = true;
			break;
		default:
			continue;
		}
		retlen += nfsrv_buildposixace(nd, name, namelen,
		    &aclp->acl_entry[i]);
		entrycnt++;
		if (malloced)
			free(name, M_NFSSTRING);
	}
	*entrycntp = txdr_unsigned(entrycnt);
	return (retlen);
}

/*
 * Compare two NFSv4 acls.
 * Return 0 if they are the same, 1 if not the same.
 */
int
nfsrv_compareacl(NFSACL_T *aclp1, NFSACL_T *aclp2)
{
	int i;
	struct acl_entry *acep1, *acep2;

	if (aclp1->acl_cnt != aclp2->acl_cnt)
		return (1);
	acep1 = aclp1->acl_entry;
	acep2 = aclp2->acl_entry;
	for (i = 0; i < aclp1->acl_cnt; i++) {
		if (acep1->ae_tag != acep2->ae_tag)
			return (1);
		switch (acep1->ae_tag) {
		case ACL_GROUP:
		case ACL_USER:
			if (acep1->ae_id != acep2->ae_id)
				return (1);
			/* fall through */
		case ACL_USER_OBJ:
		case ACL_GROUP_OBJ:
		case ACL_OTHER:
			if (acep1->ae_perm != acep2->ae_perm)
				return (1);
		}
		acep1++;
		acep2++;
	}
	return (0);
}
