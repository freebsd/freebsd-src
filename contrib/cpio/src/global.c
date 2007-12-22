/* global.c - global variables and initial values for cpio.
   Copyright (C) 1990, 1991, 1992, 2001 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along
   with this program; if not, write to the Free Software Foundation, Inc.,
   59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#include <system.h>

#include <sys/types.h>
#include "cpiohdr.h"
#include "dstring.h"
#include "extern.h"

/* If true, reset access times after reading files (-a).  */
int reset_time_flag = false;

/* Block size value, initially 512.  -B sets to 5120.  */
int io_block_size = 512;

/* The header format to recognize and produce.  */
enum archive_format archive_format = arf_unknown;

/* If true, create directories as needed. (-d with -i or -p) */
int create_dir_flag = false;

/* If true, interactively rename files. (-r) */
int rename_flag = false;

/* If non-NULL, the name of a file that will be read to
   rename all of the files in the archive.  --rename-batch-file.  */
char *rename_batch_file = NULL;

/* If true, print a table of contents of input. (-t) */
int table_flag = false;

/* If true, copy unconditionally (older replaces newer). (-u) */
int unconditional_flag = false;

/* If true, list the files processed, or ls -l style output with -t. (-v) */
int verbose_flag = false;

/* If true, print a . for each file processed. (-V) */
int dot_flag = false;

/* If true, link files whenever possible.  Used with -p option. (-l) */
int link_flag = false;

/* If true, retain previous file modification time. (-m) */
int retain_time_flag = false;

/* Set true if crc_flag is true and we are doing a cpio -i.  Used
   by copy_files so it knows whether to compute the crc.  */
int crc_i_flag = false;

/* If true, append to end of archive. (-A) */
int append_flag = false;

/* If true, swap bytes of each file during cpio -i.  */
int swap_bytes_flag = false;

/* If true, swap halfwords of each file during cpio -i.  */
int swap_halfwords_flag = false;

/* If true, we are swapping halfwords on the current file.  */
int swapping_halfwords = false;

/* If true, we are swapping bytes on the current file.  */
int swapping_bytes = false;

/* If true, set ownership of all files to UID `set_owner'.  */
int set_owner_flag = false;
uid_t set_owner;

/* If true, set group ownership of all files to GID `set_group'.  */
int set_group_flag = false;
gid_t set_group;

/* If true, do not chown the files.  */
int no_chown_flag = false;

/* If true, try to write sparse ("holey") files.  */
int sparse_flag = false;

/* If true, don't report number of blocks copied.  */
int quiet_flag = false;

/* If true, only read the archive and verify the files' CRC's, don't
   actually extract the files. */
int only_verify_crc_flag = false;

/* If true, don't use any absolute paths, prefix them by `./'.  */
int abs_paths_flag = false;

#ifdef DEBUG_CPIO
/* If true, print debugging information.  */
int debug_flag = false;
#endif

/* File position of last header read.  Only used during -A to determine
   where the old TRAILER!!! record started.  */
int last_header_start = 0;

/* With -i; if true, copy only files that match any of the given patterns;
   if false, copy only files that do not match any of the patterns. (-f) */
int copy_matching_files = true;

/* With -itv; if true, list numeric uid and gid instead of translating them
   into names.  */
int numeric_uid = false;

/* Name of file containing additional patterns (-E).  */
char *pattern_file_name = NULL;

/* Message to print when end of medium is reached (-M).  */
char *new_media_message = NULL;

/* With -M with %d, message to print when end of medium is reached.  */
char *new_media_message_with_number = NULL;
char *new_media_message_after_number = NULL;

/* File descriptor containing the archive.  */
int archive_des;

/* Name of file containing the archive, if known; NULL if stdin/out.  */
char *archive_name = NULL;

/* Name of the remote shell command, if known; NULL otherwise.  */
char *rsh_command_option = NULL;

/* CRC checksum.  */
unsigned long crc;

/* Input and output buffers.  */
char *input_buffer, *output_buffer;

/* The size of the input buffer.  */
long input_buffer_size;

/* Current locations in `input_buffer' and `output_buffer'.  */
char *in_buff, *out_buff;

/* Current number of bytes stored at `input_buff' and `output_buff'.  */
long input_size, output_size;

/* Total number of bytes read and written for all files.  
   Now that many tape drives hold more than 4Gb we need more than 32
   bits to hold input_bytes and output_bytes.  But it's not worth
   the trouble of adding special multi-precision arithmetic if the 
   compiler doesn't support 64 bit ints since input_bytes and
   output_bytes are only used to print the number of blocks copied.  */
#ifdef __GNUC__
long long input_bytes, output_bytes;
#else
long input_bytes, output_bytes;
#endif

/* 512 bytes of 0; used for various padding operations.  */
char zeros_512[512];

/* Saving of argument values for later reference.  */
char *directory_name = NULL;
char **save_patterns;
int num_patterns;

/* Character that terminates file names read from stdin.  */
char name_end = '\n';

/* true if input (cpio -i) or output (cpio -o) is a device node.  */
char input_is_special = false;
char output_is_special = false;

/* true if lseek works on the input.  */
char input_is_seekable = false;

/* true if lseek works on the output.  */
char output_is_seekable = false;

/* Print extra warning messages */
unsigned int warn_option = 0;

/* Extract to standard output? */
bool to_stdout_option = false;

/* The name this program was run with.  */
char *program_name;

/* A pointer to either lstat or stat, depending on whether
   dereferencing of symlinks is done for input files.  */
int (*xstat) ();

/* Which copy operation to perform. (-i, -o, -p) */
void (*copy_function) () = 0;
