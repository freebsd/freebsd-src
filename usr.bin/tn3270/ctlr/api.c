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
 */

#ifndef lint
static char sccsid[] = "@(#)api.c	8.1 (Berkeley) 6/6/93";
#endif /* not lint */

/*
 * This file implements the API used in the PC version.
 */

#include <stdio.h>

#include "api.h"
#include "../general/general.h"

#include "../api/disp_asc.h"

#include "screen.h"
#include "hostctlr.h"
#include "oia.h"

#include "../general/globals.h"

int apitrace = 0;

/*
 * Some defines for things we use internally.
 */

#define	PS_SESSION_ID	23
#define	BUF_SESSION_ID	0

/*
 * General utility routines.
 */

#if	defined(MSDOS)

#if	defined(LINT_ARGS)
static void movetous(char *, int, int, int);
static void movetothem(int, int, char *, int);
#endif	/* defined(LINT_ARGS) */

#define	access_api(foo,length,copyin)	(foo)
#define	unaccess_api(foo,goo,length,copyout)

static void
movetous(parms, es, di, length)
char *parms;
int es, di;
int length;
{
    char far *farparms = parms;

    movedata(es, di, FP_SEG(farparms), FP_OFF(farparms), length);
    if (apitrace) {
	Dump('(', parms, length);
    }
}

static void
movetothem(es, di, parms, length)
int es, di;
char *parms;
int length;
{
    char far *farparms = parms;

    movedata(FP_SEG(farparms), FP_OFF(farparms), es, di, length);
    if (apitrace) {
	Dump(')', parms, length);
    }
}
#endif	/* defined(MSDOS) */

#if	defined(unix)
extern char *access_api();
extern void movetous(), movetothem(), unaccess_api();
#endif	/* defined(unix) */


/*
 * Supervisor Services.
 */

static void
name_resolution(regs, sregs)
union REGS *regs;
struct SREGS *sregs;
{
    NameResolveParms parms;

    movetous((char *) &parms, sregs->es, regs->x.di, sizeof parms);

    regs->h.cl = 0;
    if (memcmp((char *)&parms, NAME_SESSMGR, sizeof parms.gate_name) == 0) {
	regs->x.dx = GATE_SESSMGR;
    } else if (memcmp((char *)&parms, NAME_KEYBOARD,
					sizeof parms.gate_name) == 0) {
	regs->x.dx = GATE_KEYBOARD;
    } else if (memcmp((char *)&parms, NAME_COPY, sizeof parms.gate_name) == 0) {
	regs->x.dx = GATE_COPY;
    } else if (memcmp((char *)&parms, NAME_OIAM, sizeof parms.gate_name) == 0) {
	regs->x.dx = GATE_OIAM;
    } else {
	regs->h.cl = 0x2e;	/* Name not found */
    }
    regs->h.ch = 0x12;
    regs->h.bh = 7;
}

/*
 * Session Information Services.
 */

static void
query_session_id(regs, sregs)
union REGS *regs;
struct SREGS *sregs;
{
    QuerySessionIdParms parms;

    movetous((char *)&parms, sregs->es, regs->x.di, sizeof parms);

    if ((parms.rc != 0) || (parms.function_id != 0)) {
	parms.rc = 0x0c;
    } else if (parms.option_code != 0x01) {
	parms.rc = 0x0d;	/* Invalid option code */
#ifdef	NOTOBS
    } else if ((parms.data_code != 0x45) && (parms.data_code != 0x00/*OBS*/)) {
	parms.rc = 0x0b;
#endif	/* NOTOBS */
    } else {
	NameArray list;

	movetous((char *)&list, FP_SEG(parms.name_array),
		    FP_OFF(parms.name_array), sizeof list);
	if ((list.length < 14) || (list.length > 170)) {
	    parms.rc = 0x12;
	} else {
	    list.number_matching_session = 1;
	    list.name_array_element.short_name = parms.data_code;
	    list.name_array_element.type = TYPE_DFT;
	    list.name_array_element.session_id = PS_SESSION_ID;
	    memcpy(list.name_array_element.long_name, "ONLYSESS",
			    sizeof list.name_array_element.long_name);
	    movetothem(FP_SEG(parms.name_array),
		FP_OFF(parms.name_array), (char *)&list, sizeof list);
	    parms.rc = 0;
	}
    }
    parms.function_id = 0x6b;
    movetothem(sregs->es, regs->x.di, (char *)&parms, sizeof parms);
}

