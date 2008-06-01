/*
 *
 * Copyright (c) 1994
 * Hewlett-Packard Company
 *
 * Permission to use, copy, modify, distribute and sell this software
 * and its documentation for any purpose is hereby granted without fee,
 * provided that the above copyright notice appear in all copies and
 * that both that copyright notice and this permission notice appear
 * in supporting documentation.  Hewlett-Packard Company makes no
 * representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 *
 */

#ifndef FMULTISET_H
#define FMULTISET_H

#ifdef MULTISET_H
#undef MULTISET_H
#undef TREE_H
#define __MULTISET_WAS_DEFINED
#endif

#define Allocator far_allocator
#define multiset far_multiset
#define rb_tree far_rb_tree
#include <faralloc.h>
#include <multiset.h>

#undef MULTISET_H
#undef TREE_H

#ifdef __MULTISET_WAS_DEFINED
#define MULTISET_H
#define TREE_H
#undef  __MULTISET_WAS_DEFINED
#endif

#undef Allocator
#undef multiset
#undef rb_tree

#endif
