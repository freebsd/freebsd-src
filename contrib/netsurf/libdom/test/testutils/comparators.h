/*
 * This file is part of libdom test suite.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2007 James Shaw <jshaw@netsurf-browser.org>
 * Copyright 2009 Bo Yang <struggleyb.nku@gmail.com>
 */

#ifndef comparators_h_
#define comparators_h_

/**
 * A function pointer type for a comparator.
 */
typedef int (*comparator)(const void* a, const void* b);

int int_comparator(const void* a, const void* b);

int str_icmp(const void *a, const void *b);
int str_icmp_r(const void *a, const void *b);
int str_cmp(const void *a, const void *b);
int str_cmp_r(const void *a, const void *b);
#endif
