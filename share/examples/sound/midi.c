/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Goran Mekić
 * Copyright (c) 2024 The FreeBSD Foundation
 *
 * Portions of this software were developed by Christos Margiolis
 * <christos@FreeBSD.org> under sponsorship from the FreeBSD Foundation.
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
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define CMD_MASK	0xF0
#define CHANNEL_MASK	0x0F
#define NOTE_ON		0x90
#define NOTE_OFF	0x80
#define CTL_CHANGE	0xB0

int
main(int argc, char *argv[])
{
	int fd;
	unsigned char raw, type, channel, b1, b2;

	if ((fd = open("/dev/umidi0.0", O_RDWR)) < 0)
		err(1, "Error opening MIDI device");

	for (;;) {
		if (read(fd, &raw, sizeof(raw)) < sizeof(raw))
			err(1, "Error reading command byte");
		if (!(raw & 0x80))
			continue;

		type = raw & CMD_MASK;
		channel = raw & CHANNEL_MASK;

		if (read(fd, &b1, sizeof(b1)) < sizeof(b1))
			err(1, "Error reading byte 1");
		if (read(fd, &b2, sizeof(b2)) < sizeof(b2))
			err(1, "Error reading byte 2");

		switch (type) {
		case NOTE_ON:
			printf("Channel %d, note on %d, velocity %d\n",
			    channel, b1, b2);
			break;
		case NOTE_OFF:
			printf("Channel %d, note off %d, velocity %d\n",
			    channel, b1, b2);
			break;
		case CTL_CHANGE:
			printf("Channel %d, controller change %d, value %d\n",
			    channel, b1, b2);
			break;
		default:
			printf("Unknown event type %d\n", type);
			break;
		}
	}
	
	close(fd);

	return (0);
}
