/*-
 * Copyright (c) 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	@(#)api.h	8.1 (Berkeley) 6/6/93
 */

/*
 * This file contains header information used by the PC API routines.
 */

#if	!defined(MSDOS)
#define far			/* For 'far *' checks */
#endif	/* !defined(MSDOS) */

#define	API_INTERRUPT_NUMBER	0x7A		/* API Interrupt Number */

/*
 * Define the gate numbers.  These are returned via the Name Resolution
 * service.
 */

#define	GATE_SESSMGR	1234
#define	GATE_KEYBOARD	5678
#define	GATE_COPY	9101
#define	GATE_OIAM	1121

/*
 * The names which correspond to the above gate numbers.
 */

#define	NAME_SESSMGR	"SESSMGR "
#define	NAME_KEYBOARD	"KEYBOARD"
#define	NAME_COPY	"COPY    "
#define	NAME_OIAM	"OIAM    "


/*
 * Name Resolution is specified in AH.
 */

#define	NAME_RESOLUTION		0x81

#if	defined(unix)
/*
 * In unix, we offer a service to allow the application to keep from
 * having to poll us constantly.
 */
#define	PS_OR_OIA_MODIFIED	0x99

#endif	/* defined(unix) */

/*
 * Codes specified in AL for various services.
 */

#define	QUERY_SESSION_ID		0x01
#define	QUERY_SESSION_PARAMETERS	0x02
#define	QUERY_SESSION_CURSOR		0x0b

#define	CONNECT_TO_KEYBOARD		0x01
#define	DISCONNECT_FROM_KEYBOARD	0x02
#define	WRITE_KEYSTROKE			0x04
#define	DISABLE_INPUT			0x05
#define	ENABLE_INPUT			0x06

#define	COPY_STRING			0x01

#define	READ_OIA_GROUP			0x02

/*
 * For each service, we define the assoicated parameter blocks.
 */

/*
 * Supervisor Services
 */

typedef struct {
    char	gate_name[8];
} NameResolveParms;


/*
 * Session Information Services
 */

typedef struct {
    char
	short_name,
	type,
	session_id,
	reserved,
	long_name[8];
} NameArrayElement;

typedef struct {
    unsigned char
	length,
	number_matching_session;
    NameArrayElement
	name_array_element;		/* Variable number */
} NameArray;

typedef struct {
    char
	rc,
	function_id,
	option_code,
	data_code;
    NameArray far
	*name_array;
    char
	long_name[8];
} QuerySessionIdParms;

#define	ID_OPTION_BY_NAME	0x01		/* By short (or long) name */
#define	ID_OPTION_ALL		0x00		/* All (of specified type */

typedef struct {
    char
	rc,
	function_id,
	session_id,
	reserved,
	session_type,
	session_characteristics,
	rows,
	columns;
    char far
	*presentation_space;
} QuerySessionParametersParms;

#define	TYPE_WSCTL		0x01		/* Work Station Control */
#define	TYPE_DFT		0x02		/* DFT Host Session */
#define	TYPE_CUT		0x03		/* CUT Host Session */
#define	TYPE_NOTEPAD		0x04		/* Notepad Session */
#define	TYPE_PC			0x05		/* Personal Computer Session */

#define	CHARACTERISTIC_EAB	0x80		/* Extended Attribute Buffer */
#define	CHARACTERISTIC_PSS	0x40		/* Program Symbols Supported */

typedef struct {
    char
	rc,
	function_id,
	session_id,
	cursor_type,
	row_address,				/* from 0 */
	column_address;				/* from 0 */
} QuerySessionCursorParms;

#define	CURSOR_INHIBITED_AUTOSCROLL	0x10
#define	CURSOR_INHIBITED		0x04
#define	CURSOR_BLINKING			0x02
#define	CURSOR_BOX			0x01
typedef struct {
    char
	rc,
	function_id,
	session_id,
	reserved;
    short
	event_queue_id,
	input_queue_id;
    char
	intercept_options,
	first_connection_identifier;
} ConnectToKeyboardParms;

typedef struct {
    char
	rc,
	function_id,
	session_id,
	reserved;
    short
	connectors_task_id;
} DisconnectFromKeyboardParms;

typedef struct {
    unsigned char
	scancode,
	shift_state;
} KeystrokeEntry;

typedef struct {
    short
	length;			/* Length (in bytes) of list */
    KeystrokeEntry keystrokes;	/* Variable size */
} KeystrokeList;

