/* Common declarations for the tar program.

   Copyright 1988, 1992, 1993, 1994, 1996, 1997, 1999, 2000, 2001 Free
   Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the
   Free Software Foundation; either version 2, or (at your option) any later
   version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General
   Public License for more details.

   You should have received a copy of the GNU General Public License along
   with this program; if not, write to the Free Software Foundation, Inc.,
   59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

/* $FreeBSD$ */

/* Declare the GNU tar archive format.  */
#include "tar.h"

/* The checksum field is filled with this while the checksum is computed.  */
#define CHKBLANKS	"        "	/* 8 blanks, no null */

/* Some constants from POSIX are given names.  */
#define NAME_FIELD_SIZE   100
#define PREFIX_FIELD_SIZE 155
#define UNAME_FIELD_SIZE   32
#define GNAME_FIELD_SIZE   32

/* Some various global definitions.  */

/* Name of file to use for interacting with user.  */
#if MSDOS
# define TTY_NAME "con"
#else
# define TTY_NAME "/dev/tty"
#endif

/* GLOBAL is defined to empty in tar.c only, and left alone in other *.c
   modules.  Here, we merely set it to "extern" if it is not already set.
   GNU tar does depend on the system loader to preset all GLOBAL variables to
   neutral (or zero) values, explicit initialization is usually not done.  */
#ifndef GLOBAL
# define GLOBAL extern
#endif

/* Exit status for GNU tar.  Let's try to keep this list as simple as
   possible.  -d option strongly invites a status different for unequal
   comparison and other errors.  */
GLOBAL int exit_status;

#define TAREXIT_SUCCESS 0
#define TAREXIT_DIFFERS 1
#define TAREXIT_FAILURE 2

/* Both WARN and ERROR write a message on stderr and continue processing,
   however ERROR manages so tar will exit unsuccessfully.  FATAL_ERROR
   writes a message on stderr and aborts immediately, with another message
   line telling so.  USAGE_ERROR works like FATAL_ERROR except that the
   other message line suggests trying --help.  All four macros accept a
   single argument of the form ((0, errno, _("FORMAT"), Args...)).  errno
   is zero when the error is not being detected by the system.  */

#define WARN(Args) \
  error Args
#define ERROR(Args) \
  (error Args, exit_status = TAREXIT_FAILURE)
#define FATAL_ERROR(Args) \
  (error Args, fatal_exit ())
#define USAGE_ERROR(Args) \
  (error Args, usage (TAREXIT_FAILURE))

/* Information gleaned from the command line.  */

#include "arith.h"
#include <backupfile.h>
#include <exclude.h>
#include <modechange.h>
#include <safe-read.h>
#include <full-write.h>

/* Log base 2 of common values.  */
#define LG_8 3
#define LG_64 6
#define LG_256 8

/* Name of this program.  */
GLOBAL const char *program_name;

/* Main command option.  */

enum subcommand
{
  UNKNOWN_SUBCOMMAND,		/* none of the following */
  APPEND_SUBCOMMAND,		/* -r */
  CAT_SUBCOMMAND,		/* -A */
  CREATE_SUBCOMMAND,		/* -c */
  DELETE_SUBCOMMAND,		/* -D */
  DIFF_SUBCOMMAND,		/* -d */
  EXTRACT_SUBCOMMAND,		/* -x */
  LIST_SUBCOMMAND,		/* -t */
  UPDATE_SUBCOMMAND		/* -u */
};

GLOBAL enum subcommand subcommand_option;

/* Selected format for output archive.  */
GLOBAL enum archive_format archive_format;

/* Either NL or NUL, as decided by the --null option.  */
GLOBAL char filename_terminator;

/* Size of each record, once in blocks, once in bytes.  Those two variables
   are always related, the second being BLOCKSIZE times the first.  They do
   not have _option in their name, even if their values is derived from
   option decoding, as these are especially important in tar.  */
GLOBAL int blocking_factor;
GLOBAL size_t record_size;

/* Boolean value.  */
GLOBAL int absolute_names_option;

/* Allow GNUTYPE_NAMES type? */
GLOBAL bool allow_name_mangling_option;

