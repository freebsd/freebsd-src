/*
 * aux_conf.h:
 * This file gets "filled in" for each architecture.
 * Generated automatically from aux_conf.h.in by configure.
 */

#ifndef _AUX_CONF_H
#define _AUX_CONF_H

/*
 * The next line is a literal inclusion of a file which includes a
 * definition for the MOUNT_TRAP macro for a particular architecture.
 * If it defines the wrong entry, check the AC_CHECK_MOUNT_TRAP m4 macro
 * in $srcdir/aux/macros.
 */

/* $srcdir/conf/trap/trap_default.h */
#ifdef COMMENT_GET_DIRECTLY_FROM_FILE
#define MOUNT_TRAP(type, mnt, flags, mnt_data) mount(type, mnt->mnt_dir, flags, mnt_data)
#endif	/* COMMENT_GET_DIRECTLY_FROM_FILE */
#include "conf/trap/trap_default.h"
/* End of included MOUNT_TRAP macro definition file */


/*
 * The next line is a literal replacement of a variable which defines the
 * the UNMOUNT_TRAP macro for a particular architecture.
 * If it defines the wrong entry, check the AC_CHECK_UNMOUNT_CALL m4 macro
 * in $srcdir/aclocal.m4.  If the arguments are being defined wrong, check
 * the macro AC_CHECK_UNMOUNT_ARGS in $srcdir/aux/macros.
 */
#define UNMOUNT_TRAP(mnt)	unmount(mnt->mnt_dir)
/* End of replaced UNMOUNT_TRAP macro definition */

/*
 * The next line is a literal inclusion of a file which includes a
 * definition for the NFS_FH_DREF macro for a particular architecture.
 * If it defines the wrong entry, check the AC_CHECK_NFS_FH_DREF m4 macro
 * in $srcdir/aux/macros.
 */

/* $srcdir/conf/fh_dref/fh_dref_freebsd22.h */
#ifdef COMMENT_GET_DIRECTLY_FROM_FILE
#define	NFS_FH_DREF(dst, src) (dst) = (u_char *) (src)
#endif	/* COMMENT_GET_DIRECTLY_FROM_FILE */
#include "conf/fh_dref/fh_dref_freebsd22.h"
/* End of included NFS_FH_DREF macro definition file */

/*
 * The next line is a literal inclusion of a file which includes a
 * definition for the NFS_SA_DREF macro for a particular architecture.
 * If it defines the wrong entry, check the AC_CHECK_NFS_SA_DREF m4 macro
 * in $srcdir/aux/macros.
 */

/* $srcdir/conf/sa_dref/sa_dref_bsd44.h */
#ifdef COMMENT_GET_DIRECTLY_FROM_FILE
#define	NFS_SA_DREF(dst, src) { \
		(dst)->addr = (struct sockaddr *) (src); \
		(dst)->addrlen = sizeof(*src); \
	}
#endif	/* COMMENT_GET_DIRECTLY_FROM_FILE */
#include "conf/sa_dref/sa_dref_bsd44.h"
/* End of included NFS_SA_DREF macro definition file */

/*
 * The next line is a literal inclusion of a file which includes a
 * definition for the NFS_HN_DREF macro for a particular architecture.
 * If it defines the wrong entry, check the AC_CHECK_NFS_HN_DREF m4 macro
 * in $srcdir/aux/macros.
 */

/* $srcdir/conf/hn_dref/hn_dref_default.h */
#ifdef COMMENT_GET_DIRECTLY_FROM_FILE
#define NFS_HN_DREF(dst, src) (dst) = (src)
#endif	/* COMMENT_GET_DIRECTLY_FROM_FILE */
#include "conf/hn_dref/hn_dref_default.h"
/* End of included NFS_HN_DREF macro definition file */

#endif /* not _AUX_CONF_H */
