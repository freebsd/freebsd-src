/*-
 * Copyright (c) 1995 Mikael Hybsch
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 *
 */

#ifndef SCD_H
#define	SCD_H

#if defined(__GNUC__) || defined(__INTEL_COMPILER)
#if __GNUC__ >= 2 || defined(__INTEL_COMPILER)
#pragma pack(1)
#endif
#endif

typedef unsigned char	bcd_t;
#define	M_msf(msf)	msf[0]
#define	S_msf(msf)	msf[1]
#define	F_msf(msf)	msf[2]

#define OREG_COMMAND	0
#define OREG_WPARAMS	1
#define OREG_CONTROL	3
#define CBIT_ATTENTION_CLEAR		0x01
#define CBIT_RESULT_READY_CLEAR		0x02
#define CBIT_DATA_READY_CLEAR		0x04
#define CBIT_RPARAM_CLEAR		0x40
#define CBIT_RESET_DRIVE		0x80

#define IREG_STATUS  0
#define SBIT_ATTENTION		0x01
#define SBIT_RESULT_READY	0x02
#define SBIT_DATA_READY		0x04
#define SBIT_BUSY		0x80

#define IREG_RESULT		1
#define IREG_DATA		2
#define IREG_FSTATUS		3
#define FBIT_WPARAM_READY	0x01

#define CMD_GET_DRIVE_CONFIG	0x00
#define CMD_SET_DRIVE_PARAM	0x10
#define CMD_GET_SUBCHANNEL_DATA	0x21
#define CMD_GET_TOC		0x24
#define CMD_READ_TOC		0x30
#define CMD_READ		0x34
#define CMD_PLAY_AUDIO		0x40
#define CMD_STOP_AUDIO		0x41
#define CMD_EJECT		0x50
#define CMD_SPIN_UP		0x51
#define CMD_SPIN_DOWN		0x52

#define ERR_CD_NOT_LOADED	0x20
#define ERR_NO_CD_INSIDE	0x21
#define ERR_NOT_SPINNING	0x22
#define ERR_FATAL_READ_ERROR1	0x53
#define ERR_FATAL_READ_ERROR2	0x57

#define ATTEN_DRIVE_LOADED	0x80
#define ATTEN_EJECT_PUSHED	0x81
#define ATTEN_AUDIO_DONE	0x90
#define ATTEN_SPIN_UP_DONE	0x24
#define ATTEN_SPIN_DOWN		0x27
#define ATTEN_EJECT_DONE	0x28


struct sony_drive_configuration {
	char vendor[8];
	char product[16];
	char revision[8];
	u_short config;
};

/* Almost same as cd_sub_channel_position_data */
struct sony_subchannel_position_data {
	u_char	control:4;
	u_char	addr_type:4;
	u_char	track_number;
	u_char	index_number;
	u_char	rel_msf[3];
	u_char	dummy;
	u_char	abs_msf[3];
};

struct sony_tracklist {
	u_char adr :4; /* xcdplayer needs these two values */
	u_char ctl :4;
	u_char track;
	u_char start_msf[3];
};

#define MAX_TRACKS 100

struct sony_toc {
	u_char session_number;

	u_char :8;
	u_char :8;
	u_char first_track;
	u_char :8;
	u_char :8;

	u_char :8;
	u_char :8;
	u_char last_track;
	u_char :8;
	u_char :8;

	u_char :8;
	u_char :8;
	u_char lead_out_start_msf[3];

	struct sony_tracklist tracks[MAX_TRACKS];

	/* The rest is just to take space in case all data is returned */

	u_char dummy[6*9];
};

#endif /* SCD_H */
