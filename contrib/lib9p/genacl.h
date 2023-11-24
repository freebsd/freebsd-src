/*
 * Copyright 2016 Chris Torek <torek@ixsystems.com>
 * All rights reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted providing that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * General ACL support for 9P2000.L.
 *
 * We mostly use Linux's xattr name space and nfs4 ACL bits, as
 * these are the most general forms available.
 *
 * Linux requests attributes named
 *
 *     "system.posix_acl_default"
 *     "system.posix_acl_access"
 *
 * to get POSIX style ACLs, and:
 *
 *     "system.nfs4_acl"
 *
 * to get NFSv4 style ACLs.  The v9fs client does not explicitly
 * ask for the latter, but if you use the Ubuntu nfs4-acl-tools
 * package, it should be able to read and write these.
 *
 * For the record, the Linux kernel source code also shows:
 *
 *  - Lustre uses "trusted.*", with "*" matching "lov", "lma",
 *    "lmv", "dmv", "link", "fid", "version", "som", "hsm", and
 *    "lfsck_namespace".
 *
 *  - ceph has a name tree of the form "ceph.<type>.<name>" with
 *     <type,name> pairs like <"dir","entries">, <"dir","files>,
 *     <"file","layout">, and so on.
 *
 *  - ext4 uses the POSIX names, plus some special ext4-specific
 *    goop that might not get externalized.
 *
 *  - NFS uses both the POSIX names and the NFSv4 ACLs.  However,
 *    what it mainly does is have nfsd generate fake NFSv4 ACLs
 *    from POSIX ACLs.  If you run an NFS client, the client
 *    relies on the server actually implementing the ACLs, and
 *    lets nfs4-acl-tools read and write the system.nfs4_acl xattr
 *    data.  If you run an NFS server off, e.g., an ext4 file system,
 *    the server looks for the system.nfs4_acl xattr, serves that
 *    out if found, and otherwise just generates the fakes.
 *
 *  - "security.*" and "selinux.*" are reserved.
 *
 *  - "security.capability" is the name for capabilities.
 *
 *  - sockets use "system.sockprotoname".
 */

#if defined(__APPLE__)
  #define HAVE_POSIX_ACLS
  #define HAVE_DARWIN_ACLS
#endif

#if defined(__FreeBSD__)
  #define HAVE_POSIX_ACLS
  #define HAVE_FREEBSD_ACLS
#endif

#include <sys/types.h>
#include <sys/acl.h>		/* XXX assumes existence of sys/acl.h */

/*
 * An ACL consists of a number of ACEs that grant some kind of
 * "allow" or "deny" to some specific entity.
 *
 * The number of ACEs is potentially unlimited, although in practice
 * they tend not to be that long.
 *
 * It's the responsibility of the back-end to supply the ACL
 * for each test.  However, the ACL may be in some sort of
 * system-specific form.  It's the responsibility of some
 * (system-specific) code to translate it to *this* form, after
 * which the backend may use l9p_acl_check_access() to get
 * access granted or denied (and, eventually, audits and alarms
 * recorded and raises, although that's yet to be designed).
 *
 * The reason for all this faffing-about with formats is so that
 * we can *report* the ACLs using Linux 9p style xattrs.
 */

struct l9p_acl;
struct l9p_fid;

void l9p_acl_free(struct l9p_acl *);

/*
 * An ACL is made up of ACEs.
 *
 * Each ACE has:
 *
 *   - a type: allow, deny, audit, alarm
 *   - a set of flags
 *   - permissions bits: a "mask"
 *   - an optional, nominally-variable-length identity
 *
 * The last part is especially tricky and currently has limited
 * support here: it's always a 16 byte field on Darwin, and just
 * a uint32_t on BSD (should be larger, really).  Linux supports
 * very large, actually-variable-size values; we'll deal with
 * this later, maybe.
 *
 * We will define the mask first, below, since these are also the bits
 * passed in for the accmask argument to l9p_acl_check_access().
 */