static void
query_session_parameters(regs, sregs)
union REGS *regs;
struct SREGS *sregs;
{
    QuerySessionParametersParms parms;

    movetous((char *)&parms, sregs->es, regs->x.di, sizeof parms);

    if ((parms.rc !=0) || (parms.function_id != 0)) {
	parms.rc = 0x0c;
    } else if (parms.session_id != PS_SESSION_ID) {
	parms.rc = 0x02;
    } else {
	parms.rc = 0;
	parms.session_type = TYPE_DFT;
	parms.session_characteristics = 0;	/* Neither EAB nor PSS */
	parms.rows = MaxNumberLines;
	parms.columns = MaxNumberColumns;
	parms.presentation_space = 0;
    }
    parms.function_id = 0x6b;
    movetothem(sregs->es, regs->x.di, (char *)&parms, sizeof parms);
}

static void
query_session_cursor(regs, sregs)
union REGS *regs;
struct SREGS *sregs;
{
    QuerySessionCursorParms parms;

    movetous((char *)&parms, sregs->es, regs->x.di, sizeof parms);

    if ((parms.rc != 0) || (parms.function_id != 0)) {
	parms.rc = 0x0c;
    } else if (parms.session_id != PS_SESSION_ID) {
	parms.rc = 0x02;
    } else {
	parms.rc = 0;
	parms.cursor_type = CURSOR_BLINKING;	/* XXX what is inhibited? */
	parms.row_address = ScreenLine(CursorAddress);
	parms.column_address = ScreenLineOffset(CursorAddress);
    }

    parms.function_id = 0x6b;
    movetothem(sregs->es, regs->x.di, (char *) &parms, sizeof parms);
}

/*
 * Keyboard Services.
 */


static void
connect_to_keyboard(regs, sregs)
union REGS *regs;
struct SREGS *sregs;
{
    ConnectToKeyboardParms parms;

    movetous((char *)&parms, sregs->es, regs->x.di, sizeof parms);

    if ((parms.rc != 0) || (parms.function_id != 0)) {
	parms.rc = 0x0c;
    } else if (parms.session_id != PS_SESSION_ID) {
	parms.rc = 0x02;
    } else if (parms.intercept_options != 0) {
	parms.rc = 0x01;
    } else {
	parms.rc = 0;
	parms.first_connection_identifier = 0;
    }
    parms.function_id = 0x62;

    movetothem(sregs->es, regs->x.di, (char *)&parms, sizeof parms);
}

static void
disconnect_from_keyboard(regs, sregs)
union REGS *regs;
struct SREGS *sregs;
{
    DisconnectFromKeyboardParms parms;

    movetous((char *)&parms, sregs->es, regs->x.di, sizeof parms);

    if ((parms.rc != 0) || (parms.function_id != 0)) {
	parms.rc = 0x0c;
    } else if (parms.session_id != PS_SESSION_ID) {
	parms.rc = 0x02;
    } else if (parms.connectors_task_id != 0) {
	parms.rc = 04;			/* XXX */
    } else {
	parms.rc = 0;
    }
    parms.function_id = 0x62;

    movetothem(sregs->es, regs->x.di, (char *)&parms, sizeof parms);
}

static void
write_keystroke(regs, sregs)
union REGS *regs;
struct SREGS *sregs;
{
    WriteKeystrokeParms parms;

