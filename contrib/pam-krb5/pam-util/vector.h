/*
 * Prototypes for vector handling.
 *
 * A vector is a list of strings, with dynamic resizing of the list as new
 * strings are added and support for various operations on strings (such as
 * splitting them on delimiters).
 *
 * Vectors require list of strings, not arbitrary binary data, and cannot
 * handle data elements containing nul characters.
 *
 * This is based on the util/vector.c library, but that library uses xmalloc
 * routines to exit the program if memory allocation fails.  This is a
 * modified version of the vector library that instead returns false on
 * failure to allocate memory, allowing the caller to do appropriate recovery.
 *
 * Only the portions of the vector library used by PAM modules are
 * implemented.
 *
 * The canonical version of this file is maintained in the rra-c-util package,
 * which can be found at <https://www.eyrie.org/~eagle/software/rra-c-util/>.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 * Copyright 2010-2011, 2014
 *     The Board of Trustees of the Leland Stanford Junior University
 *
 * Copying and distribution of this file, with or without modification, are
 * permitted in any medium without royalty provided the copyright notice and
 * this notice are preserved.  This file is offered as-is, without any
 * warranty.
 *
 * SPDX-License-Identifier: FSFAP
 */

#ifndef PAM_UTIL_VECTOR_H
#define PAM_UTIL_VECTOR_H 1

#include <config.h>
#include <portable/macros.h>
#include <portable/stdbool.h>

#include <stddef.h>

struct vector {
    size_t count;
    size_t allocated;
    char **strings;
};

BEGIN_DECLS

/* Default to a hidden visibility for all util functions. */
#pragma GCC visibility push(hidden)

/* Create a new, empty vector.  Returns NULL on memory allocation failure. */
struct vector *vector_new(void) __attribute__((__malloc__));

/*
 * Create a new vector that's a copy of an existing vector.  Returns NULL on
 * memory allocation failure.
 */
struct vector *vector_copy(const struct vector *)
    __attribute__((__malloc__, __nonnull__));

/*
 * Add a string to a vector.  Resizes the vector if necessary.  Returns false
 * on failure to allocate memory.
 */
bool vector_add(struct vector *, const char *string)
    __attribute__((__nonnull__));

/*
 * Resize the array of strings to hold size entries.  Saves reallocation work
 * in vector_add if it's known in advance how many entries there will be.
 * Returns false on failure to allocate memory.
 */
bool vector_resize(struct vector *, size_t size) __attribute__((__nonnull__));

/*
 * Reset the number of elements to zero, freeing all of the strings for a
 * regular vector, but not freeing the strings array (to cut down on memory
 * allocations if the vector will be reused).
 */
void vector_clear(struct vector *) __attribute__((__nonnull__));

/* Free the vector and all resources allocated for it. */
void vector_free(struct vector *);

/*
 * Split functions build a vector from a string.  vector_split_multi splits on
 * a set of characters.  If the vector argument is NULL, a new vector is
 * allocated; otherwise, the provided one is reused.  Returns NULL on memory
 * allocation failure, after which the provided vector may have been modified
 * to only have partial results.
 *
 * Empty strings will yield zero-length vectors.  Adjacent delimiters are
 * treated as a single delimiter by vector_split_multi.  Any leading or
 * trailing delimiters are ignored, so this function will never create
 * zero-length strings (similar to the behavior of strtok).
 */
struct vector *vector_split_multi(const char *string, const char *seps,
                                  struct vector *)
    __attribute__((__nonnull__(1, 2)));

/*
 * Exec the given program with the vector as its arguments.  Return behavior
 * is the same as execv.  Note the argument order is different than the other
 * vector functions (but the same as execv).  The vector_exec_env variant
 * calls execve and passes in the environment for the program.
 */
int vector_exec(const char *path, struct vector *)
    __attribute__((__nonnull__));
int vector_exec_env(const char *path, struct vector *, const char *const env[])
    __attribute__((__nonnull__));

/* Undo default visibility change. */
#pragma GCC visibility pop

END_DECLS

#endif /* UTIL_VECTOR_H */
