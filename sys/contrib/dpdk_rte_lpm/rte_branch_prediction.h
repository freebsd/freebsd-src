/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

/**
 * @file
 * Branch Prediction Helpers in RTE
 */

#ifndef _RTE_BRANCH_PREDICTION_H_
#define _RTE_BRANCH_PREDICTION_H_

/**
 * Check if a branch is likely to be taken.
 *
 * This compiler builtin allows the developer to indicate if a branch is
 * likely to be taken. Example:
 *
 *   if (likely(x > 1))
 *      do_stuff();
 *
 */
#ifndef likely
#define likely(x)	__builtin_expect(!!(x), 1)
#endif /* likely */

/**
 * Check if a branch is unlikely to be taken.
 *
 * This compiler builtin allows the developer to indicate if a branch is
 * unlikely to be taken. Example:
 *
 *   if (unlikely(x < 1))
 *      do_stuff();
 *
 */
#ifndef unlikely
#define unlikely(x)	__builtin_expect(!!(x), 0)
#endif /* unlikely */

#endif /* _RTE_BRANCH_PREDICTION_H_ */