/* This variable tells how to interpret newer_mtime_option, below.  If zero,
   files get archived if their mtime is not less than newer_mtime_option.
   If nonzero, files get archived if *either* their ctime or mtime is not less
   than newer_mtime_option.  */
GLOBAL int after_date_option;

/* Boolean value.  */
GLOBAL int atime_preserve_option;

/* Boolean value.  */
GLOBAL int backup_option;

/* Type of backups being made.  */
GLOBAL enum backup_type backup_type;

/* Boolean value.  */
GLOBAL int block_number_option;

/* Boolean value.  */
GLOBAL int checkpoint_option;

/* Specified name of compression program, or "gzip" as implied by -z.  */
GLOBAL const char *use_compress_program_option;

/* Boolean value.  */
GLOBAL int dereference_option;

/* Patterns that match file names to be excluded.  */
GLOBAL struct exclude *excluded;

/* Boolean value.  */
GLOBAL int fast_read_option;

/* Specified file containing names to work on.  */
GLOBAL const char *files_from_option;

/* Boolean value.  */
GLOBAL int force_local_option;

/* Specified value to be put into tar file in place of stat () results, or
   just -1 if such an override should not take place.  */
GLOBAL gid_t group_option;

/* Boolean value.  */
GLOBAL int ignore_failed_read_option;

/* Boolean value.  */
GLOBAL int ignore_zeros_option;

/* Boolean value.  */
GLOBAL int incremental_option;

/* Specified name of script to run at end of each tape change.  */
GLOBAL const char *info_script_option;

/* Boolean value.  */
GLOBAL int interactive_option;

enum old_files
{
  DEFAULT_OLD_FILES, /* default */
  UNLINK_FIRST_OLD_FILES, /* --unlink-first */
  KEEP_OLD_FILES, /* --keep-old-files */
  OVERWRITE_OLD_DIRS, /* --overwrite-dir */
  OVERWRITE_OLD_FILES /* --overwrite */
};
GLOBAL enum old_files old_files_option;

/* Specified file name for incremental list.  */
GLOBAL const char *listed_incremental_option;

/* Specified mode change string.  */
GLOBAL struct mode_change *mode_option;

/* Boolean value.  */
GLOBAL int multi_volume_option;

/* Boolean value.  */
GLOBAL int namelist_freed;

/* The same variable hold the time, whether mtime or ctime.  Just fake a
   non-existing option, for making the code clearer, elsewhere.  */
#define newer_ctime_option newer_mtime_option

/* Specified threshold date and time.  Files having an older time stamp
   do not get archived (also see after_date_option above).  */
GLOBAL time_t newer_mtime_option;

/* Zero if there is no recursion, otherwise FNM_LEADING_DIR.  */
GLOBAL int recursion_option;

/* Boolean value.  */
GLOBAL int numeric_owner_option;

/* Boolean value.  */
GLOBAL int one_file_system_option;

/* Specified value to be put into tar file in place of stat () results, or
   just -1 if such an override should not take place.  */
GLOBAL uid_t owner_option;

/* Boolean value.  */
GLOBAL int recursive_unlink_option;

/* Boolean value.  */
GLOBAL int read_full_records_option;

/* Boolean value.  */
GLOBAL int remove_files_option;

/* Specified remote shell command.  */
GLOBAL const char *rsh_command_option;

/* Boolean value.  */
GLOBAL int same_order_option;

/* If positive, preserve ownership when extracting.  */
GLOBAL int same_owner_option;

/* If positive, preserve permissions when extracting.  */
GLOBAL int same_permissions_option;

/* Boolean value.  */
GLOBAL int show_omitted_dirs_option;

/* Boolean value.  */
GLOBAL int sparse_option;

/* Boolean value.  */
GLOBAL int starting_file_option;

/* Specified maximum byte length of each tape volume (multiple of 1024).  */
GLOBAL tarlong tape_length_option;

/* Boolean value.  */
GLOBAL int to_stdout_option;

/* Boolean value.  */
GLOBAL int totals_option;

/* Boolean value.  */
GLOBAL int touch_option;
 
/* Count how many times the option has been set, multiple setting yields
   more verbose behavior.  Value 0 means no verbosity, 1 means file name
   only, 2 means file name and all attributes.  More than 2 is just like 2.  */
