/* archive file definition for GNU software */

/* So far this is correct for BSDish archives.  Don't forget that
   files must begin on an even byte boundary. */

#ifndef __GNU_AR_H__
#define __GNU_AR_H__

#define ARMAG  "!<arch>\n"	/* For COFF and a.out archives */
#define ARMAGB "!<bout>\n"	/* For b.out archives */
#define SARMAG 8
#define ARFMAG "`\n"

/* The ar_date field of the armap (__.SYMDEF) member of an archive
   must be greater than the modified date of the entire file, or 
   BSD-derived linkers complain.  We originally write the ar_date with
   this offset from the real file's mod-time.  After finishing the
   file, we rewrite ar_date if it's not still greater than the mod date.  */

#define ARMAP_TIME_OFFSET       60

struct ar_hdr {
  char ar_name[16];		/* name of this member */
  char ar_date[12];		/* file mtime */
  char ar_uid[6];		/* owner uid; printed as decimal */
  char ar_gid[6];		/* owner gid; printed as decimal */
  char ar_mode[8];		/* file mode, printed as octal   */
  char ar_size[10];		/* file size, printed as decimal */
  char ar_fmag[2];		/* should contain ARFMAG */
};

#endif /* __GNU_AR_H__ */
