/*-
 * Copyright (c) 1996 Kazutaka YOKOTA (yokota@zodiac.mech.utsunomiya-u.ac.jp)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote 
 *    products derived from this software without specific prior written 
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Id: kbdio.h,v 1.1 1996/11/14 22:19:09 sos Exp $
 */

#ifndef _I386_ISA_KBDIO_H_
#define	_I386_ISA_KBDIO_H_

/* constants */

/* I/O ports */
#define KBD_STATUS_PORT 	4	/* status port, read */
#define KBD_COMMAND_PORT	4	/* controller command port, write */
#define KBD_DATA_PORT		0	/* data port, read/write 
					   also used as keyboard command
					   and mouse command port */
/* FIXME: `IO_PSMSIZE' should really be in `isa.h'. */
#define IO_PSMSIZE		(KBD_COMMAND_PORT - KBD_DATA_PORT + 1) /* 5 */

/* controller commands (sent to KBD_COMMAND_PORT) */
#define KBDC_SET_COMMAND_BYTE 	0x0060
#define KBDC_GET_COMMAND_BYTE 	0x0020
#define KBDC_WRITE_TO_AUX    	0x00d4
#define KBDC_DISABLE_AUX_PORT 	0x00a7
#define KBDC_ENABLE_AUX_PORT 	0x00a8
#define KBDC_TEST_AUX_PORT   	0x00a9
#define KBDC_DIAGNOSE	     	0x00aa
#define KBDC_TEST_KBD_PORT   	0x00ab
#define KBDC_DISABLE_KBD_PORT 	0x00ad
#define KBDC_ENABLE_KBD_PORT 	0x00ae

/* controller command byte (set by KBDC_SET_COMMAND_BYTE) */
#define KBD_TRANSLATION		0x0040
#define KBD_RESERVED_BITS	0x0004
#define KBD_OVERRIDE_KBD_LOCK	0x0008
#define KBD_ENABLE_KBD_PORT    	0x0000
#define KBD_DISABLE_KBD_PORT   	0x0010
#define KBD_ENABLE_AUX_PORT	0x0000
#define KBD_DISABLE_AUX_PORT	0x0020
#define KBD_ENABLE_AUX_INT	0x0002
#define KBD_DISABLE_AUX_INT	0x0000
#define KBD_ENABLE_KBD_INT     	0x0001
#define KBD_DISABLE_KBD_INT    	0x0000
#define KBD_KBD_CONTROL_BITS	(KBD_DISABLE_KBD_PORT | KBD_ENABLE_KBD_INT)
#define KBD_AUX_CONTROL_BITS	(KBD_DISABLE_AUX_PORT | KBD_ENABLE_AUX_INT)

/* keyboard device commands (sent to KBD_DATA_PORT) */
#define KBDC_RESET_KBD	     	0x00ff
#define KBDC_ENABLE_KBD		0x00f4
#define KBDC_DISABLE_KBD	0x00f5
#define KBDC_SET_DEFAULTS	0x00f6
#define KBDC_SEND_DEV_ID	0x00f2
#define KBDC_SET_LEDS		0x00ed
#define KBDC_ECHO		0x00ee
#define KBDC_SET_SCAN_CODESET	0x00f0
#define KBDC_SET_TYPEMATIC	0x00f3

/* aux device commands (sent to KBD_DATA_PORT) */
#define PSMC_RESET_DEV	     	0x00ff
#define PSMC_ENABLE_DEV      	0x00f4
#define PSMC_DISABLE_DEV     	0x00f5
#define PSMC_SEND_DEV_ID     	0x00f2
#define PSMC_SEND_DEV_STATUS 	0x00e9
#define PSMC_SEND_DEV_DATA	0x00eb
#define PSMC_SET_SCALING11	0x00e6
#define PSMC_SET_SCALING21	0x00e7
#define PSMC_SET_RESOLUTION	0x00e8
#define PSMC_SET_STREAM_MODE	0x00ea
#define PSMC_SET_REMOTE_MODE	0x00f0
#define PSMC_SET_SAMPLING_RATE	0x00f3

