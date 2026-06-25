/*
 * oom.h
 * Exhaustive allocation-failure test driver, built on the fuzzer/alloc-inject
 * fault injector.
 *
 * SPDX-License-Identifier: pkgconf
 *
 * Copyright (c) 2026 pkgconf authors (see AUTHORS).
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * This software is provided 'as is' and without any warranty, express or
 * implied.  In no event shall the authors be liable for any damages arising
 * from the use of this software.
 */

#ifndef TEST_API_OOM_H
#define TEST_API_OOM_H

#include "test-api.h"
#include "alloc-inject.h"

/*
 * Exhaustively fail each successive allocation an expression makes until it can
 * complete with none failing.  Every injected failure must be reported
 * gracefully (NULL for OOM_TEST_PTR, false for OOM_TEST_BOOL); on the run where
 * no allocation fails the value succeeds and the loop ends.  Under ASAN this
 * also asserts each partial-construction error path leaks nothing.
 */
#define OOM_TEST_PTR(objvar, make_expr, free_stmt)	\
	do {	\
		for (unsigned long _oom_n = 1; ; _oom_n++)	\
		{	\
			alloc_inject_arm(_oom_n);	\
			(objvar) = (make_expr);	\
			bool _oom_f = alloc_inject_fired();	\
			alloc_inject_disarm();	\
			if (!_oom_f) { free_stmt; break; }	\
			TEST_ASSERT_NULL(objvar);	\
			free_stmt;	\
		}	\
	} while (0)

#define OOM_TEST_BOOL(make_expr)	\
	do {	\
		for (unsigned long _oom_n = 1; ; _oom_n++)	\
		{	\
			alloc_inject_arm(_oom_n);	\
			bool _oom_ok = (make_expr);	\
			bool _oom_f = alloc_inject_fired();	\
			alloc_inject_disarm();	\
			if (!_oom_f) { TEST_ASSERT_TRUE(_oom_ok); break; }	\
			TEST_ASSERT_FALSE(_oom_ok);	\
		}	\
	} while (0)

#endif // TEST_API_OOM_H
