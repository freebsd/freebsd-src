/*
 * Copyright (c) 1997, 1998 Martin Husemann <martin@rumolt.teuto.de>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software withough specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * vararray.h: basic collection macros for variable sized (growing) arrays:
 *             macro version of popular C++ template classes,
 *             not as elegant in use as the template version,
 *             but has maximum runtime performance and can give
 *             pointers to array contents (i.e. for ioctl's).
 *             Works in C as well as in C++.
 * CAVEAT:     in C++ only useable for aggregateable objects,
 *             it does use memcpy instead of copy constructors!
 */

#ifndef VARARRAY_H
#define VARARRAY_H
/* declare a variable sized array, element type is t */
#define VARA_DECL(t)    struct { int used, allocated; t *data; }

/* aggregate initializer for a variable array */
#define VARA_INITIALIZER        { 0, 0, NULL }

/* free all allocated storage */
#define VARA_FREE(a)  { if ((a).data != NULL) free((a).data);         \
                        (a).allocated = 0; (a).used = 0; }

/* number of elments currently in array*/
#define VARA_NUM(a)             ((a).used)

/* number of elements already allocated in array */
#define VARA_ALLOCATED(a) ((a).allocated)

/* pointer to array data */
#define VARA_PTR(a)             ((a).data)

/* empty the array */
#define VARA_EMPTY(a)   { (a).used = 0; }

#ifdef __cplusplus
#define VARA_NEW(t,c)   new t[c]
#define VARA_DELETE(p)  delete [] p
#else
#define VARA_NEW(t,c)   (t*)malloc(sizeof(t)*(c))
#define VARA_DELETE(p)  free(p)
#endif

/* add an element (not changing any data).
 * a is the array, i the index,
 * t the element type and n the initial allocation  */
#define VARA_ADD_AT(a,i,t,n)                                              \
{                                                                         \
        if ((i) >= (a).allocated) {                                       \
                int new_alloc = (a).allocated ? (a).allocated*2 : (n);    \
                t *new_data;                                              \
                if (new_alloc <= (i)) new_alloc = (i)+1;                  \
                new_data = VARA_NEW(t, new_alloc);                        \
                if ((a).data) {                                           \
                        memcpy(new_data, (a).data, (a).used*sizeof(t));   \
                        VARA_DELETE((a).data);                            \
                }                                                         \
                (a).data = new_data;                                      \
                (a).allocated = new_alloc;                                \
        }                                                                 \
        if ((i) >= (a).used) {                                            \
                if (i > (a).used)                                         \
                        memset(&((a).data[(a).used]), 0,                  \
                                        sizeof(t)*((i)-(a).used+1));      \
                (a).used = (i)+1;                                         \
        }                                                                 \
}

/* return an l-value at index i */
#define VARA_AT(a,i)    ((a).data[(i)])

/* iterate through the array */
#define VARA_FOREACH(a,i)       for ((i) = 0; (i) < (a).used; (i)++)

/* check for a valid index */
#define VARA_VALID(a,i)         ((i) >= 0 && (i) < (a).used)

/* remove one entry */
#define VARA_REMOVEAT(a,i)                                                  \
{                                                                           \
  if ((i) < ((a).used -1))                                                  \
    memmove(&((a).data[(i)]), &((a).data[(i)+1]), sizeof((a).data[0]));     \
  (a).used--;                                                               \
}

/* free all storage allocated for the array */
#define VARA_DESTROY(a)                               \
{                                                     \
  if ((a).data) VARA_DELETE((a).data);                \
  (a).allocated = 0;                                  \
  (a).used = 0;                                       \
  (a).data = NULL;                                    \
}

#endif