/* PSMC_SET_RESOLUTION argument */
#define PSMD_RESOLUTION_25	0	/* 25ppi */
#define PSMD_RESOLUTION_50	1	/* 50ppi */
#define PSMD_RESOLUTION_100	2	/* 100ppi (default after reset) */
#define PSMD_RESOLUTION_200	3	/* 200ppi */
/* FIXME: I don't know if it's possible to go beyond 200ppi. 
          The values below are of my wild guess. */
#define PSMD_RESOLUTION_400	4	/* 400ppi */
#define PSMD_RESOLUTION_800	5	/* 800ppi */
#define PSMD_MAX_RESOLUTION	PSMD_RESOLUTION_800

/* PSMC_SET_SAMPLING_RATE */
#define PSMD_MAX_RATE		255	/* FIXME: not sure if it's possible */

/* status bits (KBD_STATUS_PORT) */
#define KBDS_BUFFER_FULL	0x0021
#define KBDS_ANY_BUFFER_FULL	0x0001
#define KBDS_KBD_BUFFER_FULL	0x0001
#define KBDS_AUX_BUFFER_FULL	0x0021
#define KBDS_INPUT_BUFFER_FULL	0x0002

/* return code */
#define KBD_ACK 		0x00fa
#define KBD_RESEND		0x00fe
#define KBD_RESET_DONE		0x00aa
#define KBD_RESET_FAIL		0x00fc
#define KBD_DIAG_DONE		0x0055
#define KBD_DIAG_FAIL		0x00fd
#define KBD_ECHO		0x00ee

#define PSM_ACK 		0x00fa
#define PSM_RESEND		0x00fe
#define PSM_RESET_DONE		0x00aa
#define PSM_RESET_FAIL		0x00fc

/* aux device ID */
#define PSM_MOUSE_ID		0
#define PSM_BALLPOINT_ID	2

#ifdef KERNEL

/* driver specific options: the following options may be set by
   `options' statements in the kernel configuration file. */

/* retry count */
#ifndef KBD_MAXRETRY
#define KBD_MAXRETRY		3
#endif

/* timing parameters */
#ifndef KBD_RESETDELAY
#define KBD_RESETDELAY  	200     /* wait 200msec after kbd/mouse reset */
#endif
#ifndef KBD_MAXWAIT
#define KBD_MAXWAIT		5 	/* wait 5 times at most after reset */
#endif

/* debugging */
/* #define KBDIO_DEBUG			   produces debugging output */

/* end of driver specific options */

/* misc */
#ifndef TRUE
#define TRUE			(-1)
#endif
#ifndef FALSE
#define FALSE			0
#endif

/* function prototypes */

int wait_while_controller_busy __P((int port));

int wait_for_data __P((int port));
int wait_for_kbd_data __P((int port));
int wait_for_aux_data __P((int port));

int write_controller_command __P((int port,int c));
int write_controller_data __P((int port,int c));

int write_kbd_command __P((int port,int c));
int write_aux_command __P((int port,int c));
int send_kbd_command __P((int port,int c));
int send_aux_command __P((int port,int c));
int send_kbd_command_and_data __P((int port,int c,int d));
int send_aux_command_and_data __P((int port,int c,int d));

int read_controller_data __P((int port));
int read_kbd_data __P((int port));
int read_kbd_data_no_wait __P((int port));
int read_aux_data __P((int port));

void empty_kbd_buffer __P((int port, int t));
void empty_aux_buffer __P((int port, int t));
void empty_both_buffers __P((int port, int t));

int reset_kbd __P((int port));
int reset_aux_dev __P((int port));

int test_controller __P((int port));
int test_kbd_port __P((int port));
int test_aux_port __P((int port));

int set_controller_command_byte __P((int port,int command,int flag));

#endif /* KERNEL */

#endif /* !_I386_ISA_KBDIO_H_ */
