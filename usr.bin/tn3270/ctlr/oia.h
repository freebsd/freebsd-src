/*-
 * Copyright (c) 1988 The Regents of the University of California.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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
 *	@(#)oia.h	4.2 (Berkeley) 4/26/91
 */

/*
 * This file describes the Operator Information Area in the 3270.
 *
 * Our OIA looks like that used by the 3270 PC and PC 3270 products.
 */

#define	INCLUDED_OIA

typedef struct {
    char
	online_ownership,
	character_selection,
	shift_state,
	pss_group_1,
	highlight_group_1,
	color_group_1,
	insert,
	input_inhibited[5],
	pss_group_2,
	highlight_group_2,
	color_group_2,
	comm_error_reminder,
	printer_status,
	reserved_group_14,
	reserved_group_15,
	autokey_play_record_status,
	autokey_abort_pause_status,
	enlarge_state;
} OIA;

/* Bits in online_ownership */
#define	OIA_SETUP		0x80
#define	OIA_TEST		0x40
#define	OIA_SSCP_LU		0x20
#define	OIA_LU_LU		0x10
#define	OIA_UNOWNED		0x08
#define	OIA_SUBSYSTEM_READY	0x04

/* Bit in character_selection */
#define	OIA_EXTENDED_SELECT	0x80
#define	OIA_APL			0x40
#define	OIA_KANA		0x20
#define	OIA_ALPHA		0x10
#define	OIA_TEXT		0x08

/* Bits in shift_state */
#define	OIA_NUMERIC		0x80
#define	OIA_UPPER_SHIFT		0x40

/* Bits in pss_group_1, highlight_group_1, and color_group_1 */
#define	OIA_SELECTABLE		0x80
#define	OIA_FIELD_INHERIT	0x40

/* Bits in insert */
#define	OIA_INSERT_MODE		0x80

/* We define this to be a 'long' followed by a 'char' (5 bytes) */

#define	OIA_NON_RESETTABLE	0x80
#define	OIA_SECURITY_KEY	0x40
#define	OIA_MACHINE_CHECK	0x20
#define	OIA_COMM_CHECK		0x10
#define	OIA_PROGRAM_CHECK	0x08
#define	OIA_RETRY		0x04
#define	OIA_DEVICE_NOT_WORKING	0x02
#define	OIA_DEVICE_VERY_BUSY	0x01

#define	OIA_DEVICE_BUSY		  0x80
#define	OIA_TERMINAL_WAIT	  0x40
#define	OIA_MINUS_SYMBOL	  0x20
#define	OIA_MINUS_FUNCTION	  0x10
#define	OIA_TOO_MUCH_ENTERED	  0x08
#define	OIA_NOT_ENOUGH_ENTERED	  0x04
#define	OIA_WRONG_NUMBER	  0x02
#define	OIA_NUMERIC_FIELD	  0x01

#define	OIA_OP_UNAUTHORIZED	    0x80
#define	OIA_OP_UNAUTHORIZED_MIN	    0x40
#define	OIA_INVALID_DEAD_KEY_COMBO  0x20
#define	OIA_WRONG_PLACE		    0x10

#define	OIA_MESSAGE_PENDING	      0x80
#define	OIA_PARTITION_WAIT	      0x40
#define	OIA_SYSTEM_WAIT		      0x20
#define	OIA_HARDWARE_MISMATCH	      0x10
#define	OIA_LOGICAL_TERM_NOT_CONF     0x08


#define	OIA_AUTOKEY_INHIBIT	        0x80
#define	OIA_API_INHIBIT		        0x40

/* Bits in pss_group_2 */
#define	OIA_PS_SELECTED		0x80
#define	OIA_PC_DISPLAY_DISABLE	0x40

/* Bits in highlight_group_2 and color_group_2 */
#define	OIA_SELECTED		0x80

/* Bits in comm_error_reminder */
#define	OIA_COMM_ERROR		0x80
#define	OIA_RTM			0x40

/* Bits in printer_status */
#define	OIA_PRINT_NOT_CUSTOM	0x80
#define	OIA_PRINTER_MALFUNCTION	0x40
#define	OIA_PRINTER_PRINTING	0x20
#define	OIA_ASSIGN_PRINTER	0x10
#define	OIA_WHAT_PRINTER	0x08
#define	OIA_PRINTER_ASSIGNMENT	0x04

/* Bits in autokey_play_record_status */
#define	OIA_PLAY		0x80
#define	OIA_RECORD		0x40

/* Bits in autokey_abort_pause_status */
#define	OIA_RECORDING_OVERFLOW	0x80
#define	OIA_PAUSE		0x40

/* Bits in enlarge_state */
#define	OIA_WINDOW_IS_ENLARGED	0x80

/* Define functions to set and read the oia */

#define	SetOiaOnlineA(oia) SetOiaMyJob((oia))		/* Side-effect */
#define	ResetOiaOnlineA(oia) \
	/* Nothing defined for this */

#define	IsOiaReady3274(oia)	((oia)->online_ownership&OIA_SUBSYSTEM_READY)
#define	ResetOiaReady3274(oia)	(oia)->online_ownership &= ~OIA_SUBSYSTEM_READY
#define	SetOiaReady3274(oia)	(oia)->online_ownership |= OIA_SUBSYSTEM_READY

#define	IsOiaMyJob(oia)		((oia)->online_ownership&OIA_LU_LU)
#define	ResetOiaMyJob(oia)	(oia)->online_ownership &= ~OIA_LU_LU
#define	SetOiaMyJob(oia)	(oia)->online_ownership |= OIA_LU_LU

#define	IsOiaInsert(oia)	((oia)->online_ownership&OIA_INSERT_MODE)
#define	ResetOiaInsert(oia)	(oia)->online_ownership &= ~OIA_INSERT_MODE
#define	SetOiaInsert(oia)	(oia)->online_ownership |= OIA_INSERT_MODE

#define	IsOiaSystemLocked(oia)	((oia)->input_inhibited[3]&OIA_SYSTEM_WAIT)
#define	ResetOiaSystemLocked(oia) \
				(oia)->input_inhibited[3] &= ~OIA_SYSTEM_WAIT
#define	SetOiaSystemLocked(oia)	(oia)->input_inhibited[3] |= OIA_SYSTEM_WAIT

#define	IsOiaTWait(oia)		((oia)->input_inhibited[1]&OIA_TERMINAL_WAIT)
#define	ResetOiaTWait(oia)	(oia)->input_inhibited[1] &= ~OIA_TERMINAL_WAIT
#define	SetOiaTWait(oia)	(oia)->input_inhibited[1] |= OIA_TERMINAL_WAIT

#define	IsOiaApiInhibit(oia)	((oia)->input_inhibited[4] &   OIA_API_INHIBIT)
#define	ResetOiaApiInhibit(oia)	((oia)->input_inhibited[4] &= ~OIA_API_INHIBIT)
#define	SetOiaApiInhibit(oia)	((oia)->input_inhibited[4] |=  OIA_API_INHIBIT)

/* A macro to let the world know that someone has modified the OIA. */
#define	SetOiaModified()	oia_modified = 1
#define	SetPsModified()		ps_modified = 1
