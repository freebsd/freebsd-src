/*
 * Header file mkisofs.h - assorted structure definitions and typecasts.

   Written by Eric Youngdale (1993).

   Copyright 1993 Yggdrasil Computing, Incorporated

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

/* ADD_FILES changes made by Ross Biro biro@yggdrasil.com 2/23/95 */

#include <sys/types.h>
#include <stdio.h>

/* This symbol is used to indicate that we do not have things like
   symlinks, devices, and so forth available.  Just files and dirs */

#ifdef VMS
#define NON_UNIXFS
#endif

#ifdef DJGPP
#define NON_UNIXFS
#endif

#ifdef VMS
#include <sys/dir.h>
#define dirent direct
#else
#include <dirent.h>
#endif

#include <string.h>
#include <sys/stat.h>

#ifdef linux
#include <sys/dir.h>
#endif

#ifdef ultrix
extern char *strdup();
#endif

#ifdef __STDC__
#define DECL(NAME,ARGS) NAME ARGS
#define FDECL1(NAME,TYPE0, ARG0) \
	NAME(TYPE0 ARG0)
#define FDECL2(NAME,TYPE0, ARG0,TYPE1, ARG1) \
	NAME(TYPE0 ARG0, TYPE1 ARG1)
#define FDECL3(NAME,TYPE0, ARG0,TYPE1, ARG1, TYPE2, ARG2) \
	NAME(TYPE0 ARG0, TYPE1 ARG1, TYPE2 ARG2)
#define FDECL4(NAME,TYPE0, ARG0,TYPE1, ARG1, TYPE2, ARG2, TYPE3, ARG3) \
	NAME(TYPE0 ARG0, TYPE1 ARG1, TYPE2 ARG2, TYPE3 ARG3)
#define FDECL5(NAME,TYPE0, ARG0,TYPE1, ARG1, TYPE2, ARG2, TYPE3, ARG3, TYPE4, ARG4) \
	NAME(TYPE0 ARG0, TYPE1 ARG1, TYPE2 ARG2, TYPE3 ARG3, TYPE4 ARG4)
#define FDECL6(NAME,TYPE0, ARG0,TYPE1, ARG1, TYPE2, ARG2, TYPE3, ARG3, TYPE4, ARG4, TYPE5, ARG5) \
	NAME(TYPE0 ARG0, TYPE1 ARG1, TYPE2 ARG2, TYPE3 ARG3, TYPE4 ARG4, TYPE5 ARG5)
#else
#define DECL(NAME,ARGS) NAME()
#define FDECL1(NAME,TYPE0, ARG0) NAME(ARG0) TYPE0 ARG0;
#define FDECL2(NAME,TYPE0, ARG0,TYPE1, ARG1) NAME(ARG0, ARG1) TYPE0 ARG0; TYPE1 ARG1;
#define FDECL3(NAME,TYPE0, ARG0,TYPE1, ARG1, TYPE2, ARG2) \
	NAME(ARG0, ARG1, ARG2) TYPE0 ARG0; TYPE1 ARG1; TYPE2 ARG2;
#define FDECL4(NAME,TYPE0, ARG0,TYPE1, ARG1, TYPE2, ARG2, TYPE3, ARG3) \
	NAME(ARG0, ARG1, ARG2, ARG3, ARG4) TYPE0 ARG0; TYPE1 ARG1; TYPE2 ARG2; TYPE3 ARG3;
#define FDECL5(NAME,TYPE0, ARG0,TYPE1, ARG1, TYPE2, ARG2, TYPE3, ARG3, TYPE4, ARG4) \
	NAME(ARG0, ARG1, ARG2, ARG3, ARG4) TYPE0 ARG0; TYPE1 ARG1; TYPE2 ARG2; TYPE3 ARG3; TYPE4 ARG4;
#define FDECL6(NAME,TYPE0, ARG0,TYPE1, ARG1, TYPE2, ARG2, TYPE3, ARG3, TYPE4, ARG4, TYPE5, ARG5) \
	NAME(ARG0, ARG1, ARG2, ARG3, ARG4, ARG5) TYPE0 ARG0; TYPE1 ARG1; TYPE2 ARG2; TYPE3 ARG3; TYPE4 ARG4; TYPE5 ARG5;
#define const
#endif


#ifdef __svr4__
#include <stdlib.h>
#else
extern int optind;
extern char *optarg;
/* extern int getopt (int __argc, char **__argv, char *__optstring); */
#endif

#include "iso9660.h"
#include "defaults.h"

struct directory_entry{
  struct directory_entry * next;
  struct iso_directory_record isorec;
  unsigned int starting_block;
  unsigned int size;
  unsigned int priority;
  char * name;
  char * table;
  char * whole_name;
  struct directory * filedir;
  struct directory_entry * parent_rec;
  unsigned int flags;
  ino_t inode;  /* Used in the hash table */
  dev_t dev;  /* Used in the hash table */
  unsigned char * rr_attributes;
  unsigned int rr_attr_size;
  unsigned int total_rr_attr_size;
};

