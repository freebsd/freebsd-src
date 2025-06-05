package t_array;

use strict;
use vars qw(@ISA);

#require ktemplate;
require t_template;

@ISA=qw(t_template);

my @parms = qw(NAME TYPE);
my %defaults = ( );
my @templatelines = <DATA>;

sub new { # no args
    my $self = {};
    bless $self;
    $self->init(\@parms, \%defaults, \@templatelines);
    return $self;
}

__DATA__

/*
 * array type, derived from template
 *
 * parameters:
 * NAME: <NAME>
 * TYPE: <TYPE>
 *
 * methods:
 * int init() -> nonzero if fail initial allocation
 * unsigned long size() -> nonnegative number of values stored
 * int grow(newsize) -> negative if fail allocation, memset(,0,) new space
 * <TYPE> *getaddr(idx) -> aborts if out of range
 * void set(idx, value) -> aborts if out of range
 * <TYPE> get(idx) -> value, or aborts if out of range
 */

#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <stdint.h>

struct <NAME>__header {
    size_t allocated;
    <TYPE> *elts;
};
typedef struct <NAME>__header <NAME>;

static inline int
<NAME>_init(<NAME> *arr)
{
    arr->elts = calloc(10, sizeof(<TYPE>));
    if (arr->elts == NULL)
	return ENOMEM;
    arr->allocated = 10;
    return 0;
}

static inline long
<NAME>_size(<NAME> *arr)
{
    return arr->allocated;
}

static inline unsigned long
<NAME>_max_size(<NAME> *arr)
{
    size_t upper_bound;

    upper_bound = SIZE_MAX / sizeof(*arr->elts);
    if (upper_bound > ULONG_MAX)
	upper_bound = ULONG_MAX;
    return (unsigned long) upper_bound;
}

static inline int
<NAME>_grow(<NAME> *arr, unsigned long newcount)
{
    size_t oldsize = sizeof(*arr->elts) * arr->allocated;
    size_t newsize;
    void *ptr;

    if (newcount > LONG_MAX)
	return -1;
    if (newcount < arr->allocated)
	return 0;
    if (newcount > <NAME>_max_size(arr))
	return -1;

    newsize = sizeof(*arr->elts) * newcount;
    ptr = realloc(arr->elts, newsize);
    if (ptr == NULL)
	return -1;
    memset((char *)ptr + oldsize, 0, newsize - oldsize);
    arr->elts = ptr;
    arr->allocated = newcount;
    return 0;
}

static inline <TYPE> *
<NAME>_getaddr (<NAME> *arr, long idx)
{
    if (idx < 0 || (unsigned long) idx >= arr->allocated)
	abort();
    return arr->elts + idx;
}

static inline void
<NAME>_set (<NAME> *arr, long idx, <TYPE> value)
{
    <TYPE> *newvalp;
    newvalp = <NAME>_getaddr(arr, idx);
    *newvalp = value;
}

static inline <TYPE>
<NAME>_get (<NAME> *arr, long idx)
{
    return *<NAME>_getaddr(arr, idx);
}

static inline void
<NAME>_destroy (<NAME> *arr)
{
    free(arr->elts);
    arr->elts = 0;
}
