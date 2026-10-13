/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Vladimir Grebenshchikov <vova@zote.me>
 */

#ifndef _LINUX_TEST_SUPPORT_H_
#define	_LINUX_TEST_SUPPORT_H_

#include <sys/types.h>
#include <atf-c.h>

void linux_test_helper_path(const atf_tc_t *, const char *, char *, size_t);
void linux_test_run(const atf_tc_t *, const char *);

#endif /* _LINUX_TEST_SUPPORT_H_ */
