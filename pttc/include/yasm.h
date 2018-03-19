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

#ifndef YASM_H
#define YASM_H

#include "file.h"
#include "util.h"

#include <stdint.h>

/* Parses all labels in @t and appends them to @l.
 *
 * Returns 0 on success; a negative enum errcode otherwise.
 * Returns -err_section if @t contains a "[section]" yasm directive.
 * Sections are currently not supported.
 * Returns -err_label_addr if the address for a label could not be
 * determined.
 */
extern int parse_yasm_labels(struct label *l, const struct text *t);

/* Modifies @s, so it can be used as a label, if @s actually looks like
 * a label.
 *
 * Returns true if @s looks like a label; false otherwise.
 * Returns -err_internal if @l or @name is the NULL pointer.
 */
extern int make_label(char *s);

/* Represents the state of the pt directive parser.  The parser uses the
 * canonical yasm lst file syntax to follow all asm source files that
 * were used during a yasm run.  The lst file stores information about
 * these files in terms of line numbers and line increments.  With this
 * information the contents of the lst file can be correlated to the
 * actual source files.
 */
struct state {
	/* Current line number.  */
	int n;

	/* Current line increment for this file.  */
	int inc;

	/* Current filename.  */
	char *filename;

	/* Pointer to the current line.  */
	char *line;
};

/* Allocates new state.
 *
 * Returns a non-NULL state object on success; NULL otherwise.
 */
extern struct state *st_alloc(void);

/* Deallocates and clears all fields of @st.
 * If @st is the NULL pointer, nothing happens.
 */
extern void st_free(struct state *st);

/* Prints @s to stderr enriched with @st's file and line information.
 *
 * Returns @errcode on success.
 * Returns -err_internal if @st is the NULL pointer or @errcode is
 * not negative.
 */
extern int st_print_err(const struct state *st, const char *s, int errcode);

/* The kind of directive: Intel PT or sideband. */
enum pt_directive_kind {
	pdk_pt,
#if defined(FEATURE_SIDEBAND)
	pdk_sb,
#endif
};

/* Represents a pt directive with name and payload.  */
struct pt_directive {
	/* The kind of the directive. */
	enum pt_directive_kind kind;

	/* Name of the directive.  */
	char *name;

	/* Length of name.  */
	size_t nlen;

	/* Everything between the '(' and ')' in the directive.  */
	char *payload;

	/* Length of payoad.  */
	size_t plen;
};

/* Allocates a new pt directive that can hold a directive name and
 * payload of no more than @n characters.
 *
 * Returns a non-NULL pt directive object on success; NULL otherwise.
 */
extern struct pt_directive *pd_alloc(size_t n);

/* Deallocates and clears all fields of @pd.
 * If @pd is the NULL pointer, nothing happens.
 */
extern void pd_free(struct pt_directive *pd);

/* Copies @kind, @name and @payload to the corresponding fields in @pd.
 *
 * Returns 0 on success; a negative enum errcode otherwise.
 * Returns -err_internal if @pd or @name or @payload is the NULL
 * pointer.
 */
extern int pd_set(struct pt_directive *pd, enum pt_directive_kind kind,
		  const char *name, const char *payload);

/* Parses a pt directive from @st and stores it in @pd.
 *
 * Returns 0 on success; a negative enum errcode otherwise.
 * Returns -err_internal if @pd or @st is the NULL pointer.
 */
extern int pd_parse(struct pt_directive *pd, struct state *st);

/* Represents a yasm assembled file.  */
struct yasm {
	/* Filename of the .asm file.  */
	char *pttfile;

	/* Filename of the .lst file.  It is the concatenation of
	 * fileroot and ".lst".
	 */
	char *lstfile;

	/* Filename of the .bin file.  It is the concatenation of
	 * fileroot and ".bin".
	 */
	char *binfile;

	/* Fileroot is the pttfile filename, but with a trailing file
	 * extension removed.  It is used to create files based on the
	 * pttfile and is also used to create the .pt and .exp files
	 * during the parsing step.
	 */
	char *fileroot;

	/* The list of files that are encountered while parsing the
	 * lstfile.
	 */
	struct file_list *fl;

	/* State of the current assembly file, while parsing the
	 * lstfile.
	 */
	struct state *st_asm;

	/* Current line number in the lstfile.  */
	int lst_curr_line;

	/* The list of labels found in the lstfile.  */
	struct label *l;
};

/* Allocates a new yasm container with @pttfile.
 *
 * Returns a non-NULL yasm container object on success; NULL otherwise.
 */
extern struct yasm *yasm_alloc(const char *pttfile);

/* Deallocates and clears all field of @y.
 * If @y is the NULL pointer, nothing happens.
 */
extern void yasm_free(struct yasm *y);

/* Assembles the pttfile with yasm and parses all labels.
 *
 * Returns 0 on success; a negative enum errcode otherwise.
 */
extern int yasm_parse(struct yasm *y);

/* Looks up @labelname and stores its address in @addr if found.
 *
 * Returns 0 on success; a negative enum errcode otherwise.
 */
extern int yasm_lookup_label(const struct yasm *y, uint64_t *addr,
			     const char *labelname);

/* Looks up the special section label "section_@name_@attribute" and stores
 * its value in @value if found.
 *
 * Valid attributes are:
 *
 * - start    the section's start address in the binary file
 * - vstart   the section's virtual load address
 * - length   the section's size in bytes
 *
 * Returns 0 on success; a negative enum errcode otherwise.
 */
extern int yasm_lookup_section_label(const struct yasm *y, const char *name,
				     const char *attribute, uint64_t *value);

/* Stores the next pt directive in @pd.
 *
 * Returns 0 on success; a negative enum errcode otherwise.
 * Returns -err_internal if @y or @pd is the NULL pointer.
 * Returns -err_no_directive if there is no pt directive left.
 */
extern int yasm_next_pt_directive(struct yasm *y, struct pt_directive *pd);

/* Calls pd_parse for the current file and line.
 *
 * Returns 0 on success; a negative enum errcode otherwise.
 * Returns -err_no_directive if the current source line contains no PT
 * directive.
 */
extern int yasm_pd_parse(struct yasm *y, struct pt_directive *pd);

/* Stores the next line in the asm file into @dest.  The memory behind
 * @dest must be large enough to store @destlen bytes.
 *
 * Returns 0 on success; a negative enum errcode otherwise.
 * Returns -err_internal if @y is the NULL pointer or @dest is NULL, but
 * @destlen is non-zero.
 */
extern int yasm_next_line(struct yasm *y, char *dest, size_t destlen);

/* Prints the error message @s together with errstr[@errcode].  File and
 * line information are printed regarding the current state of @y.
 *
 * Returns 0 on success; a negative enum errcode otherwise.
 * Returns -err_internal if @errcode is not negative.
 */
extern int yasm_print_err(const struct yasm *y, const char *s, int errcode);

#endif /* YASM_H */
