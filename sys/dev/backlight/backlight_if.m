#-
# SPDX-License-Identifier: BSD-2-Clause-FreeBSD
#
# Copyright (c) 2020 Emmanuel Vadot <manu@FreeBSD.org>
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#
# $FreeBSD$

#include <dev/backlight/backlight.h>

INTERFACE backlight;

CODE {
	static int
	backlight_default_update_status(device_t dev, struct backlight_props *props)
	{
		return (EOPNOTSUPP);
	}

	static int
	backlight_default_get_status(device_t dev, struct backlight_props *props)
	{
		return (EOPNOTSUPP);
	}

	static int
	backlight_default_get_info(device_t dev, struct backlight_info *info)
	{
		return (EOPNOTSUPP);
	}
};

METHOD int update_status {
	device_t dev;
	struct backlight_props *props;
} DEFAULT backlight_default_update_status;

METHOD int get_status {
	device_t dev;
	struct backlight_props *props;
} DEFAULT backlight_default_get_status;

METHOD int get_info {
	device_t dev;
	struct backlight_info *info;
} DEFAULT backlight_default_get_info;
