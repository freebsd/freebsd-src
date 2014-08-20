/* test/tests.h
 *
 * Set of test suites for libwapcaplet
 *
 * Copyright 2009 The NetSurf Browser Project
 *                Daniel Silverstone <dsilvers@netsurf-browser.org>
 */

#ifndef lwc_tests_h_
#define lwc_tests_h_

#include <signal.h>

#include <check.h>

#include "libwapcaplet/libwapcaplet.h"

extern void lwc_basic_suite(SRunner *);
extern void lwc_memory_suite(SRunner *);

#endif /* lwc_tests_h_ */