struct file_hash{
  struct file_hash * next;
  ino_t inode;  /* Used in the hash table */
  dev_t dev;  /* Used in the hash table */
  unsigned int starting_block;
  unsigned int size;
};

struct directory{
  struct directory * next;  /* Next directory at same level as this one */
  struct directory * subdir; /* First subdirectory in this directory */
  struct directory * parent;
  struct directory_entry * contents;
  struct directory_entry * self;
  char * whole_name;  /* Entire path */
  char * de_name;  /* Entire path */
  unsigned int ce_bytes;  /* Number of bytes of CE entries reqd for this dir */
  unsigned int depth;
  unsigned int size;
  unsigned int extent;
  unsigned short path_index;
};

struct deferred{
  struct deferred * next;
  unsigned int starting_block;
  char * name;
  struct directory * filedir;
  unsigned int flags;
};

#ifdef ADD_FILES
struct file_adds {
  char *name;
  struct file_adds *child;
  struct file_adds *next;
  int add_count;
  int used;
  struct dirent de;
  struct {
    char *path;
    char *name;
  } *adds;
};
extern struct file_adds *root_file_adds;

#endif

extern void DECL(sort_n_finish,(struct directory *));
extern int goof;
extern struct directory * root;
extern struct directory * reloc_dir;
extern unsigned int next_extent;
extern unsigned int last_extent;
extern unsigned int path_table_size;
extern unsigned int path_table[4];
extern unsigned int path_blocks;
extern char * path_table_l;
extern char * path_table_m;
extern struct iso_directory_record root_record;

extern int use_RockRidge;
extern int follow_links;
extern int verbose;
extern int all_files;
extern int generate_tables;
extern int omit_period;
extern int omit_version_number;
extern int transparent_compression;
extern int RR_relocation_depth;
extern int full_iso9660_filenames;

extern int DECL(scan_directory_tree,(char * path, struct directory_entry * self));
extern void DECL(dump_tree,(struct directory * node));
extern void DECL(assign_directory_addresses,(struct directory * root));

extern int DECL(iso9660_file_length,(const char* name,
			       struct directory_entry * sresult, int flag));
extern int DECL(iso_write,(FILE * outfile));
extern void generate_path_tables();
extern void DECL(generate_iso9660_directories,(struct directory *, FILE*));
extern void DECL(generate_one_directory,(struct directory *, FILE*));
extern void generate_root_record();
extern int DECL(iso9660_date,(char *, time_t));
extern void DECL(add_hash,(struct directory_entry *));
extern struct file_hash * DECL(find_hash,(dev_t, ino_t));
extern void DECL(add_directory_hash,(dev_t, ino_t));
extern struct file_hash * DECL(find_directory_hash,(dev_t, ino_t));
extern void flush_file_hash();
extern int DECL(delete_file_hash,(struct directory_entry *));
extern struct directory_entry * DECL(find_file_hash,(char *));
extern void DECL(add_file_hash,(struct directory_entry *));
extern void finish_cl_pl_entries();
extern int DECL(get_733,(char *));

extern void DECL(set_723,(char *, unsigned int));
extern void DECL(set_733,(char *, unsigned int));
extern void DECL(sort_directory,(struct directory_entry **));
extern int DECL(generate_rock_ridge_attributes,(char *, char *,
					  struct directory_entry *,
					  struct stat *, struct stat *,
					  int  deep_flag));
extern char * DECL(generate_rr_extension_record,(char * id,  char  * descriptor,
				    char * source, int  * size));

extern char * extension_record;
extern int extension_record_extent;
extern int n_data_extents;

/* These are a few goodies that can be specified on the command line, and  are
   filled into the root record */

extern char * preparer;
extern char * publisher;
extern char * copyright;
extern char * biblio;
extern char * abstract;
extern char * appid;
extern char * volset_id;
extern char * system_id;
extern char * volume_id;

extern void * DECL(e_malloc,(size_t));


#define SECTOR_SIZE (2048)
#define ROUND_UP(X)    ((X + (SECTOR_SIZE - 1)) & ~(SECTOR_SIZE - 1))

#define NEED_RE 1
#define NEED_PL  2
#define NEED_CL 4
#define NEED_CE 8
#define NEED_SP 16

#define TABLE_INODE (sizeof(ino_t) >= 4 ? 0x7ffffffe : 0x7ffe)
#define UNCACHED_INODE (sizeof(ino_t) >= 4 ? 0x7fffffff : 0x7fff)
#define UNCACHED_DEVICE (sizeof(dev_t) >= 4 ? 0x7fffffff : 0x7fff)

#ifdef VMS
#define STAT_INODE(X) (X.st_ino[0])
#define PATH_SEPARATOR ']'
#define SPATH_SEPARATOR ""
#else
#define STAT_INODE(X) (X.st_ino)
#define PATH_SEPARATOR '/'
#define SPATH_SEPARATOR "/"
#endif

