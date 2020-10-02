/*-
 * Copyright (c) 2020 The FreeBSD Foundation
 *
 * This software was developed by Emmanuel Vadot under sponsorship
 * from the FreeBSD Foundation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef __LINUX_DMI_H__
#define	__LINUX_DMI_H__

#include <linux/mod_devicetable.h>

int linux_dmi_check_system(const struct dmi_system_id *);
bool linux_dmi_match(enum dmi_field, const char *);
const struct dmi_system_id *linux_dmi_first_match(const struct dmi_system_id *);
const char *linux_dmi_get_system_info(int);

#define	dmi_check_system(sysid)	linux_dmi_check_system(sysid)
#define	dmi_match(f, str)	linux_dmi_match(f, str)
#define	dmi_first_match(sysid)	linux_dmi_first_match(sysid)
#define	dmi_get_system_info(sysid)	linux_dmi_get_system_info(sysid)

#endif	/* __LINUX_DMI_H__ */