/*
 * ACL entry mask, and accmask argument flags.
 *
 * NB: not every bit is implemented, but they are all here because
 * they are all defined as part of an NFSv4 ACL entry, which is
 * more or less a superset of a POSIX ACL entry.  This means you
 * can put a complete NFSv4 ACL in and we can reproduce it.
 *
 * Note that the LIST_DIRECTORY, ADD_FILE, and ADD_SUBDIRECTORY bits
 * apply only to a directory, while the READ_DATA, WRITE_DATA, and
 * APPEND_DATA bits apply only to a file.  See aca_parent/aca_child
 * below.
 */
#define	L9P_ACE_READ_DATA		0x00001
#define	L9P_ACE_LIST_DIRECTORY		0x00001 /* same as READ_DATA */
#define	L9P_ACE_WRITE_DATA		0x00002
#define	L9P_ACE_ADD_FILE		0x00002 /* same as WRITE_DATA */
#define	L9P_ACE_APPEND_DATA		0x00004
#define	L9P_ACE_ADD_SUBDIRECTORY	0x00004 /* same as APPEND_DATA */
#define	L9P_ACE_READ_NAMED_ATTRS	0x00008
#define	L9P_ACE_WRITE_NAMED_ATTRS	0x00010
#define	L9P_ACE_EXECUTE			0x00020
#define	L9P_ACE_DELETE_CHILD		0x00040
#define	L9P_ACE_READ_ATTRIBUTES		0x00080
#define	L9P_ACE_WRITE_ATTRIBUTES	0x00100
#define	L9P_ACE_WRITE_RETENTION		0x00200 /* not used here */
#define	L9P_ACE_WRITE_RETENTION_HOLD	0x00400 /* not used here */
/*					0x00800 unused? */
#define	L9P_ACE_DELETE			0x01000
#define	L9P_ACE_READ_ACL		0x02000
#define	L9P_ACE_WRITE_ACL		0x04000
#define	L9P_ACE_WRITE_OWNER		0x08000
#define	L9P_ACE_SYNCHRONIZE		0x10000 /* not used here */

/*
 * This is not an ACE bit, but is used with the access checking
 * below.  It represents a request to unlink (delete child /
 * delete) an entity, and is equivalent to asking for *either*
 * (not both) permission.
 */
#define	L9P_ACOP_UNLINK (L9P_ACE_DELETE_CHILD | L9P_ACE_DELETE)

/*
 * Access checking takes a lot of arguments, so they are
 * collected into a "struct" here.
 *
 * The aca_parent and aca_pstat fields may/must be NULL if the
 * operation itself does not involve "directory" permissions.
 * The aca_child and aca_cstat fields may/must be NULL if the
 * operation does not involve anything *but* a directory.  This
 * is how we decide whether you're interested in L9P_ACE_READ_DATA
 * vs L9P_ACE_LIST_DIRECTORY, for instance.
 *
 * Note that it's OK for both parent and child to be directories
 * (as is the case when we're adding or deleting a subdirectory).
 */
struct l9p_acl_check_args {
	uid_t	aca_uid;		/* the uid that is requesting access */
	gid_t	aca_gid;		/* the gid that is requesting access */
	gid_t	*aca_groups;		/* the additional group-set, if any */
	size_t	aca_ngroups;		/* number of groups in group-set */
	struct l9p_acl *aca_parent;	/* ACLs associated with parent/dir */
	struct stat *aca_pstat;		/* stat data for parent/dir */
	struct l9p_acl *aca_child;	/* ACLs associated with file */
	struct stat *aca_cstat;		/* stat data for file */
	int	aca_aclmode;		/* mode checking bits, see below */
	bool	aca_superuser;		/* alway allow uid==0 in STAT_MODE */
};

/*
 * Access checking mode bits in aca_checkmode.  If you enable
 * ACLs, they are used first, optionally with ZFS style ACLs.
 * This means that even if aca_superuser is set, if an ACL denies
 * permission to uid 0, permission is really denied.
 *
 * NFS style ACLs run before POSIX style ACLs (though POSIX
 * ACLs aren't done yet anyway).
 *
 * N.B.: you probably want L9P_ACL_ZFS, especially when operating
 * with a ZFS file system on FreeBSD.
 */
