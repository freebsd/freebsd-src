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

#ifndef LNGALLOC_H
#define LNGALLOC_H

#include <limits.h>
#include <algobase.h>
#include <defalloc.h>

template <class T>
class long_allocator {
public:
    typedef T value_type;
    typedef T* pointer;
    typedef const T* const_pointer;
    typedef T& reference;
    typedef const T& const_reference;
    typedef unsigned long size_type;
    typedef long difference_type;
    pointer allocate(size_type n) { 
	return ::allocate((difference_type)n, (pointer)0);
    }
    void deallocate(pointer p) { ::deallocate(p); }
    pointer address(reference x) { return (pointer)&x; }
    const_pointer const_address(const_reference x) { 
	return (const_pointer)&x; 
    }
    size_type init_page_size() { 
	return max(size_type(1), size_type(4096/sizeof(T))); 
    }
    size_type max_size() const { 
	return max(size_type(1), size_type(ULONG_MAX/sizeof(T))); 
    }
};

class long_allocator<void> {
public:
    typedef void* pointer;
};

#endif