    movetous((char *)&parms, sregs->es, regs->x.di, sizeof parms);

    if ((parms.rc != 0) || (parms.function_id != 0)) {
	parms.rc = 0x0c;
    } else if (parms.session_id != PS_SESSION_ID) {
	parms.rc = 0x02;
    } else if (parms.connectors_task_id != 0) {
	parms.rc = 0x04;
    } else {
	parms.number_of_keys_sent = 0;
	parms.rc = 0;
	if (parms.options == OPTION_SINGLE_KEYSTROKE) {
	    KeystrokeEntry *entry = &parms.keystroke_specifier.keystroke_entry;

	    if (AcceptKeystroke(entry->scancode, entry->shift_state) == 0) {
		parms.rc = 0x10;		/* XXX needs 0x12 too! */
	    }
	    parms.number_of_keys_sent++;
	} else if (parms.options == OPTION_MULTIPLE_KEYSTROKES) {
	    KeystrokeList
		list,
		far *atlist = parms.keystroke_specifier.keystroke_list;
	    KeystrokeEntry
		entry[10],		/* 10 at a time */
		*ourentry,
		far *theirentry;
	    int
		todo;

	    movetous((char *)&list, FP_SEG(atlist),
			FP_OFF(atlist), sizeof *atlist);
	    todo = list.length/2;
	    ourentry = entry+(highestof(entry)+1);
	    theirentry = &atlist->keystrokes;

	    while (todo) {
		if (ourentry > &entry[highestof(entry)]) {
		    int thistime;

		    thistime = todo;
		    if (thistime > numberof(entry)) {
			thistime = numberof(entry);
		    }
		    movetous((char *)entry, FP_SEG(theirentry),
			    FP_OFF(theirentry), thistime*sizeof *theirentry);
		    theirentry += thistime;
		    ourentry = entry;
		}
		if (AcceptKeystroke(ourentry->scancode,
						ourentry->shift_state) == 0) {
		    parms.rc = 0x10;		/* XXX needs 0x12 too! */
		    break;
		}
		parms.number_of_keys_sent++;
		ourentry++;
		todo--;
	    }
	} else {
	    parms.rc = 0x01;
	}
    }
    parms.function_id = 0x62;

    movetothem(sregs->es, regs->x.di, (char *)&parms, sizeof parms);
/* XXX */
}


static void
disable_input(regs, sregs)
union REGS *regs;
struct SREGS *sregs;
{
    DisableInputParms parms;

    movetous((char *)&parms, sregs->es, regs->x.di, sizeof parms);

    if ((parms.rc != 0) || (parms.function_id != 0)) {
	parms.rc = 0x0c;
    } else if (parms.session_id != PS_SESSION_ID) {
	parms.rc = 0x02;
    } else if (parms.connectors_task_id != 0) {
	parms.rc = 0x04;
    } else {
	SetOiaApiInhibit(&OperatorInformationArea);
	parms.rc = 0;
    }
    parms.function_id = 0x62;

    movetothem(sregs->es, regs->x.di, (char *)&parms, sizeof parms);
}

static void
enable_input(regs, sregs)
union REGS *regs;
struct SREGS *sregs;
{
    EnableInputParms parms;

    movetous((char *)&parms, sregs->es, regs->x.di, sizeof parms);

    if ((parms.rc != 0) || (parms.function_id != 0)) {
	parms.rc = 0x0c;
    } else if (parms.session_id != PS_SESSION_ID) {
	parms.rc = 0x02;
    } else if (parms.connectors_task_id != 0) {
	parms.rc = 0x04;
    } else {
	ResetOiaApiInhibit(&OperatorInformationArea);
	parms.rc = 0;
    }
    parms.function_id = 0x62;

    movetothem(sregs->es, regs->x.di, (char *)&parms, sizeof parms);
}

/*
 * Copy Services.
 */

