#ifndef FSTYP_H
#define	FSTYP_H

#include <stdbool.h>

#define	EXFAT_ENC	"UCS-2LE"
/*
 * NTFS itself is agnostic to encoding; it just stores 255 u16 wchars.  In
 * practice, UTF-16 seems expected for NTFS.  (Maybe also for exFAT.)
 */
#define	NTFS_ENC	"UTF-16LE"

typedef int (*fstyp_function)(FILE *, char *, size_t);

struct fstype {
	const char	    *name;
	fstyp_function	function;
	bool		    unmountable;
	char		    *precache_encoding;
};

void enable_encodings();
int fstypef(FILE *, char *, size_t, bool show_unmountable, const struct fstype **result);

#endif /* !FSTYP_H */