GLOBAL int verbose_option;

/* Boolean value.  */
GLOBAL int verify_option;

/* Specified name of file containing the volume number.  */
GLOBAL const char *volno_file_option;

/* Specified value or pattern.  */
GLOBAL const char *volume_label_option;

/* Other global variables.  */

/* File descriptor for archive file.  */
GLOBAL int archive;

/* Nonzero when outputting to /dev/null.  */
GLOBAL int dev_null_output;

/* Timestamp for when we started execution.  */
#if HAVE_CLOCK_GETTIME
  GLOBAL struct timespec start_timespec;
# define start_time (start_timespec.tv_sec)
#else
  GLOBAL time_t start_time;
#endif

/* Name of file for the current archive entry.  */
GLOBAL char *current_file_name;

/* Name of link for the current archive entry.  */
GLOBAL char *current_link_name;

/* List of tape drive names, number of such tape drives, allocated number,
   and current cursor in list.  */
GLOBAL const char **archive_name_array;
GLOBAL int archive_names;
GLOBAL int allocated_archive_names;
GLOBAL const char **archive_name_cursor;

/* Structure for keeping track of filenames and lists thereof.  */
struct name
  {
    struct name *next;
    size_t length;		/* cached strlen(name) */
    char found;			/* a matching file has been found */
    char firstch;		/* first char is literally matched */
    char regexp;		/* this name is a regexp, not literal */
    int change_dir;		/* set with the -C option */
    char const *dir_contents;	/* for incremental_option */
    char fake;			/* dummy entry */
    char name[1];
  };

/* Information about a sparse file.  */
struct sp_array
  {
    off_t offset;
    size_t numbytes;
  };
GLOBAL struct sp_array *sparsearray;

/* Number of elements in sparsearray.  */
GLOBAL int sp_array_size;

/* Declarations for each module.  */

/* FIXME: compare.c should not directly handle the following variable,
   instead, this should be done in buffer.c only.  */

enum access_mode
{
  ACCESS_READ,
  ACCESS_WRITE,
  ACCESS_UPDATE
};
extern enum access_mode access_mode;

/* Module buffer.c.  */

extern FILE *stdlis;
extern char *save_name;
extern off_t save_sizeleft;
extern off_t save_totsize;
extern bool write_archive_to_stdout;

size_t available_space_after PARAMS ((union block *));
off_t current_block_ordinal PARAMS ((void));
void close_archive PARAMS ((void));
void closeout_volume_number PARAMS ((void));
union block *find_next_block PARAMS ((void));
void flush_read PARAMS ((void));
void flush_write PARAMS ((void));
void flush_archive PARAMS ((void));
void init_volume_number PARAMS ((void));
void open_archive PARAMS ((enum access_mode));
void print_total_written PARAMS ((void));
void reset_eof PARAMS ((void));
void set_next_block_after PARAMS ((union block *));

/* Module create.c.  */

void create_archive PARAMS ((void));
void dump_file PARAMS ((char *, int, dev_t));
void finish_header PARAMS ((union block *));
void write_eot PARAMS ((void));

#define GID_TO_CHARS(val, where) gid_to_chars (val, where, sizeof (where))
#define MAJOR_TO_CHARS(val, where) major_to_chars (val, where, sizeof (where))
#define MINOR_TO_CHARS(val, where) minor_to_chars (val, where, sizeof (where))
#define MODE_TO_CHARS(val, where) mode_to_chars (val, where, sizeof (where))
#define OFF_TO_CHARS(val, where) off_to_chars (val, where, sizeof (where))
#define SIZE_TO_CHARS(val, where) size_to_chars (val, where, sizeof (where))
#define TIME_TO_CHARS(val, where) time_to_chars (val, where, sizeof (where))
#define UID_TO_CHARS(val, where) uid_to_chars (val, where, sizeof (where))
#define UINTMAX_TO_CHARS(val, where) uintmax_to_chars (val, where, sizeof (where))