static
copy_subroutine(target, source, parms, what_is_user, length)
BufferDescriptor *target, *source;
CopyStringParms *parms;
int what_is_user;
#define	USER_IS_TARGET	0
#define	USER_IS_SOURCE	1
{
#define	TARGET_NO_EAB		1
#define	SOURCE_NO_EAB		2
#define	TARGET_PC		4
#define	SOURCE_PC		8
#define	NO_FIELD_ATTRIBUTES	16
    int needtodo = 0;
    int access_length;
    char far *input;
    char far *output;
    char far *access_pointer;

    if ((target->characteristics^source->characteristics)
		    &CHARACTERISTIC_EAB) {
	if (target->characteristics&CHARACTERISTIC_EAB) {
	    needtodo |= TARGET_NO_EAB;	/* Need to bump for EAB in target */
	} else {
	    needtodo |= SOURCE_NO_EAB;	/* Need to bump for EAB in source */
	}
    }
    if (target->session_type != source->session_type) {
	if (target->session_type == TYPE_PC) {
	    needtodo |= TARGET_PC;	/* scan codes to PC */
	} else {
	    needtodo |= SOURCE_PC;	/* PC to scan codes */
	}
    }
    if ((parms->copy_mode&COPY_MODE_FIELD_ATTRIBUTES) == 0) {
	needtodo |= NO_FIELD_ATTRIBUTES;
    }
    access_length = length;
    if (what_is_user == USER_IS_TARGET) {
	if (target->characteristics&CHARACTERISTIC_EAB) {
	    access_length *= 2;
	}
	input = (char far *) &Host[source->begin];
	access_pointer = target->buffer;
	output = access_api(target->buffer, access_length, 0);
    } else {
	if (source->characteristics&CHARACTERISTIC_EAB) {
	    access_length *= 2;
	}
	access_pointer = source->buffer;
	input = access_api(source->buffer, access_length, 1);
	output = (char far *) &Host[target->begin];
    }
    while (length--) {
	if (needtodo&TARGET_PC) {
	    *output++ = disp_asc[*input++];
	} else if (needtodo&SOURCE_PC) {
	    *output++ = asc_disp[*input++];
	} else {
	    *output++ = *input++;
	}
	if (needtodo&TARGET_NO_EAB) {
	    input++;
	} else if (needtodo&SOURCE_NO_EAB) {
	    *output++ = 0;		/* Should figure out good EAB? */
	}
    }
    if (what_is_user == USER_IS_TARGET) {
	unaccess_api(target->buffer, access_pointer, access_length, 1);
    } else {
	unaccess_api(source->buffer, access_pointer, access_length, 0);
    }
}


static void
copy_string(regs, sregs)
union REGS *regs;
struct SREGS *sregs;
{
    CopyStringParms parms;
    BufferDescriptor *target = &parms.target, *source = &parms.source;
    int length;

    movetous((char *)&parms, sregs->es, regs->x.di, sizeof parms);

