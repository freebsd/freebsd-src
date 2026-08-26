/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The FreeBSD Foundation
 */

#ifndef	_LINUXKPI_LINUX_ARGS_H_
#define	_LINUXKPI_LINUX_ARGS_H_

#define	__COUNT_ARGS(_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, __count, ...)	\
    __count
#define	COUNT_ARGS(X...) \
    __COUNT_ARGS(, ##X, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0)

#define	CONCATENATE(a, b)	__CONCAT(a, b)

#endif /* _LINUXKPI_LINUX_ARGS_H_ */
