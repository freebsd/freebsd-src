/*
 * Vector handling (counted lists of char *'s).
 *
 * A vector is a table for handling a list of strings with less overhead than
 * linked list.  The intention is for vectors, once allocated, to be reused;
 * this saves on memory allocations once the array of char *'s reaches a
 * stable size.
 *
 * This is based on the util/vector.c library, but that library uses xmalloc
 * routines to exit the program if memory allocation fails.  This is a
 * modified version of the vector library that instead returns false on
 * failure to allocate memory, allowing the caller to do appropriate recovery.
 *
 * Vectors require list of strings, not arbitrary binary data, and cannot
 * handle data elements containing nul characters.
 *
 * Only the portions of the vector library used by PAM modules are
 * implemented.
 *
 * The canonical version of this file is maintained in the rra-c-util package,
 * which can be found at <https://www.eyrie.org/~eagle/software/rra-c-util/>.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 * Copyright 2017-2018 Russ Allbery <eagle@eyrie.org>
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

#include <config.h>
#include <portable/system.h>

#include <pam-util/vector.h>


/*
 * Allocate a new, empty vector.  Returns NULL if memory allocation fails.
 */
struct vector *
vector_new(void)
{
    struct vector *vector;

    vector = calloc(1, sizeof(struct vector));
    vector->allocated = 1;
    vector->strings = calloc(1, sizeof(char *));
    return vector;
}


/*
 * Allocate a new vector that's a copy of an existing vector.  Returns NULL if
 * memory allocation fails.
 */
struct vector *
vector_copy(const struct vector *old)
{
    struct vector *vector;
    size_t i;

    vector = vector_new();
    if (!vector_resize(vector, old->count)) {
        vector_free(vector);
        return NULL;
    }
    vector->count = old->count;
    for (i = 0; i < old->count; i++) {
        vector->strings[i] = strdup(old->strings[i]);
        if (vector->strings[i] == NULL) {
            vector_free(vector);
            return NULL;
        }
    }
    return vector;
}


/*
 * Resize a vector (using reallocarray to resize the table).  Return false if
 * memory allocation fails.
 */
bool
vector_resize(struct vector *vector, size_t size)
{
    size_t i;
    char **strings;

    if (vector->count > size) {
        for (i = size; i < vector->count; i++)
            free(vector->strings[i]);
        vector->count = size;
    }
    if (size == 0)
        size = 1;
    strings = reallocarray(vector->strings, size, sizeof(char *));
    if (strings == NULL)
        return false;
    vector->strings = strings;
    vector->allocated = size;
    return true;
}


/*
 * Add a new string to the vector, resizing the vector as necessary.  The
 * vector is resized an element at a time; if a lot of resizes are expected,
 * vector_resize should be called explicitly with a more suitable size.
 * Return false if memory allocation fails.
 */
bool
vector_add(struct vector *vector, const char *string)
{
    size_t next = vector->count;

    if (vector->count == vector->allocated)
        if (!vector_resize(vector, vector->allocated + 1))
            return false;
    vector->strings[next] = strdup(string);
    if (vector->strings[next] == NULL)
        return false;
    vector->count++;
    return true;
}


/*
 * Empty a vector but keep the allocated memory for the pointer table.
 */
void
vector_clear(struct vector *vector)
{
    size_t i;

    for (i = 0; i < vector->count; i++)
        if (vector->strings[i] != NULL)
            free(vector->strings[i]);
    vector->count = 0;
}


/*
 * Free a vector completely.
 */
void
vector_free(struct vector *vector)
{
    if (vector == NULL)
        return;
    vector_clear(vector);
    free(vector->strings);
    free(vector);
}


/*
 * Given a vector that we may be reusing, clear it out.  If the first argument
 * is NULL, allocate a new vector.  Used by vector_split*.  Returns NULL if
 * memory allocation fails.
 */
static struct vector *
vector_reuse(struct vector *vector)
{
    if (vector == NULL)
        return vector_new();
    else {
        vector_clear(vector);
        return vector;
    }
}


/*
 * Given a string and a set of separators expressed as a string, count the
 * number of strings that it will split into when splitting on those
 * separators.
 */
static size_t
split_multi_count(const char *string, const char *seps)
{
    const char *p;
    size_t count;

    if (*string == '\0')
        return 0;
    for (count = 1, p = string + 1; *p != '\0'; p++)
        if (strchr(seps, *p) != NULL && strchr(seps, p[-1]) == NULL)
            count++;

    /*
     * If the string ends in separators, we've overestimated the number of
     * strings by one.
     */
    if (strchr(seps, p[-1]) != NULL)
        count--;
    return count;
}


/*
 * Given a string, split it at any of the provided separators to form a
 * vector, copying each string segment.  If the third argument isn't NULL,
 * reuse that vector; otherwise, allocate a new one.  Any number of
 * consecutive separators are considered a single separator.  Returns NULL on
 * memory allocation failure, after which the provided vector may only have
 * partial results.
 */
struct vector *
vector_split_multi(const char *string, const char *seps, struct vector *vector)
{
    const char *p, *start;
    size_t i, count;
    bool created = false;

    if (vector == NULL)
        created = true;
    vector = vector_reuse(vector);
    if (vector == NULL)
        return NULL;

    count = split_multi_count(string, seps);
    if (vector->allocated < count && !vector_resize(vector, count))
        goto fail;

    vector->count = 0;
    for (start = string, p = string, i = 0; *p != '\0'; p++)
        if (strchr(seps, *p) != NULL) {
            if (start != p) {
                vector->strings[i] = strndup(start, (size_t)(p - start));
                if (vector->strings[i] == NULL)
                    goto fail;
                i++;
                vector->count++;
            }
            start = p + 1;
        }
    if (start != p) {
        vector->strings[i] = strndup(start, (size_t)(p - start));
        if (vector->strings[i] == NULL)
            goto fail;
        vector->count++;
    }
    return vector;

fail:
    if (created)
        vector_free(vector);
    return NULL;
}


/*
 * Given a vector and a path to a program, exec that program with the vector
 * as its arguments.  This requires adding a NULL terminator to the vector and
 * casting it appropriately.  Returns 0 on success and -1 on error, like exec
 * does.
 */
int
vector_exec(const char *path, struct vector *vector)
{
    if (vector->allocated == vector->count)
        if (!vector_resize(vector, vector->count + 1))
            return -1;
    vector->strings[vector->count] = NULL;
    return execv(path, (char *const *) vector->strings);
}


/*
 * Given a vector, a path to a program, and the environment, exec that program
 * with the vector as its arguments and the given environment.  This requires
 * adding a NULL terminator to the vector and casting it appropriately.
 * Returns 0 on success and -1 on error, like exec does.
 */
int
vector_exec_env(const char *path, struct vector *vector,
                const char *const env[])
{
    if (vector->allocated == vector->count)
        if (!vector_resize(vector, vector->count + 1))
            return -1;
    vector->strings[vector->count] = NULL;
    return execve(path, (char *const *) vector->strings, (char *const *) env);
}
