/*
 * Sun RPC is a product of Sun Microsystems, Inc. and is provided for
 * unrestricted use provided that this legend is included on all tape
 * media and as a part of the software program in whole or part.  Users
 * may copy or modify Sun RPC without charge, but are not authorized
 * to license or distribute it to anyone else except as part of a product or
 * program developed by the user.
 * 
 * SUN RPC IS PROVIDED AS IS WITH NO WARRANTIES OF ANY KIND INCLUDING THE
 * WARRANTIES OF DESIGN, MERCHANTIBILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE, OR ARISING FROM A COURSE OF DEALING, USAGE OR TRADE PRACTICE.
 * 
 * Sun RPC is provided with no support and without any obligation on the
 * part of Sun Microsystems, Inc. to assist in its use, correction,
 * modification or enhancement.
 * 
 * SUN MICROSYSTEMS, INC. SHALL HAVE NO LIABILITY WITH RESPECT TO THE
 * INFRINGEMENT OF COPYRIGHTS, TRADE SECRETS OR ANY PATENTS BY SUN RPC
 * OR ANY PART THEREOF.
 * 
 * In no event will Sun Microsystems, Inc. be liable for any lost revenue
 * or profits or other special, indirect and consequential damages, even if
 * Sun has been advised of the possibility of such damages.
 * 
 * Sun Microsystems, Inc.
 * 2550 Garcia Avenue
 * Mountain View, California  94043
 */

/*
 * Protocol description for the mount program
 */

#ifndef RPC_HDR
%#ifndef lint
%/*static char sccsid[] = "from: @(#)mount.x 1.2 87/09/18 Copyr 1987 Sun Micro";*/
%/*static char sccsid[] = "from: @(#)mount.x	2.1 88/08/01 4.0 RPCSRC";*/
%static const char rcsid[] =
%  "$FreeBSD$";
%#endif /* not lint */
#endif

const MNTPATHLEN = 1024;	/* maximum bytes in a pathname argument */
const MNTNAMLEN = 255;		/* maximum bytes in a name argument */
const FHSIZE = 32;		/* size in bytes of a file handle */
#ifdef WANT_NFS3
const FHSIZE3 = 64;		/* size in bytes of a file handle (v3) */
#endif

/*
 * The fhandle is the file handle that the server passes to the client.
 * All file operations are done using the file handles to refer to a file
 * or a directory. The file handle can contain whatever information the
 * server needs to distinguish an individual file.
 */
typedef opaque fhandle[FHSIZE];	
#ifdef WANT_NFS3
typedef opaque fhandle3<FHSIZE3>;
#endif

/*
 * If a status of zero is returned, the call completed successfully, and 
 * a file handle for the directory follows. A non-zero status indicates
 * some sort of error. The status corresponds with UNIX error numbers.
 */
union fhstatus switch (unsigned fhs_status) {
case 0:
	fhandle fhs_fhandle;
default:
	void;
};

#ifdef WANT_NFS3
/*
 * Status codes returned by the version 3 mount call.
 */
enum mountstat3 {
	MNT3_OK = 0,                 /* no error */
	MNT3ERR_PERM = 1,            /* Not owner */
	MNT3ERR_NOENT = 2,           /* No such file or directory */
	MNT3ERR_IO = 5,              /* I/O error */
	MNT3ERR_ACCES = 13,          /* Permission denied */
	MNT3ERR_NOTDIR = 20,         /* Not a directory */
	MNT3ERR_INVAL = 22,          /* Invalid argument */
	MNT3ERR_NAMETOOLONG = 63,    /* Filename too long */
	MNT3ERR_NOTSUPP = 10004,     /* Operation not supported */
	MNT3ERR_SERVERFAULT = 10006  /* A failure on the server */
};

struct mountres3_ok {
	fhandle3	fhandle;
	int		auth_flavors<>;
};

union mountres3 switch (mountstat3 fhs_status) {
case 0:
	mountres3_ok	mountinfo;
default:
	void;
};
#endif