void gid_to_chars PARAMS ((gid_t, char *, size_t));
void major_to_chars PARAMS ((major_t, char *, size_t));
void minor_to_chars PARAMS ((minor_t, char *, size_t));
void mode_to_chars PARAMS ((mode_t, char *, size_t));
void off_to_chars PARAMS ((off_t, char *, size_t));
void size_to_chars PARAMS ((size_t, char *, size_t));
void time_to_chars PARAMS ((time_t, char *, size_t));
void uid_to_chars PARAMS ((uid_t, char *, size_t));
void uintmax_to_chars PARAMS ((uintmax_t, char *, size_t));

/* Module diffarch.c.  */

extern int now_verifying;

void diff_archive PARAMS ((void));
void diff_init PARAMS ((void));
void verify_volume PARAMS ((void));

/* Module extract.c.  */

extern int we_are_root;
void extr_init PARAMS ((void));
void extract_archive PARAMS ((void));
void extract_finish PARAMS ((void));
void fatal_exit PARAMS ((void)) __attribute__ ((noreturn));

/* Module delete.c.  */

void delete_archive_members PARAMS ((void));

/* Module incremen.c.  */

char *get_directory_contents PARAMS ((char *, dev_t));
void read_directory_file PARAMS ((void));
void write_directory_file PARAMS ((void));
void gnu_restore PARAMS ((size_t));

/* Module list.c.  */

enum read_header
{
  HEADER_STILL_UNREAD,		/* for when read_header has not been called */
  HEADER_SUCCESS,		/* header successfully read and checksummed */
  HEADER_SUCCESS_EXTENDED,	/* likewise, but we got an extended header */
  HEADER_ZERO_BLOCK,		/* zero block where header expected */
  HEADER_END_OF_FILE,		/* true end of file while header expected */
  HEADER_FAILURE		/* ill-formed header, or bad checksum */
};

extern union block *current_header;
extern struct stat current_stat;
extern enum archive_format current_format;

void decode_header PARAMS ((union block *, struct stat *,
			    enum archive_format *, int));
#define STRINGIFY_BIGINT(i, b) \
  stringify_uintmax_t_backwards ((uintmax_t) (i), (b) + UINTMAX_STRSIZE_BOUND)
char *stringify_uintmax_t_backwards PARAMS ((uintmax_t, char *));
char const *tartime PARAMS ((time_t));

#define GID_FROM_HEADER(where) gid_from_header (where, sizeof (where))
#define MAJOR_FROM_HEADER(where) major_from_header (where, sizeof (where))
#define MINOR_FROM_HEADER(where) minor_from_header (where, sizeof (where))
#define MODE_FROM_HEADER(where) mode_from_header (where, sizeof (where))
#define OFF_FROM_HEADER(where) off_from_header (where, sizeof (where))
#define SIZE_FROM_HEADER(where) size_from_header (where, sizeof (where))
#define TIME_FROM_HEADER(where) time_from_header (where, sizeof (where))
#define UID_FROM_HEADER(where) uid_from_header (where, sizeof (where))
#define UINTMAX_FROM_HEADER(where) uintmax_from_header (where, sizeof (where))

gid_t gid_from_header PARAMS ((const char *, size_t));
major_t major_from_header PARAMS ((const char *, size_t));
minor_t minor_from_header PARAMS ((const char *, size_t));
mode_t mode_from_header PARAMS ((const char *, size_t));
off_t off_from_header PARAMS ((const char *, size_t));
size_t size_from_header PARAMS ((const char *, size_t));
time_t time_from_header PARAMS ((const char *, size_t));
uid_t uid_from_header PARAMS ((const char *, size_t));
uintmax_t uintmax_from_header PARAMS ((const char *, size_t));

void list_archive PARAMS ((void));
void print_for_mkdir PARAMS ((char *, int, mode_t));
void print_header PARAMS ((void));
void read_and PARAMS ((void (*do_) ()));
enum read_header read_header PARAMS ((bool));
void skip_file PARAMS ((off_t));
void skip_member PARAMS ((void));

/* Module mangle.c.  */

void extract_mangle PARAMS ((void));

/* Module misc.c.  */

void assign_string PARAMS ((char **, const char *));
char *quote_copy_string PARAMS ((const char *));
int unquote_string PARAMS ((char *));

int contains_dot_dot PARAMS ((char const *));

