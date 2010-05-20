/* extern.h - External declarations for cpio.  Requires system.h.
   Copyright (C) 1990, 1991, 1992, 2001, 2006 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public
   License along with this program; if not, write to the Free
   Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301 USA.  */

#include "paxlib.h"
#include "quotearg.h"
#include "quote.h"

enum archive_format
{
  arf_unknown, arf_binary, arf_oldascii, arf_newascii, arf_crcascii,
  arf_tar, arf_ustar, arf_hpoldascii, arf_hpbinary
};

extern enum archive_format archive_format;
extern int reset_time_flag;
extern int io_block_size;
extern int create_dir_flag;
extern int rename_flag;
extern char *rename_batch_file;
extern int table_flag;
extern int unconditional_flag;
extern int verbose_flag;
extern int dot_flag;
extern int link_flag;
extern int retain_time_flag;
extern int crc_i_flag;
extern int append_flag;
extern int swap_bytes_flag;
extern int swap_halfwords_flag;
extern int swapping_bytes;
extern int swapping_halfwords;
extern int set_owner_flag;
extern uid_t set_owner;
extern int set_group_flag;
extern gid_t set_group;
extern int no_chown_flag;
extern int sparse_flag;
extern int quiet_flag;
extern int only_verify_crc_flag;
extern int abs_paths_flag;
extern unsigned int warn_option;

/* Values for warn_option */
#define CPIO_WARN_NONE     0
#define CPIO_WARN_TRUNCATE 0x01
#define CPIO_WARN_ALL      (unsigned int)-1

extern bool to_stdout_option;

extern int last_header_start;
extern int copy_matching_files;
extern int numeric_uid;
extern char *pattern_file_name;
extern char *new_media_message;
extern char *new_media_message_with_number;
extern char *new_media_message_after_number;
extern int archive_des;
extern char *archive_name;
extern char *rsh_command_option;
extern unsigned int crc;
extern int delayed_seek_count;
#ifdef DEBUG_CPIO
extern int debug_flag;
#endif

extern char *input_buffer, *output_buffer;
extern char *in_buff, *out_buff;
extern long input_buffer_size;
extern long input_size, output_size;
#ifdef __GNUC__
extern long long input_bytes, output_bytes;
#else
extern long input_bytes, output_bytes;
#endif
extern char *directory_name;
extern char **save_patterns;
extern int num_patterns;
extern char name_end;
extern char input_is_special;
extern char output_is_special;
extern char input_is_seekable;
extern char output_is_seekable;
extern char *program_name;
extern int (*xstat) ();
extern void (*copy_function) ();


/* copyin.c */
void warn_junk_bytes (long bytes_skipped);
/* FIXME: make read_* static in copyin.c */
void read_in_header (struct cpio_file_stat *file_hdr, int in_des);
void read_in_old_ascii (struct cpio_file_stat *file_hdr, int in_des);
void read_in_new_ascii (struct cpio_file_stat *file_hdr, int in_des);
void read_in_binary (struct cpio_file_stat *file_hdr,
		     struct old_cpio_header *short_hdr, int in_des);
void swab_array (char *arg, int count);
void process_copy_in (void);
void long_format (struct cpio_file_stat *file_hdr, char *link_name);
void print_name_with_quoting (char *p);

/* copyout.c */
int write_out_header (struct cpio_file_stat *file_hdr, int out_des);
void process_copy_out (void);

/* copypass.c */
void process_copy_pass (void);
int link_to_maj_min_ino (char *file_name, int st_dev_maj, 
			 int st_dev_min, int st_ino);
int link_to_name (char *link_name, char *link_target);

/* dirname.c */
char *dirname (char *path);

/* filemode.c */
void mode_string (unsigned int mode, char *str);

/* idcache.c */
#ifndef __MSDOS__
char *getgroup ();
char *getuser ();
uid_t *getuidbyname ();
gid_t *getgidbyname ();
#endif

/* main.c */
void process_args (int argc, char *argv[]);
void initialize_buffers (void);

/* makepath.c */
int make_path (char *argpath, int mode, int parent_mode,
	       uid_t owner, gid_t group, char *verbose_fmt_string);

/* tar.c */
void write_out_tar_header (struct cpio_file_stat *file_hdr, int out_des);
int null_block (long *block, int size);
void read_in_tar_header (struct cpio_file_stat *file_hdr, int in_des);
int otoa (char *s, unsigned long *n);
int is_tar_header (char *buf);
int is_tar_filename_too_long (char *name);

/* userspec.c */
#ifndef __MSDOS__
char *parse_user_spec (char *name, uid_t *uid, gid_t *gid,
		       char **username, char **groupname);
#endif

/* util.c */
void tape_empty_output_buffer (int out_des);
void disk_empty_output_buffer (int out_des);
void swahw_array (char *ptr, int count);
void tape_buffered_write (char *in_buf, int out_des, off_t num_bytes);
void tape_buffered_read (char *in_buf, int in_des, off_t num_bytes);
int tape_buffered_peek (char *peek_buf, int in_des, int num_bytes);
void tape_toss_input (int in_des, off_t num_bytes);
void copy_files_tape_to_disk (int in_des, int out_des, off_t num_bytes);
void copy_files_disk_to_tape (int in_des, int out_des, off_t num_bytes, char *filename);
void copy_files_disk_to_disk (int in_des, int out_des, off_t num_bytes, char *filename);
void warn_if_file_changed (char *file_name, unsigned long old_file_size,
                           off_t old_file_mtime);
void create_all_directories (char *name);
void prepare_append (int out_file_des);
char *find_inode_file (unsigned long node_num,
		       unsigned long major_num, unsigned long minor_num);
void add_inode (unsigned long node_num, char *file_name,
	        unsigned long major_num, unsigned long minor_num);
int open_archive (char *file);
void tape_offline (int tape_des);
void get_next_reel (int tape_des);
void set_new_media_message (char *message);
#if defined(__MSDOS__) && !defined(__GNUC__)
int chown (char *path, int owner, int group);
#endif
#ifdef __TURBOC__
int utime (char *filename, struct utimbuf *utb);
#endif
#ifdef HPUX_CDF
char *add_cdf_double_slashes (char *filename);
#endif
void write_nuls_to_file (off_t num_bytes, int out_des, 
			 void (*writer) (char *in_buf,
					 int out_des, off_t num_bytes));
#define DISK_IO_BLOCK_SIZE	512

/* FIXME: Move to system.h? */
#ifndef SYMLINK_USES_UMASK
# define UMASKED_SYMLINK(name1,name2,mode)    symlink(name1,name2)
#else
# define UMASKED_SYMLINK(name1,name2,mode)    umasked_symlink(name1,name2,mode)
#endif /* SYMLINK_USES_UMASK */

void set_perms (int fd, struct cpio_file_stat *header);
void set_file_times (int fd, const char *name, unsigned long atime,
		     unsigned long mtime);
void stat_to_cpio (struct cpio_file_stat *hdr, struct stat *st);
void cpio_safer_name_suffix (char *name, bool link_target,
			     bool absolute_names, bool strip_leading_dots);

/* FIXME: These two defines should be defined in paxutils */
#define LG_8  3
#define LG_16 4

uintmax_t from_ascii (char const *where, size_t digs, unsigned logbase);

#define FROM_OCTAL(f) from_ascii (f, sizeof f, LG_8)
#define FROM_HEX(f) from_ascii (f, sizeof f, LG_16)
	    
