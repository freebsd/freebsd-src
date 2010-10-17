/* The <sys/stat.h> header defines a struct that is used in the stat() and
 * fstat functions.  The information in this struct comes from the i-node of
 * some file.  These calls are the only approved way to inspect i-nodes.
 */

#ifndef _STAT_H
#define _STAT_H

#ifndef _TYPES_H		/* not quite right */
#include <sys/types.h>
#endif

struct stat {
  dev_t st_dev;			/* major/minor device number */
  ino_t st_ino;			/* i-node number */
  mode_t st_mode;		/* file mode, protection bits, etc. */
  short int st_nlink;		/* # links; TEMPORARY HACK: should be nlink_t*/
  uid_t st_uid;			/* uid of the file's owner */
  short int st_gid;		/* gid; TEMPORARY HACK: should be gid_t */
  dev_t st_rdev;
  off_t st_size;		/* file size */
  time_t st_atime;		/* time of last access */
  time_t st_mtime;		/* time of last data modification */
  time_t st_ctime;		/* time of last file status change */
};

/* Traditional mask definitions for st_mode. */
#define S_IFMT  0170000		/* type of file */
#define S_IFREG 0100000		/* regular */
#define S_IFBLK 0060000		/* block special */
#define S_IFDIR 0040000  	/* directory */
#define S_IFCHR 0020000		/* character special */
#define S_IFIFO 0010000		/* this is a FIFO */
#define S_ISUID 0004000		/* set user id on execution */
#define S_ISGID 0002000		/* set group id on execution */
				/* next is reserved for future use */
#define S_ISVTX   01000		/* save swapped text even after use */

/* POSIX masks for st_mode. */
#define S_IRWXU   00700		/* owner:  rwx------ */
#define S_IRUSR   00400		/* owner:  r-------- */
#define S_IWUSR   00200		/* owner:  -w------- */
#define S_IXUSR   00100		/* owner:  --x------ */

#define S_IRWXG   00070		/* group:  ---rwx--- */
#define S_IRGRP   00040		/* group:  ---r----- */
#define S_IWGRP   00020		/* group:  ----w---- */
#define S_IXGRP   00010		/* group:  -----x--- */

#define S_IRWXO   00007		/* others: ------rwx */
#define S_IROTH   00004		/* others: ------r-- */ 
#define S_IWOTH   00002		/* others: -------w- */
#define S_IXOTH   00001		/* others: --------x */

/* The following macros test st_mode (from POSIX Sec. 5.6.1.1. */
#define S_ISREG(m)	((m & S_IFMT) == S_IFREG)	/* is a reg file */
#define S_ISDIR(m)	((m & S_IFMT) == S_IFDIR)	/* is a directory */
#define S_ISCHR(m)	((m & S_IFMT) == S_IFCHR)	/* is a char spec */
#define S_ISBLK(m)	((m & S_IFMT) == S_IFBLK)	/* is a block spec */
#define S_ISFIFO(m)	((m & S_IFMT) == S_IFIFO)	/* is a pipe/FIFO */


/* Function Prototypes. */
#ifndef _ANSI_H
#include <ansi.h>
#endif

_PROTOTYPE( int chmod, (const char *_path, int _mode)			);
_PROTOTYPE( int fstat, (int _fildes, struct stat *_buf)			);
_PROTOTYPE( int mkdir, (const char *_path, int _mode)			);
_PROTOTYPE( int mkfifo, (const char *_path, int _mode)			);
_PROTOTYPE( int stat , (const char *_path, struct stat *_buf)		);
_PROTOTYPE( mode_t umask, (int _cmask)					);

#endif /* _STAT_H */