int remove_any_file PARAMS ((const char *, int));
int maybe_backup_file PARAMS ((const char *, int));
void undo_last_backup PARAMS ((void));

int deref_stat PARAMS ((int, char const *, struct stat *));

int chdir_arg PARAMS ((char const *));
void chdir_do PARAMS ((int));

void decode_mode PARAMS ((mode_t, char *));

void chdir_fatal PARAMS ((char const *)) __attribute__ ((noreturn));
void chmod_error_details PARAMS ((char const *, mode_t));
void chown_error_details PARAMS ((char const *, uid_t, gid_t));
void close_error PARAMS ((char const *));
void close_warn PARAMS ((char const *));
void exec_fatal PARAMS ((char const *)) __attribute__ ((noreturn));
void link_error PARAMS ((char const *, char const *));
void mkdir_error PARAMS ((char const *));
void mkfifo_error PARAMS ((char const *));
void mknod_error PARAMS ((char const *));
void open_error PARAMS ((char const *));
void open_fatal PARAMS ((char const *)) __attribute__ ((noreturn));
void open_warn PARAMS ((char const *));
void read_error PARAMS ((char const *));
void read_error_details PARAMS ((char const *, off_t, size_t));
void read_fatal PARAMS ((char const *)) __attribute__ ((noreturn));
void read_fatal_details PARAMS ((char const *, off_t, size_t));
void read_warn_details PARAMS ((char const *, off_t, size_t));
void readlink_error PARAMS ((char const *));
void readlink_warn PARAMS ((char const *));
void savedir_error PARAMS ((char const *));
void savedir_warn PARAMS ((char const *));
void seek_error PARAMS ((char const *));
void seek_error_details PARAMS ((char const *, off_t));
void seek_warn PARAMS ((char const *));
void seek_warn_details PARAMS ((char const *, off_t));
void stat_error PARAMS ((char const *));
void stat_warn PARAMS ((char const *));
void symlink_error PARAMS ((char const *, char const *));
void truncate_error PARAMS ((char const *));
void truncate_warn PARAMS ((char const *));
void unlink_error PARAMS ((char const *));
void utime_error PARAMS ((char const *));
void waitpid_error PARAMS ((char const *));
void write_error PARAMS ((char const *));
void write_error_details PARAMS ((char const *, ssize_t, size_t));
void write_fatal PARAMS ((char const *)) __attribute__ ((noreturn));
void write_fatal_details PARAMS ((char const *, ssize_t, size_t))
     __attribute__ ((noreturn));

pid_t xfork PARAMS ((void));
void xpipe PARAMS ((int[2]));

char const *quote PARAMS ((char const *));
char const *quote_n PARAMS ((int, char const *));

/* Module names.c.  */

extern struct name *gnu_list_name;

void gid_to_gname PARAMS ((gid_t, char gname[GNAME_FIELD_SIZE]));
int gname_to_gid PARAMS ((char gname[GNAME_FIELD_SIZE], gid_t *));
void uid_to_uname PARAMS ((uid_t, char uname[UNAME_FIELD_SIZE]));
int uname_to_uid PARAMS ((char uname[UNAME_FIELD_SIZE], uid_t *));

void init_names PARAMS ((void));
void name_add PARAMS ((const char *));
void name_init PARAMS ((int, char *const *));
void name_term PARAMS ((void));
char *name_next PARAMS ((int));
void name_close PARAMS ((void));
void name_gather PARAMS ((void));
struct name *addname PARAMS ((char const *, int));
int name_match PARAMS ((const char *));
void names_notfound PARAMS ((void));
void collect_and_sort_names PARAMS ((void));
struct name *name_scan PARAMS ((const char *));
char *name_from_list PARAMS ((void));
void blank_name_list PARAMS ((void));
char *new_name PARAMS ((const char *, const char *));

bool excluded_name PARAMS ((char const *));

void add_avoided_name PARAMS ((char const *));
int is_avoided_name PARAMS ((char const *));

/* Module tar.c.  */

int confirm PARAMS ((const char *, const char *));
void request_stdin PARAMS ((const char *));

/* Module update.c.  */

extern char *output_start;

void update_archive PARAMS ((void));
