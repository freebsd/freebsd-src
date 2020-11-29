/* SPDX-License-Identifier: GPL-2.0 OR MIT */
/*
 * Copyright (C) 2015-2019 Jason A. Donenfeld <Jason@zx2c4.com>. All Rights Reserved.
 */

#ifndef _ZINC_SELFTEST_RUN_H
#define _ZINC_SELFTEST_RUN_H

static inline bool selftest_run(const char *name, bool (*selftest)(void),
				bool *const nobs[], unsigned int nobs_len)
{
	unsigned long set = 0, subset = 0, largest_subset = 0;
	unsigned int i;
	bool failed;

	MPASS(nobs_len <= BITS_PER_LONG);
	failed = false;

	for (i = 0; i < nobs_len; ++i)
		set |= ((unsigned long)*nobs[i]) << i;

	do {
		for (i = 0; i < nobs_len; ++i)
			*nobs[i] = BIT(i) & subset;
		if (selftest())
			largest_subset = max(subset, largest_subset);
		else {
			failed = true;
			pr_err("%s self-test combination 0x%lx: FAIL\n", name,
			       subset);
		}
		subset = (subset - set) & set;
	} while (subset);

	for (i = 0; i < nobs_len; ++i)
		*nobs[i] = BIT(i) & largest_subset;

	if (largest_subset == set && !failed && bootverbose)
		pr_info("%s self-tests: pass\n", name);

	return !WARN_ON(largest_subset != set);
}
#endif