#define	L9P_ACM_NFS_ACL		0x0001	/* enable NFS ACL checking */
#define	L9P_ACM_ZFS_ACL		0x0002	/* use ZFS ACL unlink semantics */
#define	L9P_ACM_POSIX_ACL	0x0004	/* enable POSIX ACL checking (notyet) */
#define	L9P_ACM_STAT_MODE	0x0008	/* enable st_mode bits */

/*
 * Requests to access some file or directory must provide:
 *
 *  - An operation.  This should usually be just one bit from the
 *    L9P_ACE_* bit-sets above, or our special L9P_ACOP_UNLINK.
 *    For a few file-open operations it may be multiple bits,
 *    e.g., both read and write data.
 *  - The identity of the accessor: uid + gid + gid-set.
 *  - The type of access desired: this may be multiple bits.
 *  - The parent directory, if applicable.
 *  - The child file/dir being accessed, if applicable.
 *  - stat data for parent and/or child, if applicable.
 *
 * The ACLs and/or stat data of the parent and/or child get used
 * here, so the caller must provide them.  We should have a way to
 * cache these on fids, but not yet.  The parent and child
 * arguments are a bit tricky; see the code in genacl.c.
 */
int l9p_acl_check_access(int32_t op, struct l9p_acl_check_args *args);

/*
 * When falling back to POSIX ACL or Unix-style permissions
 * testing, it's nice to collapse the above detailed permissions
 * into simple read/write/execute bits (value 0..7).  We provide
 * a small utility function that does this.
 */
int l9p_ace_mask_to_rwx(int32_t);

/*
 * The rest of the data in an ACE.
 */

/* type in ace_type */
#define	L9P_ACET_ACCESS_ALLOWED		0
#define	L9P_ACET_ACCESS_DENIED		1
#define	L9P_ACET_SYSTEM_AUDIT		2
#define	L9P_ACET_SYSTEM_ALARM		3

/* flags in ace_flags */
#define	L9P_ACEF_FILE_INHERIT_ACE		0x001
#define	L9P_ACEF_DIRECTORY_INHERIT_ACE		0x002
#define	L9P_ACEF_NO_PROPAGATE_INHERIT_ACE	0x004
#define	L9P_ACEF_INHERIT_ONLY_ACE		0x008
#define	L9P_ACEF_SUCCESSFUL_ACCESS_ACE_FLAG	0x010
#define	L9P_ACEF_FAILED_ACCESS_ACE_FLAG		0x020
#define	L9P_ACEF_IDENTIFIER_GROUP		0x040
#define	L9P_ACEF_OWNER				0x080
#define	L9P_ACEF_GROUP				0x100
#define	L9P_ACEF_EVERYONE			0x200

#if defined(__APPLE__)
#  define L9P_ACE_IDSIZE 16 /* but, how do we map Darwin uuid? */
#else
#  define L9P_ACE_IDSIZE 4
#endif

struct l9p_ace {
	uint16_t ace_type;		/* ACL entry type */
	uint16_t ace_flags;		/* ACL entry flags */
	uint32_t ace_mask;		/* ACL entry mask */
	uint32_t ace_idsize;		/* length of ace_idbytes */
	unsigned char ace_idbytes[L9P_ACE_IDSIZE];
};

#define	L9P_ACLTYPE_NFSv4	1	/* currently the only valid type */
struct l9p_acl {
	uint32_t acl_acetype;		/* reserved for future expansion */
	uint32_t acl_nace;		/* number of occupied ACEs */
	uint32_t acl_aceasize;		/* actual size of ACE array */
	struct l9p_ace acl_aces[];	/* variable length ACE array */
};

/*
 * These are the system-specific converters.
 *
 * Right now the backend needs to just find BSD NFSv4 ACLs
 * and convert them before each operation that needs to be
 * tested.
 */
#if defined(HAVE_DARWIN_ACLS)
struct l9p_acl *l9p_darwin_nfsv4acl_to_acl(acl_t acl);
#endif

#if defined(HAVE_FREEBSD_ACLS)
struct l9p_acl *l9p_freebsd_nfsv4acl_to_acl(acl_t acl);
#endif

#if defined(HAVE_POSIX_ACLS) && 0 /* not yet */
struct l9p_acl *l9p_posix_acl_to_acl(acl_t acl);
#endif
