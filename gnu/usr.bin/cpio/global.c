/* global.c - global variables and initial values for cpio.
   Copyright (C) 1990, 1991, 1992 Free Software Foundation, Inc.

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

#include <sys/types.h>
#include "cpiohdr.h"
#include "dstring.h"
#include "system.h"
#include "extern.h"

/* If TRUE, reset access times after reading files (-a).  */
int reset_time_flag = FALSE;

/* Block size value, initially 512.  -B sets to 5120.  */
int io_block_size = 512;

/* The header format to recognize and produce.  */
enum archive_format archive_format = arf_unknown;

/* If TRUE, create directories as needed. (-d with -i or -p) */
int create_dir_flag = FALSE;

/* If TRUE, interactively rename files. (-r) */
int rename_flag = FALSE;

/* If TRUE, print a table of contents of input. (-t) */
int table_flag = FALSE;

/* If TRUE, copy unconditionally (older replaces newer). (-u) */
int unconditional_flag = FALSE;

/* If TRUE, list the files processed, or ls -l style output with -t. (-v) */
int verbose_flag = FALSE;

/* If TRUE, print a . for each file processed. (-V) */
int dot_flag = FALSE;

/* If TRUE, link files whenever possible.  Used with -p option. (-l) */
int link_flag = FALSE;

/* If TRUE, retain previous file modification time. (-m) */
int retain_time_flag = FALSE;

/* Set TRUE if crc_flag is TRUE and we are doing a cpio -i.  Used
   by copy_files so it knows whether to compute the crc.  */
int crc_i_flag = FALSE;

/* If TRUE, append to end of archive. (-A) */
int append_flag = FALSE;

/* If TRUE, swap bytes of each file during cpio -i.  */
int swap_bytes_flag = FALSE;

/* If TRUE, swap halfwords of each file during cpio -i.  */
int swap_halfwords_flag = FALSE;

/* If TRUE, we are swapping halfwords on the current file.  */
int swapping_halfwords = FALSE;

/* If TRUE, we are swapping bytes on the current file.  */
int swapping_bytes = FALSE;

/* If TRUE, set ownership of all files to UID `set_owner'.  */
int set_owner_flag = FALSE;
uid_t set_owner;

/* If TRUE, set group ownership of all files to GID `set_group'.  */
int set_group_flag = FALSE;
gid_t set_group;

/* If TRUE, do not chown the files.  */
int no_chown_flag = FALSE;

#ifdef DEBUG_CPIO
/* If TRUE, print debugging information.  */
int debug_flag = FALSE;
#endif

/* File position of last header read.  Only used during -A to determine
   where the old TRAILER!!! record started.  */
int last_header_start = 0;

/* With -i; if TRUE, copy only files that match any of the given patterns;
   if FALSE, copy only files that do not match any of the patterns. (-f) */
int copy_matching_files = TRUE;

/* With -itv; if TRUE, list numeric uid and gid instead of translating them
   into names.  */
int numeric_uid = FALSE;

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

/* CRC checksum.  */
unsigned long crc;

/* Input and output buffers.  */
char *input_buffer, *output_buffer;

/* Current locations in `input_buffer' and `output_buffer'.  */
char *in_buff, *out_buff;

/* Current number of bytes stored at `input_buff' and `output_buff'.  */
long input_size, output_size;

/* Total number of bytes read and written for all files.  */
long input_bytes, output_bytes;

/* 512 bytes of 0; used for various padding operations.  */
char zeros_512[512];

/* Saving of argument values for later reference.  */
char *directory_name = NULL;
char **save_patterns;
int num_patterns;

/* Character that terminates file names read from stdin.  */
char name_end = '\n';

/* TRUE if input (cpio -i) or output (cpio -o) is a device node.  */
char input_is_special = FALSE;
char output_is_special = FALSE;

/* TRUE if lseek works on the input.  */
char input_is_seekable = FALSE;

/* TRUE if lseek works on the output.  */
char output_is_seekable = FALSE;

/* If nonzero, don't consider file names that contain a `:' to be
   on remote hosts; all files are local.  */
int f_force_local = 0;

/* The name this program was run with.  */
char *program_name;

/* A pointer to either lstat or stat, depending on whether
   dereferencing of symlinks is done for input files.  */
int (*xstat) ();

/* Which copy operation to perform. (-i, -o, -p) */
void (*copy_function) () = 0;
