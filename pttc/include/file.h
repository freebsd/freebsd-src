/*
 * Copyright (c) 2013-2018, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *  * Neither the name of Intel Corporation nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef FILE_H
#define FILE_H

#include <stddef.h>

/* Provides linewise access to a string.
 * Access to the lines is guarded by the text_line function.
 */
struct text {
	/* Number of lines.  */
	size_t n;

	/* Each line[0] to line[n-1] points to the start of the
	 * corresponding line.
	 */
	char **line;
};

/* Allocates new text.
 *
 * Note, if s is NULL or the empty string the text has zero lines.
 *
 * Returns a non-NULL text object on success; NULL otherwise.
 */
extern struct text *text_alloc(const char *s);

/* Deallocates @t.
 * If @t is the NULL pointer, nothing happens.
 */
extern void text_free(struct text *t);

/* Initializes @t with @s.  All "\n" or "\r\n" lineendings, will be
 * replaced with '\0'.
 *
 * Returns 0 on success; a negative enum errcode otherwise.
 * Returns -err_internal if @t is the NULL pointer.
 */
extern int text_parse(struct text *t, const char *s);

/* Copies at most @destlen characters of line @n from text @t to @dest.
 * The line counts start with 0.
 * If @dest is the NULL pointer just the line number is checked.
 *
 * Returns 0 on success; a negative enum errcode otherwise.
 * Returns -err_internal if @t is the NULL pointer or if @dest is the
 * NULL pointer, but @destlen is non-zero.
 * Returns -err_out_of_range if @n is not in the range.
 *
 * Note, the string is always null byte terminated on success.
 */
extern int text_line(const struct text *t, char *dest, size_t destlen,
		     size_t n);

/* Provides access to lines of files.  Access to all files is cached
 * after the first request.
 *
 * By convention, the first file_list element in the list is the head
 * and stores no file information.
 */
struct file_list {
	/* Name of the file.  */
	char *filename;

	/* The content of the file.  */
	struct text *text;

	/* Points to the next file list entry.  It's NULL if the
	 * current file_list is the last entry in the list.
	 */
	struct file_list *next;
};

/* Allocates a new file list.
 *
 * Returns a non-NULL file list object on succes; NULL otherwise.
 */
extern struct file_list *fl_alloc(void);

/* Deallocates @fl.
 * If @fl is the NULL pointer, nothing happens.
 */
extern void fl_free(struct file_list *fl);

/* Looks up line @n in a file @filename.  The line content is stored in
 * @dest, which should have a capacity of @destlen.
 * If @dest is the NULL pointer just the line number is checked.
 * See function text_line how the line is copied to @dest.
 * The file @filename is loaded implicitly.
 *
 * Returns 0 on success; a negative enum errcode otherwise.
 * Returns -err_internal if @fl or @filename is the NULL pointer or if
 * @dest is the NULL pointer, but @destlen is non-zero.
 * Returns -err_out_of_range if n is not a valid line number.
 * Returns -err_file_stat if @filename could not be found.
 * Returns -err_file_open if @filename could not be opened.
 * Returns -err_file_read if the content of @filename could not be fully
 * read.
 */
extern int fl_getline(struct file_list *fl, char *dest, size_t destlen,
		      const char *filename, size_t n);

/* Looks up the text for @filename and stores its contents in @t.
 * The file @filename is loaded implicitly.
 *
 * Returns 0 on success; a negative enum errcode otherwise.
 * Returns -err_internal if @fl or @t or @filename is the NULL pointer.
 * Returns -err_file_stat if @filename could not be found.
 * Returns -err_file_open if @filename could not be opened.
 * Returns -err_file_read if the content of @filename could not be fully
 * read.
 */
extern int fl_gettext(struct file_list *fl, const struct text **t,
		      const char *filename);

#endif /* FILE_H */