typedef struct {
    char
	rc,
	function_id,
	session_id,
	reserved;
    short
	connectors_task_id;
    char
	options,
	number_of_keys_sent;
    union {
	KeystrokeEntry
	    keystroke_entry;
	KeystrokeList far
	    *keystroke_list;
    } keystroke_specifier;
} WriteKeystrokeParms;

#define	OPTION_SINGLE_KEYSTROKE		0x20
#define	OPTION_MULTIPLE_KEYSTROKES	0x30

typedef struct {
    char
	rc,
	function_id,
	session_id,
	reserved;
    short
	connectors_task_id;
} DisableInputParms;

typedef DisableInputParms EnableInputParms;

typedef struct {
    char
	session_id,
	reserved;
    char far
	*buffer;
    char
	characteristics,
	session_type;
    short
	begin;			/* Offset within buffer */
} BufferDescriptor;
    
typedef struct {
    char
	rc,
	function_id;
    BufferDescriptor
	source;
    short
	source_end;		/* Offset within source buffer */
    BufferDescriptor
	target;
    char
	copy_mode,
	reserved;
} CopyStringParms;

#define	COPY_MODE_7_COLOR		0x80	/* Else 4 color mode */
#define	COPY_MODE_FIELD_ATTRIBUTES	0x40	/* Else don't copy attributes */

typedef struct {
    char
	rc,
	function_id,
	session_id,
	reserved;
    char far
	*oia_buffer;
    char
	oia_group_number;
} ReadOiaGroupParms;

/* If the user wants all groups, we return API_OIA_BYTES_ALL_GROUPS bytes */
#define	API_OIA_ALL_GROUPS		'\377'
#define	API_OIA_BYTES_ALL_GROUPS	22	/* 22 bytes of data */

/* API_OIA_INPUT_INHIBITED is special.  It returns more than on byte of data */
#define	API_OIA_INPUT_INHIBITED		8

#define	API_OIA_LAST_LEGAL_GROUP	18	/* Highest legal number */



#if	defined(MSDOS)

#if	!defined(FP_SEG)
#include <dos.h>
#endif	/* !defined(FP_SEG) */

#else	/* defined(MSDOS) */

/*
 * These definitions are here to provide the descriptions of
 * some registers which are, normally, defined in <dos.h> on
 * a dos system.
 */

#define	FP_SEG(x)	((unsigned int)(((unsigned long)(x))>>16))
#define	FP_OFF(y)	((unsigned int)(((unsigned long)(y))&0xFFFF))

/*
 * Undo the preceeding.
 */

#define	SEG_OFF_BACK(x,y)	(((x)<<16)|(y))

/*
 * Now, it is somewhat of a pain, but we need to keep
 * 8086 conventions about which of the "highlow"'s map
 * into which of the "words".
 */

#include <sys/param.h>		/* Get ENDIAN from machine/endian.h */

/* Determine endian'ess (if necessary) */

#if	!(defined(BYTE_ORDER) && defined(BIG_ENDIAN))
#define	LITTLE_ENDIAN	1234	/* least-significant byte first (vax) */
#define	BIG_ENDIAN	4321	/* most-significant byte first (IBM, net) */

#if	defined(vax) || defined(ns32000) || defined(i386) || (defined(mips)&&defined(MIPSEL))
#define	BYTE_ORDER	LITTLE_ENDIAN
#endif	/* defined(vax) || defined(ns32000) */ 

#if	defined(sun) || defined(tahoe) || defined(ibm032) || defined(pyr) || defined(gould) || (defined(mips)&&defined(MIPSEB))
#define	BYTE_ORDER	BIG_ENDIAN
#endif	/* defined(sun) || defined(tahoe) || defined(ibm032) || defined(pyr) || defined(gould) */

#endif	/* !(defined(BYTE_ORDER) && defined(BIG_ENDIAN)) */

struct highlow {
    unsigned char
#if	BYTE_ORDER == LITTLE_ENDIAN
	al,
	ah,
	bl,
	bh,
	cl,
	ch,
	dl,
	dh;
#endif	/* BYTE_ORDER == LITTLE_ENDIAN */
#if	BYTE_ORDER == BIG_ENDIAN
	ah,
	al,
	bh,
	bl,
	ch,
	cl,
	dh,
	dl;
#endif	/* BYTE_ORDER == BIG_ENDIAN */
};

struct words {
    unsigned short
	ax,
	bx,
	cx,
	dx;
    unsigned short
	si,
	di;
};

union REGS {
    struct highlow h;
    struct words x;
};

struct SREGS {
    unsigned short
	cs,
	ds,
	es,
	ss;
};
#endif	/* defined(MSDOS) (else section) */
