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
 * $FreeBSD$
 */

#ifndef _I386_ISA_KBDIO_H_
#define	_I386_ISA_KBDIO_H_

/* constants */

/* I/O ports */
#ifdef PC98
#define KBD_STATUS_PORT 	2	/* status port, read */
#define KBD_COMMAND_PORT	2	/* controller command port, write */
#define KBD_DATA_PORT		0	/* data port, read/write 
					 * also used as keyboard command
					 * and mouse command port 
					 */
#else
#define KBD_STATUS_PORT 	4	/* status port, read */
#define KBD_COMMAND_PORT	4	/* controller command port, write */
#define KBD_DATA_PORT		0	/* data port, read/write 
					 * also used as keyboard command
					 * and mouse command port 
					 */
#endif	/* PC98 */

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
#define KBDC_SET_SCANCODE_SET	0x00f0
#define KBDC_SET_TYPEMATIC	0x00f3

/* aux device commands (sent to KBD_DATA_PORT) */
#define PSMC_RESET_DEV	     	0x00ff
#define PSMC_ENABLE_DEV      	0x00f4
#define PSMC_DISABLE_DEV     	0x00f5
#define PSMC_SET_DEFAULTS	0x00f6
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
#define PSMD_RES_LOW		0	/* typically 25ppi */
#define PSMD_RES_MEDIUM_LOW	1	/* typically 50ppi */
#define PSMD_RES_MEDIUM_HIGH	2	/* typically 100ppi (default) */
#define PSMD_RES_HIGH		3	/* typically 200ppi */
#define PSMD_MAX_RESOLUTION	PSMD_RES_HIGH

/* PSMC_SET_SAMPLING_RATE */
#define PSMD_MAX_RATE		255	/* FIXME: not sure if it's possible */

/* status bits (KBD_STATUS_PORT) */
#ifdef PC98
#define KBDS_BUFFER_FULL	0x0002
#define KBDS_ANY_BUFFER_FULL	0x0002
#define KBDS_KBD_BUFFER_FULL	0x0002
#define KBDS_AUX_BUFFER_FULL	0x0002
#else
#define KBDS_BUFFER_FULL	0x0021
#define KBDS_ANY_BUFFER_FULL	0x0001
#define KBDS_KBD_BUFFER_FULL	0x0001
#define KBDS_AUX_BUFFER_FULL	0x0021
#endif
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

#ifndef TRUE
#define TRUE			1
#endif
#ifndef FALSE
#define FALSE			0
#endif

/* types/structures */

typedef caddr_t KBDC;

/* function prototypes */

KBDC kbdc_open __P((int port));

int kbdc_lock __P((KBDC kbdc, int lock));

int kbdc_data_ready __P((KBDC kbdc));

int write_controller_command __P((KBDC kbdc,int c));
int write_controller_data __P((KBDC kbdc,int c));

int write_kbd_command __P((KBDC kbdc,int c));
int write_aux_command __P((KBDC kbdc,int c));
int send_kbd_command __P((KBDC kbdc,int c));
int send_aux_command __P((KBDC kbdc,int c));
int send_kbd_command_and_data __P((KBDC kbdc,int c,int d));
int send_aux_command_and_data __P((KBDC kbdc,int c,int d));

int read_controller_data __P((KBDC kbdc));
int read_kbd_data __P((KBDC kbdc));
int read_kbd_data_no_wait __P((KBDC kbdc));
int read_aux_data __P((KBDC kbdc));
int read_aux_data_no_wait __P((KBDC kbdc));

void empty_kbd_buffer __P((KBDC kbdc, int t));
void empty_aux_buffer __P((KBDC kbdc, int t));
void empty_both_buffers __P((KBDC kbdc, int t));

int reset_kbd __P((KBDC kbdc));
int reset_aux_dev __P((KBDC kbdc));

int test_controller __P((KBDC kbdc));
int test_kbd_port __P((KBDC kbdc));
int test_aux_port __P((KBDC kbdc));

int kbdc_get_device_mask __P((KBDC kbdc));
void kbdc_set_device_mask __P((KBDC kbdc, int mask));

int get_controller_command_byte __P((KBDC kbdc));
int set_controller_command_byte __P((KBDC kbdc, int command, int flag));

#endif /* KERNEL */

#endif /* !_I386_ISA_KBDIO_H_ */
