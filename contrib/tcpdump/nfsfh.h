/*
 * $Header: /tcpdump/master/tcpdump/nfsfh.h,v 1.9 2000/06/01 01:16:36 assar Exp $
 *
 * nfsfh.h - NFS file handle definitions (for portable use)
 *
 * Jeffrey C. Mogul
 * Digital Equipment Corporation
 * Western Research Laboratory
 *	$NetBSD: nfsfh.h,v 1.1.1.2 1997/10/03 17:25:13 christos Exp $	*/

/*
 * Internal representation of dev_t, because different NFS servers
 * that we might be spying upon use different external representations.
 */
typedef struct {
	u_int32_t Minor;	/* upper case to avoid clashing with macro names */
	u_int32_t Major;
} my_devt;

#define	dev_eq(a,b)	((a.Minor == b.Minor) && (a.Major == b.Major))

/*
 * Many file servers now use a large file system ID.  This is
 * our internal representation of that.
 */
typedef	struct {
	my_devt	Fsid_dev;		/* XXX avoid name conflict with AIX */
	char Opaque_Handle[2 * 32 + 1];
	u_int32_t fsid_code;
} my_fsid;

#define	fsid_eq(a,b)	((a.fsid_code == b.fsid_code) &&\
			 dev_eq(a.Fsid_dev, b.Fsid_dev))

extern void Parse_fh(caddr_t *, int, my_fsid *, ino_t *, char **, char **, int);
