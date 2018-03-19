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

#ifndef UTIL_H
#define UTIL_H

#include <stdint.h>

/* Duplicates @s and returns a pointer to it.
 *
 * The returned pointer must be freed by the caller.
 *
 * Returns the pointer to the duplicate on success; otherwise NULL is
 * returned.
 */
extern char *duplicate_str(const char *s);

/* Converts the string @str into an usigned x-bit value @val using base @base.
 *
 * Returns 0 on success; a negative enum errcode otherwise.
 * Returns -err_internal if either @str or @val is NULL.
 * Returns -err_parse_int if there was a general parsing error.
 * Returns -err_parse_int_too_big if parsed value wouldn't fit into x bit.
 */
extern int str_to_uint64(const char *str, uint64_t *val, int base);
extern int str_to_uint32(const char *str, uint32_t *val, int base);
extern int str_to_uint16(const char *str, uint16_t *val, int base);
extern int str_to_uint8(const char *str, uint8_t *val, int base);

/* Executes @file and passes @argv as command-line arguments.
 * The last element in @argv must be NULL.
 *
 * Returns 0 on success; a negative enum errcode otherwise.
 */
extern int run(const char *file, char *const argv[]);

/* Prints condstr, together with file and line, to stderr if cond is not 0.
 * Please do not use this function directly, use the bug_on convenience
 * macro.
 *
 * Returns cond.
 */
extern int do_bug_on(int cond, const char *condstr, const char *file, int line);

/* Convenience macro that wraps cond as condstr and current file and line
 * for do_bug_on.
 *
 * Returns cond.
 */
#define bug_on(cond) do_bug_on(cond, #cond, __FILE__, __LINE__)

/* Represents a label list with the corresponding address.
 *
 * By convention, the first label in the list is the head and stores
 * no label information.
 */
struct label {
	/* Labelname.  */
	char *name;

	/* Address associated with the label.  */
	uint64_t addr;

	/* The next label in the list.  */
	struct label *next;
};

/* Allocates a new label list.
 *
 * Returns a non-NULL label list object on success; NULL otherwise.
 */
extern struct label *l_alloc(void);

/* Deallocates and clears all elements in the list denoted by @l.
 * If @l is the NULL pointer, nothing happens.
 */
extern void l_free(struct label *l);

/* Appends a label to the last element in @l with @name and @addr.
 *
 * Returns 0 on success; a negative enum errcode otherwise.
 */
extern int l_append(struct label *l, const char *name, uint64_t addr);

/* Looks up the label @name in @l and stores the address where @addr points to.
 *
 * Returns 0 on success; a negative enum errcode otherwise.
 * Returns -err_internal if @l or @addr or @name is the NULL pointer.
 * Returns -err_no_label if a label with @name does not exist in @l.
 */
extern int l_lookup(const struct label *l, uint64_t *addr, const char *name);

/* Find the label @name in @l and return a pointer to it.
 *
 * Returns a pointer to the found label on success; NULL otherwise.
 */
extern struct label *l_find(struct label *l, const char *name);

#endif /* UTIL_H */