    length = 1+parms.source_end-source->begin;
    if ((parms.rc != 0) || (parms.function_id !=0)) {
	parms.rc = 0x0c;
    } else if (target->session_id == BUF_SESSION_ID) {	/* Target is buffer */
	if (source->session_id != PS_SESSION_ID) {		/* A no-no */
	    parms.rc = 0x2;
	} else {
	    if ((source->begin < 0) || (source->begin > highestof(Host))) {
		parms.rc = 0x06;		/* invalid source definition */
	    } else {
		if ((source->begin+length) > highestof(Host)) {
		    length = highestof(Host)-source->begin;
		    parms.rc = 0x0f;	/* Truncate */
		}
	        if ((source->characteristics == target->characteristics) &&
		    (source->session_type == target->session_type)) {
		    if (source->characteristics&CHARACTERISTIC_EAB) {
			length *= 2;
		    }
		    movetothem(FP_SEG(target->buffer),
			    FP_OFF(target->buffer),
			    (char *)&Host[source->begin], length);
		} else {
		    copy_subroutine(target, source, &parms,
							USER_IS_TARGET, length);
		}
	    }
	}
    } else if (source->session_id != BUF_SESSION_ID) {
	    parms.rc = 0xd;
    } else {
	/* Send to presentation space (3270 buffer) */
	if ((target->begin < 0) || (target->begin > highestof(Host))) {
	    parms.rc = 0x07;		/* invalid target definition */
	} if (!UnLocked) {
		parms.rc = 0x03;	/* Keyboard locked */
	} else if (parms.copy_mode != 0) {
		parms.rc = 0x0f;	/* Copy of field attr's not allowed */
	} else if (IsProtected(target->begin) || /* Make sure no protected */
		    (WhereAttrByte(target->begin) !=	/* in range */
			    WhereAttrByte(target->begin+length-1))) {
		parms.rc = 0x0e;	/* Attempt to write in protected */
	} else {
	    if ((target->begin+length) > highestof(Host)) {
		length = highestof(Host)-target->begin;
		parms.rc = 0x0f;	/* Truncate */
	    }
	    TurnOnMdt(target->begin);	/* Things have changed */
	    if ((source->characteristics == target->characteristics) &&
		    (source->session_type == target->session_type)) {
		if (source->characteristics&CHARACTERISTIC_EAB) {
		    length *= 2;
		}
		movetous((char *)&Host[target->begin],
			    FP_SEG(source->buffer),
			    FP_OFF(source->buffer), length);
	    } else {
		copy_subroutine(target, source, &parms, USER_IS_SOURCE, length);
	    }
	}
    }
    parms.function_id = 0x64;
    movetothem(sregs->es, regs->x.di, (char *)&parms, sizeof parms);
}


/*
 * Operator Information Area Services.
 */

static void
read_oia_group(regs, sregs)
union REGS *regs;
struct SREGS *sregs;
{
    ReadOiaGroupParms parms;

    movetous((char *)&parms, sregs->es, regs->x.di, sizeof parms);

    if ((parms.rc != 0) || (parms.function_id != 0)) {
	parms.rc = 0x0c;
    } else if (parms.session_id != PS_SESSION_ID) {
	parms.rc = 0x02;
    } else {
	int group = parms.oia_group_number;
	char *from;
	int size;

	if ((group != API_OIA_ALL_GROUPS) &&
		((group > API_OIA_LAST_LEGAL_GROUP) || (group < 0))) {
	} else {
	    if (group == API_OIA_ALL_GROUPS) {
		size = API_OIA_BYTES_ALL_GROUPS;
		from = (char *)&OperatorInformationArea;
	    } else if (group == API_OIA_INPUT_INHIBITED) {
		size = sizeof OperatorInformationArea.input_inhibited;
		from = (char *)&OperatorInformationArea.input_inhibited[0];
	    } else {
		size = 1;
		from = ((char *)&OperatorInformationArea)+group;
	    }
	    movetothem(FP_SEG(parms.oia_buffer), FP_OFF(parms.oia_buffer),
			from, size);
	}
    }
    parms.function_id = 0x6d;
    movetothem(sregs->es, regs->x.di, (char *)&parms, sizeof parms);
}

/*ARGSUSED*/
static void
unknown_op(regs, sregs)
union REGS *regs;
struct SREGS *sregs;
{
    regs->h.ch = 0x12;
    regs->h.cl = 0x05;
}


