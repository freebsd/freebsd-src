/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 The FreeBSD Foundation
 *
 * This software was developed by Christos Margiolis <christos@FreeBSD.org>
 * under sponsorship from the FreeBSD Foundation.
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

#include <sys/param.h>
#include <sys/soundcard.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define NOTE2OCTAVE(n)	(n / 12 - 1)
#define NOTE2FREQ(n)	(440 * pow(2.0f, ((float)n - 69) / 12))
#define CHAN_MASK	0x0f

static struct note {
	const char *name;
	const char *alt;
} notes[] = {
	{ "C",	NULL },
	{ "C#",	"Db" },
	{ "D",	NULL },
	{ "D#",	"Eb" },
	{ "E",	NULL },
	{ "F",	NULL },
	{ "F#",	"Gb" },
	{ "G",	NULL },
	{ "G#",	"Ab" },
	{ "A",	NULL },
	{ "A#",	"Bb" },
	{ "B",	NULL },
};

/* Hardcoded values are not defined in sys/soundcard.h. */
static const char *ctls[] = {
	[CTL_BANK_SELECT]		= "Bank Select",
	[CTL_MODWHEEL]			= "Modulation Wheel",
	[CTL_BREATH]			= "Breath Controller",
	[0x03]				= "Undefined",
	[CTL_FOOT]			= "Foot Pedal",
	[CTL_PORTAMENTO_TIME]		= "Portamento Time",
	[CTL_DATA_ENTRY]		= "Data Entry",
	[CTL_MAIN_VOLUME]		= "Volume",
	[CTL_BALANCE]			= "Balance",
	[0x09]				= "Undefined",
	[CTL_PAN]			= "Pan",
	[CTL_EXPRESSION]		= "Expression",
	[0x0c]				= "Effect Controller 1",
	[0x0d]				= "Effect Controller 2",
	[0x0e]				= "Undefined",
	[0x0f]				= "Undefined",
	[CTL_GENERAL_PURPOSE1]		= "General Purpose 1",
	[CTL_GENERAL_PURPOSE2]		= "General Purpose 2",
	[CTL_GENERAL_PURPOSE3]		= "General Purpose 3",
	[CTL_GENERAL_PURPOSE4]		= "General Purpose 4",
	[0x14 ... 0x1f]			= "Undefined",
	[0x20 ... 0x3f]			= "LSB Controller",
	[CTL_DAMPER_PEDAL]		= "Damper Pedal (Sustain)",
	[CTL_PORTAMENTO]		= "Portamento",
	[CTL_SOSTENUTO]			= "Sostenuto Pedal",
	[CTL_SOFT_PEDAL]		= "Soft Pedal",
	[0x44]				= "Legato Foot-Switch",
	[CTL_HOLD2]			= "Hold 2",
	[0x46]				= "Sound Controller 1",
	[0x47]				= "Sound Controller 2",
	[0x48]				= "Sound Controller 3",
	[0x49]				= "Sound Controller 4",
	[0x4a]				= "Sound Controller 5",
	[0x4b]				= "Sound Controller 6",
	[0x4c]				= "Sound Controller 7",
	[0x4d]				= "Sound Controller 8",
	[0x4e]				= "Sound Controller 9",
	[0x4f]				= "Sound Controller 10",
	[CTL_GENERAL_PURPOSE5]		= "General Purpose 5",
	[CTL_GENERAL_PURPOSE6]		= "General Purpose 6",
	[CTL_GENERAL_PURPOSE7]		= "General Purpose 7",
	[CTL_GENERAL_PURPOSE8]		= "General Purpose 8",
	[0x54]				= "Portamento CC",
	[0x55 ... 0x57]			= "Undefined",
	[0x58]				= "Hi-Res Velocity Prefix",
	[0x59 ... 0x5a]			= "Undefined",
	[CTL_EXT_EFF_DEPTH]		= "Effect 1 Depth",
	[CTL_TREMOLO_DEPTH]		= "Effect 2 Depth",
	[CTL_CHORUS_DEPTH]		= "Effect 3 Depth",
	[CTL_DETUNE_DEPTH]		= "Effect 4 Depth",
	[CTL_PHASER_DEPTH]		= "Effect 5 Depth",
	[CTL_DATA_INCREMENT]		= "Data Increment",
	[CTL_DATA_DECREMENT]		= "Data Decrement",
	[CTL_NONREG_PARM_NUM_LSB]	= "NRPN (LSB)",
	[CTL_NONREG_PARM_NUM_MSB]	= "NRPN (MSB)",
	[CTL_REGIST_PARM_NUM_LSB]	= "RPN (LSB)",
	[CTL_REGIST_PARM_NUM_MSB]	= "RPN (MSB)",
	[0x66 ... 0x77]			= "Undefined",
	/* Channel mode messages */
	[0x78]				= "All Sound Off",
	[0x79]				= "Reset All Controllers",
	[0x7a]				= "Local On/Off Switch",
	[0x7b]				= "All Notes Off",
	[0x7c]				= "Omni Mode Off",
	[0x7d]				= "Omni Mode On",
	[0x7e]				= "Mono Mode",
	[0x7f]				= "Poly Mode",
};

