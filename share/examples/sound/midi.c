/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Goran Mekić
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

#include <unistd.h>

#include "ossmidi.h"

int
main()
{
	midi_event_t event;
	midi_config_t midi_config;
	int l = -1;
	unsigned char raw;

	midi_config.device = "/dev/umidi1.0";
	oss_midi_init(&midi_config);

	while ((l = read(midi_config.fd, &raw, sizeof(raw))) != -1) {
		if (!(raw & 0x80)) {
			continue;
		}
		event.type = raw & CMD_MASK;
		event.channel = raw & CHANNEL_MASK;
		switch (event.type) {
		case NOTE_ON:
		case NOTE_OFF:
		case CONTROLLER_ON:
			if ((l = read(midi_config.fd, &(event.note), sizeof(event.note))) == -1) {
				perror("Error reading MIDI note");
				exit(1);
			}
			if ((l = read(midi_config.fd, &(event.velocity), sizeof(event.velocity))) == -1) {
				perror("Error reading MIDI velocity");
				exit(1);
			}
			break;
		}
		switch (event.type) {
		case NOTE_ON:
		case NOTE_OFF:
			printf("Channel %d, note %d, velocity %d\n", event.channel, event.note, event.velocity);
			break;
		case CONTROLLER_ON:
			printf("Channel %d, controller %d, value %d\n", event.channel, event.controller, event.value);
			break;
		default:
			printf("Unknown event type %d\n", event.type);
		}
	}
	return 0;
}
