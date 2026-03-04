/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The FreeBSD Foundation
 */

#ifndef	_LINUXKPI_LINUX_LINUX_LOGO_H_
#define	_LINUXKPI_LINUX_LINUX_LOGO_H_

struct linux_logo {
	int			 type;
	unsigned int		 width;
	unsigned int		 height;
	unsigned int		 clutsize;
	const unsigned char 	*clut;
	const unsigned char 	*data;
};

#endif /* _LINUXKPI_LINUX_LINUX_LOGO_H_ */