static void __dead2
usage(void)
{
	fprintf(stderr, "usage: %s [-t] device\n", getprogname());
	exit(1);
}

static uint8_t
read_byte(int fd)
{
	uint8_t byte;

	if (read(fd, &byte, sizeof(byte)) < (ssize_t)sizeof(byte))
		err(1, "read");

	return (byte);
}

int
main(int argc, char *argv[])
{
	struct note *pn;
	char buf[16];
	int fd, ch, tflag = 0;
	uint8_t event, chan, b1, b2;

	while ((ch = getopt(argc, argv, "t")) != -1) {
		switch (ch) {
		case 't':
			tflag = 1;
			break;
		case '?':	/* FALLTHROUGH */
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (argc < 1)
		usage();

	if ((fd = open(*argv, O_RDONLY)) < 0)
		err(1, "open(%s)", *argv);

	for (;;) {
		event = read_byte(fd);
		if (!(event & 0x80))
			continue;
		chan = (event & CHAN_MASK) + 1;

		switch (event) {
		case 0x80 ... 0x8f:	/* FALLTHROUGH */
		case 0x90 ... 0x9f:
			b1 = read_byte(fd);
			b2 = read_byte(fd);
			pn = &notes[b1 % nitems(notes)];
			snprintf(buf, sizeof(buf), "%s%d", pn->name,
			    NOTE2OCTAVE(b1));
			if (pn->alt != NULL) {
				snprintf(buf + strlen(buf), sizeof(buf),
				    "/%s%d", pn->alt, NOTE2OCTAVE(b1));
			}
			printf("Note %-3s		channel=%d, "
			    "note=%d (%s, %.2fHz), velocity=%d\n",
			    (event >= 0x80 && event <= 0x8f) ? "off" : "on",
			    chan, b1, buf, NOTE2FREQ(b1), b2);
			break;
		case 0xa0 ... 0xaf:
			b1 = read_byte(fd);
			b2 = read_byte(fd);
			printf("Polyphonic aftertouch	channel=%d, note=%d, "
			    "pressure=%d\n",
			    chan, b1, b2);
			break;
		case 0xb0 ... 0xbf:
			b1 = read_byte(fd);
			b2 = read_byte(fd);
			if (b1 > nitems(ctls) - 1)
				break;
			printf("Control/Mode change	channel=%d, "
			    "control=%d (%s), value=%d",
			    chan, b1, ctls[b1], b2);
			if (b1 >= 0x40 && b1 <= 0x45) {
				if (b2 <= 63)
					printf(" (off)");
				else
					printf(" (on)");
			}
			if (b1 == 0x7a) {
				if (b2 == 0)
					printf(" (off)");
				else if (b2 == 127)
					printf(" (on");
			}
			putchar('\n');
			break;
		case 0xc0 ... 0xcf:
			b1 = read_byte(fd);
			printf("Program change		channel=%d, "
			    "program=%d\n",
			    chan, b1);
			break;
		case 0xd0 ... 0xdf:
			b1 = read_byte(fd);
			printf("Channel aftertouch	channel=%d, "
			    "pressure=%d\n",
			    chan, b1);
			break;
		case 0xe0 ... 0xef:
			/* TODO Improve */
			b1 = read_byte(fd);
			b2 = read_byte(fd);
			printf("Pitch bend		channel=%d, change=%d\n",
			    chan, b1 | b2 << 7);
			break;
		case 0xf0:
			printf("SysEx			vendorid=");
			b1 = read_byte(fd);
			printf("0x%02x", b1);
			if (b1 == 0) {
				printf(" 0x%02x 0x%02x",
				    read_byte(fd), read_byte(fd));
			}
			printf(" data=");
			for (;;) {
				b1 = read_byte(fd);
				printf("0x%02x ", b1);
				/* End of SysEx (EOX) */
				if (b1 == 0xf7)
					break;
			}
			putchar('\n');
			break;
		case 0xf2:
			b1 = read_byte(fd);
			b2 = read_byte(fd);
			printf("Song position pointer	ptr=%d\n",
			    b1 | b2 << 7);
			break;
		case 0xf3:
			b1 = read_byte(fd);
			printf("Song select		song=%d\n", b1);
			break;
		case 0xf6:
			printf("Tune request\n");
			break;
		case 0xf7:
			printf("End of SysEx (EOX)\n");
			break;
		case 0xf8:
			if (tflag)
				printf("Timing clock\n");
			break;
		case 0xfa:
			printf("Start\n");
			break;
		case 0xfb:
			printf("Continue\n");
			break;
		case 0xfc:
			printf("Stop\n");
			break;
		case 0xfe:
			printf("Active sensing\n");
			break;
		case 0xff:
			printf("System reset\n");
			break;
		case 0xf1:		/* TODO? MIDI time code qtr. frame */
		case 0xf4:		/* Undefined (reserved) */
		case 0xf5:
		case 0xf9:
		case 0xfd:
			break;
		default:
			printf("Unknown event type: 0x%02x\n", event);
			break;
		}
	}

	close(fd);

	return (0);
}
