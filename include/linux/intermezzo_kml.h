#ifndef __INTERMEZZO_KML_H
#define __INTERMEZZO_KML_H

#include <linux/version.h>
#include <linux/intermezzo_psdev.h>
#include <linux/fs.h>
#include <linux/intermezzo_journal.h>

#define PRESTO_KML_MAJOR_VERSION 0x00010000
#define PRESTO_KML_MINOR_VERSION 0x00002001
#define PRESTO_OP_NOOP          0
#define PRESTO_OP_CREATE        1
#define PRESTO_OP_MKDIR         2
#define PRESTO_OP_UNLINK        3
#define PRESTO_OP_RMDIR         4
#define PRESTO_OP_CLOSE         5
#define PRESTO_OP_SYMLINK       6
#define PRESTO_OP_RENAME        7
#define PRESTO_OP_SETATTR       8
#define PRESTO_OP_LINK          9
#define PRESTO_OP_OPEN          10
#define PRESTO_OP_MKNOD         11
#define PRESTO_OP_WRITE         12
#define PRESTO_OP_RELEASE       13
#define PRESTO_OP_TRUNC         14
#define PRESTO_OP_SETEXTATTR    15
#define PRESTO_OP_DELEXTATTR    16

#define PRESTO_LML_DONE     	1 /* flag to get first write to do LML */
#define KML_KOP_MARK            0xffff

struct presto_lml_data {
        loff_t   rec_offset;
};

struct big_journal_prefix {
        u32 len;
        u32 version; 
        u32 pid;
        u32 uid;
        u32 fsuid;
        u32 fsgid;
        u32 opcode;
        u32 ngroups;
        u32 groups[NGROUPS_MAX];
};

enum kml_opcode {
        KML_CREATE = 1,
        KML_MKDIR,
        KML_UNLINK,
        KML_RMDIR,
        KML_CLOSE,
        KML_SYMLINK,
        KML_RENAME,
        KML_SETATTR,
        KML_LINK,
        KML_OPEN,
        KML_MKNOD,
        KML_ENDMARK = 0xff
};

struct kml_create {
	char 			*path;
	struct presto_version 	new_objectv, 
				old_parentv, 
				new_parentv;
	int 			mode;
	int 			uid;
	int 			gid;
};

struct kml_open {
};

struct kml_mkdir {
	char 			*path;
	struct presto_version 	new_objectv, 
				old_parentv, 
				new_parentv;
	int 			mode;
	int 			uid;
	int 			gid;
};

struct kml_unlink {
	char 			*path, 	
				*name;
	struct presto_version 	old_tgtv, 
				old_parentv, 
				new_parentv;
};

struct kml_rmdir {
	char 			*path, 
				*name;
	struct presto_version 	old_tgtv, 
				old_parentv, 
				new_parentv;
};

struct kml_close {
	int 			open_mode, 
				open_uid, 
				open_gid;
	char 			*path;
	struct presto_version 	new_objectv;
	__u64 			ino;
      	int 			generation;
};

struct kml_symlink {
	char 			*sourcepath, 	
				*targetpath;
	struct presto_version 	new_objectv, 
				old_parentv, 
				new_parentv;
      	int 			uid;
	int 			gid;
};

struct kml_rename {
	char 			*sourcepath, 
				*targetpath;
	struct presto_version 	old_objectv, 
				new_objectv, 
				old_tgtv, 
				new_tgtv;
};

struct kml_setattr {
	char 			*path;
	struct presto_version 	old_objectv;
	struct iattr 		iattr;
};

struct kml_link {
	char 			*sourcepath, 	
				*targetpath;
	struct presto_version 	new_objectv, 
				old_parentv, 
				new_parentv;
};

struct kml_mknod {
	char 			*path;
	struct presto_version 	new_objectv, 
				old_parentv, 
				new_parentv;
	int 			mode;
      	int 			uid;
	int 			gid;
       	int 			major;
	int 			minor;
};

/* kml record items for optimizing */
struct kml_kop_node
{
        u32             kml_recno;
        u32             kml_flag;
        u32             kml_op;
        nlink_t         i_nlink;
        u32             i_ino;
};

struct kml_kop_lnode
{
        struct list_head chains;
        struct kml_kop_node node;
};

struct kml_endmark {
	u32			total;
	struct kml_kop_node 	*kop;
};

/* kml_flag */
#define  KML_REC_DELETE               1
#define  KML_REC_EXIST                0

struct kml_optimize {
	struct list_head kml_chains;
        u32              kml_flag;
        u32              kml_op;
        nlink_t          i_nlink;
        u32              i_ino;
};

struct kml_rec {
	/* attribute of this record */
	int 				rec_size;
        int     			rec_kml_offset;

	struct 	big_journal_prefix 	rec_head;
	union {
		struct kml_create 	create;
		struct kml_open 	open;
		struct kml_mkdir 	mkdir;
		struct kml_unlink 	unlink;
		struct kml_rmdir 	rmdir;
		struct kml_close 	close;
		struct kml_symlink 	symlink;
		struct kml_rename 	rename;
		struct kml_setattr 	setattr;
		struct kml_mknod 	mknod;
		struct kml_link 	link;
		struct kml_endmark      endmark;
	} rec_kml;
        struct 	journal_suffix 		rec_tail;

        /* for kml optimize only */
        struct  kml_optimize kml_optimize;
};

/* kml record items for optimizing */
extern void kml_kop_init (struct presto_file_set *fset);
extern void kml_kop_addrec (struct presto_file_set *fset, 
		struct inode *ino, u32 op, u32 flag);
extern int  kml_kop_flush (struct presto_file_set *fset);

/* defined in kml_setup.c */
extern int kml_init (struct presto_file_set *fset);
extern int kml_cleanup (struct presto_file_set *fset);

/* defined in kml.c */
extern int begin_kml_reint (struct file *file, unsigned long arg);
extern int do_kml_reint (struct file *file, unsigned long arg);
extern int end_kml_reint (struct file *file, unsigned long arg);

/* kml_utils.c */
extern char *dlogit (void *tbuf, const void *sbuf, int size);
extern char * bdup_printf (char *format, ...);

/* defined in kml_decode.c */
/* printop */
#define  PRINT_KML_PREFIX             0x1
#define  PRINT_KML_SUFFIX             0x2
#define  PRINT_KML_REC                0x4
#define  PRINT_KML_OPTIMIZE           0x8
#define  PRINT_KML_EXIST              0x10
#define  PRINT_KML_DELETE             0x20
extern void   kml_printrec (struct kml_rec *rec, int printop);
extern int    print_allkmlrec (struct list_head *head, int printop);
extern int    delete_kmlrec (struct list_head *head);
extern int    kml_decoderec (char *buf, int pos, int buflen, int *size,
	                     struct kml_rec **newrec);
extern int decode_kmlrec (struct list_head *head, char *kml_buf, int buflen);
extern void kml_freerec (struct kml_rec *rec);

/* defined in kml_reint.c */
#define KML_CLOSE_BACKFETCH            1
extern int kml_reintbuf (struct  kml_fsdata *kml_fsdata,
                  	char *mtpt, struct kml_rec **rec);

/* defined in kml_setup.c */
extern int kml_init (struct presto_file_set *fset);
extern int kml_cleanup (struct presto_file_set *fset);

#endif