/*
 * The type dirpath is the pathname of a directory
 */
typedef string dirpath<MNTPATHLEN>;

/*
 * The type name is used for arbitrary names (hostnames, groupnames)
 */
typedef string name<MNTNAMLEN>;

/*
 * A list of who has what mounted
 */
typedef struct mountbody *mountlist;
struct mountbody {
	name ml_hostname;
	dirpath ml_directory;
	mountlist ml_next;
};

/*
 * A list of netgroups
 */
typedef struct groupnode *groups;
struct groupnode {
	name gr_name;
	groups gr_next;
};

/*
 * A list of what is exported and to whom
 */
typedef struct exportnode *exports;
struct exportnode {
	dirpath ex_dir;
	groups ex_groups;
	exports ex_next;
};

program MOUNTPROG {
	/*
	 * Version one of the mount protocol communicates with version two
	 * of the NFS protocol. Version three communicates with
	 * version three of the NFS protocol. The only connecting
	 * point is the fhandle structure, which is the same for both
	 * protocols.
	 */
	version MOUNTVERS {
		/*
		 * Does no work. It is made available in all RPC services
		 * to allow server reponse testing and timing
		 */
		void
		MOUNTPROC_NULL(void) = 0;

		/*	
		 * If fhs_status is 0, then fhs_fhandle contains the
	 	 * file handle for the directory. This file handle may
		 * be used in the NFS protocol. This procedure also adds
		 * a new entry to the mount list for this client mounting
		 * the directory.
		 * Unix authentication required.
		 */
		fhstatus 
		MOUNTPROC_MNT(dirpath) = 1;

		/*
		 * Returns the list of remotely mounted filesystems. The 
		 * mountlist contains one entry for each hostname and 
		 * directory pair.
		 */
		mountlist
		MOUNTPROC_DUMP(void) = 2;

		/*
		 * Removes the mount list entry for the directory
		 * Unix authentication required.
		 */
		void
		MOUNTPROC_UMNT(dirpath) = 3;

		/*
		 * Removes all of the mount list entries for this client
		 * Unix authentication required.
		 */
		void
		MOUNTPROC_UMNTALL(void) = 4;

		/*
		 * Returns a list of all the exported filesystems, and which
		 * machines are allowed to import it.
		 */
		exports
		MOUNTPROC_EXPORT(void)  = 5;

		/*
		 * Identical to MOUNTPROC_EXPORT above
		 */
		exports
		MOUNTPROC_EXPORTALL(void) = 6;
	} = 1;
#ifdef WANT_NFS3
	version MOUNTVERS3 {
		/*
		 * Does no work. It is made available in all RPC services
		 * to allow server reponse testing and timing
		 */
		void
		MOUNTPROC_NULL(void) = 0;

		/*
		 * If mountres3.fhs_status is MNT3_OK, then
		 * mountres3.mountinfo contains the file handle for
		 * the directory and a list of acceptable
		 * authentication flavors.  This file handle may only
		 * be used in the NFS version 3 protocol.  This
		 * procedure also results in the server adding a new
		 * entry to its mount list recording that this client
		 * has mounted the directory. AUTH_UNIX authentication
		 * or better is required.
		 */
		mountres3
		MOUNTPROC_MNT(dirpath) = 1;

		/*
		 * Returns the list of remotely mounted filesystems. The 
		 * mountlist contains one entry for each hostname and 
		 * directory pair.
		 */
		mountlist
		MOUNTPROC_DUMP(void) = 2;

		/*
		 * Removes the mount list entry for the directory
		 * Unix authentication required.
		 */
		void
		MOUNTPROC_UMNT(dirpath) = 3;

		/*
		 * Removes all of the mount list entries for this client
		 * Unix authentication required.
		 */
		void
		MOUNTPROC_UMNTALL(void) = 4;

		/*
		 * Returns a list of all the exported filesystems, and which
		 * machines are allowed to import it.
		 */
		exports
		MOUNTPROC_EXPORT(void)  = 5;
	} = 3;
#endif
} = 100005;