handle_api(regs, sregs)
union REGS *regs;
struct SREGS *sregs;
{
/*
 * Do we need to log this transaction?
 */
    if (apitrace) {
	Dump('<', (char *)regs, sizeof *regs);
	Dump('<', (char *)sregs, sizeof *sregs);
    }
    if (regs->h.ah == NAME_RESOLUTION) {
	name_resolution(regs, sregs);
#if	defined(unix)
    } else if (regs->h.ah == PS_OR_OIA_MODIFIED) {
	while ((oia_modified == 0) && (ps_modified == 0)) {
	    (void) Scheduler(1);
	}
	oia_modified = ps_modified = 0;
#endif	/* defined(unix) */
    } else if (regs->h.ah != 0x09) {
	regs->h.ch = 0x12;
	regs->h.cl = 0x0f;		/* XXX Invalid environmental access */
    } else if (regs->x.bx != 0x8020) {
	regs->h.ch = 0x12;
	regs->h.cl = 0x08;		/* XXX Invalid wait specified */
    } else if (regs->h.ch != 0) {
	regs->x.cx = 0x1206;		/* XXX Invalid priority */
    } else {
	switch (regs->x.dx) {
	case GATE_SESSMGR:
	    switch (regs->h.al) {
	    case QUERY_SESSION_ID:
		if (regs->h.cl != 0) {
		    regs->x.cx = 0x1206;
		} else {
		    regs->x.cx = 0x1200;
		    query_session_id(regs, sregs);
		}
		break;
	    case QUERY_SESSION_PARAMETERS:
		if (regs->h.cl != 0) {
		    regs->x.cx = 0x1206;
		} else {
		    regs->x.cx = 0x1200;
		    query_session_parameters(regs, sregs);
		}
		break;
	    case QUERY_SESSION_CURSOR:
		if ((regs->h.cl != 0xff) && (regs->h.cl != 0x00/*OBS*/)) {
		    regs->x.cx = 0x1206;
		} else {
		    regs->x.cx = 0x1200;
		    query_session_cursor(regs, sregs);
		}
		break;
	    default:
		unknown_op(regs, sregs);
		break;
	    }
	    break;
	case GATE_KEYBOARD:
	    if (regs->h.cl != 00) {
		regs->x.cx = 0x1206;
	    } else {
		regs->x.cx = 0x1200;
		switch (regs->h.al) {
		case CONNECT_TO_KEYBOARD:
		    connect_to_keyboard(regs, sregs);
		    break;
		case DISABLE_INPUT:
		    disable_input(regs, sregs);
		    break;
		case WRITE_KEYSTROKE:
		    write_keystroke(regs, sregs);
		    break;
		case ENABLE_INPUT:
		    enable_input(regs, sregs);
		    break;
		case DISCONNECT_FROM_KEYBOARD:
		    disconnect_from_keyboard(regs, sregs);
		    break;
		default:
		    unknown_op(regs, sregs);
		    break;
		}
	    }
	    break;
	case GATE_COPY:
	    if (regs->h.cl != 0xff) {
		regs->x.cx = 0x1206;
	    } else {
		regs->x.cx = 0x1200;
		switch (regs->h.al) {
		case COPY_STRING:
		    copy_string(regs, sregs);
		    break;
		default:
		    unknown_op(regs, sregs);
		    break;
		}
	    }
	    break;
	case GATE_OIAM:
	    if (regs->h.cl != 0xff) {
		regs->x.cx = 0x1206;
	    } else {
		regs->x.cx = 0x1200;
		switch (regs->h.al) {
		case READ_OIA_GROUP:
		    read_oia_group(regs, sregs);
		    break;
		default:
		    unknown_op(regs, sregs);
		    break;
		}
	    }
	    break;
	default:
	    regs->h.ch = 0x12;
	    regs->h.cl = 0x34;		/* Invalid GATE entry */
	    break;
	}
    }
/*
 * Do we need to log this transaction?
 */
    if (apitrace) {
	Dump('>', (char *)regs, sizeof *regs);
	Dump('>', (char *)sregs, sizeof *sregs);
#ifdef	MSDOS
	{
	    int ch;

	    while ((ch = getchar()) != '\n' && ch != EOF)
		;
	}
#endif	/* MSDOS */
    }
}
