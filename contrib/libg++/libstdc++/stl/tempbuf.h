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

#ifndef TEMPBUF_H
#define TEMPBUF_H

#include <limits.h>
#include <pair.h>

#ifndef __stl_buffer_size
#define __stl_buffer_size 16384 // 16k
#endif

extern char __stl_temp_buffer[__stl_buffer_size];

//not reentrant code

template <class T>
pair<T*, int> get_temporary_buffer(int len, T*) {
    while (len > __stl_buffer_size / sizeof(T)) {
	set_new_handler(0);
        T* tmp = (T*)(::operator new((unsigned int)len * sizeof(T)));
        if (tmp) return pair<T*, int>(tmp, len);
        len = len / 2;
    }
    return pair<T*, int>((T*)__stl_temp_buffer, 
                         (int)(__stl_buffer_size / sizeof(T)));
}

template <class T>
void return_temporary_buffer(T* p) {
    if ((char*)(p) != __stl_temp_buffer) deallocate(p);
}

template <class T>
pair<T*, long> get_temporary_buffer(long len, T* p) {
    if (len > INT_MAX/sizeof(T)) 
	len = INT_MAX/sizeof(T);
    pair<T*, int> tmp = get_temporary_buffer((int)len, p);
    return pair<T*, long>(tmp.first, (long)(tmp.second));
}

#endif
