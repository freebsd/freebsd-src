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
static char copyright[] =
"@(#) Copyright (c) 1988, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)tnrecv.c	8.1 (Berkeley) 6/6/93";
#endif /* not lint */

#include <stdio.h>

#include <api/apilib.h>

#include "tncomp.h"


#include "../ctlr/api.h"
#include "../ctlr/function.h"
#include "../ctlr/hostctlr.h"
#include "../ctlr/oia.h"
#include "../ctlr/screen.h"

#include "../api/disp_asc.h"
#include "../api/astosc.h"

#include "../general/general.h"

ScreenImage Host[MAXSCREENSIZE];

static char
    a_send_sequence[SEND_SEQUENCE_LENGTH+1],
    a_ack_sequence[ACK_SEQUENCE_LENGTH+1],
    a_checksum[CHECKSUM_LENGTH+1],
    data_array[DATA_LENGTH+1];

static int
    verbose,
    blocks,
    enter_index,
    clear_index,
    ScreenSize,
    session_id;

static unsigned int
    send_sequence,
    ack_sequence = -1,
    checksum;

api_perror(string)
char *string;
{
    fprintf(stderr, "Error: [0x%x/0x%x:0x%x/0x%x] from %s.\n",
	api_sup_fcn_id, api_sup_errno,
	api_fcn_fcn_id, api_fcn_errno, string);
}


char *
session_type(type)
int	type;
{
    switch (type) {
    case TYPE_WSCTL:
	return "work station control";
    case TYPE_DFT:
	return "distributed function terminal";
    case TYPE_CUT:
	return "control unit terminal";
    case TYPE_NOTEPAD:
	return "notepad";
    case TYPE_PC:
	return "personal computer";
    default:
	return "(UNKNOWN)";
    }
}

static int
wait_for_ps_or_oia()
{
#if	defined(unix)
    return api_ps_or_oia_modified();
#endif	/* defined(unix) */
}


static int
wait_for_unlock()
{
    OIA oia;
    ReadOiaGroupParms re;
    static char zeroes[sizeof oia.input_inhibited] = { 0 };

    do {
	re.rc = re.function_id = 0;
	re.session_id = session_id;
	re.oia_buffer = (char far *) &oia;
	re.oia_group_number = API_OIA_ALL_GROUPS;
	if (api_read_oia_group(&re) == -1) {
	    api_perror("api_read_oia_group");
	    return -1;
	} else if (verbose) {
	    if (IsOiaReady3274(&oia)) {
		printf("3274 ready, ");
	    }
	    if (IsOiaMyJob(&oia)) {
		printf("my job, ");
	    }
	    if (IsOiaInsert(&oia)) {
		printf("insert mode, ");
	    }
	    if (IsOiaSystemLocked(&oia)) {
		printf("system locked, ");
	    }
	    if (IsOiaTWait(&oia)) {
		printf("terminal wait, ");
	    }
	    printf("are some bits from the OIA.\n");
	}
	/* We turned this on, so turn it off now */
	ResetOiaApiInhibit(&oia);
	if (memcmp(zeroes, oia.input_inhibited, sizeof oia.input_inhibited)) {
	    if (wait_for_ps_or_oia() == -1) {
		return -1;
	    }
	}
    } while (memcmp(zeroes, oia.input_inhibited, sizeof oia.input_inhibited));
    return 0;
}

static int
initialize()
{
    QuerySessionIdParms id;
    QuerySessionParametersParms pa;
    QuerySessionCursorParms cu;
    ConnectToKeyboardParms conn;
    DisableInputParms disable;
    NameArray namearray;

    if (api_init() == 0) {
	fprintf(stderr, "API function not available.\n");
	return -1;
    }

    id.rc = 0;
    id.function_id = 0;
    id.option_code = ID_OPTION_BY_NAME;
    id.data_code = 'E';
    id.name_array = &namearray;
    namearray.length = sizeof namearray;
    if (api_query_session_id(&id)) {
	api_perror("api_query_session_id");
    } else if (namearray.number_matching_session == 0) {
	fprintf(stderr, "query_session_id:  No matching sessions!\n");
	return -1;
    } else if (verbose) {
	printf("Session short name 0x%x, type is ",
				namearray.name_array_element.short_name);
	printf("%s", session_type(namearray.name_array_element.type));
	printf(", session ID is: 0x%x\n",
				namearray.name_array_element.session_id);
    }
    session_id = namearray.name_array_element.session_id;

    pa.rc = pa.function_id = 0;
    pa.session_id = session_id;
    if (api_query_session_parameters(&pa) == -1) {
	api_perror("api_query_session_parameters");
	return -1;
    } else if (verbose) {
	printf("Session type %s, ", session_type(pa.session_type));
	if (pa.session_characteristics&CHARACTERISTIC_EAB) {
	    printf(" has EAB, ");
	}
	if (pa.session_characteristics&CHARACTERISTIC_PSS) {
	    printf(" has PSS, ");
	}
	printf("%d rows, %d columns ", pa.rows, pa.columns);
	if (pa.presentation_space) {
	    printf("presentation space at 0x%x:0x%x.\n",
		FP_SEG(pa.presentation_space), FP_OFF(pa.presentation_space));
	} else {
	    printf("(no direct presentation space access).\n");
	}
    }
    ScreenSize = pa.rows*pa.columns;
    if (pa.session_characteristics&CHARACTERISTIC_EAB) {
	fprintf(stderr,
    "tncomp utilities not designed to work with extended attribute buffers.\n");
	return -1;
    }

    if (verbose) {
	cu.rc = cu.function_id = 0;
	cu.session_id = session_id;
	if (api_query_session_cursor(&cu) == -1) {
	    api_perror("api_query_session_cursor");
	} else {
	    printf("cursor");
	    if (cu.cursor_type&CURSOR_INHIBITED_AUTOSCROLL) {
		printf(" inhibited autoscroll");
	    }
	    if (cu.cursor_type&CURSOR_INHIBITED) {
		printf(" inhibited");
	    }
	    if (cu.cursor_type&CURSOR_BLINKING) {
		printf(" blinking");
	    } else {
		printf(" not blinking");
	    }
	    if (cu.cursor_type&CURSOR_BOX) {
		printf(" box ");
	    } else {
		printf(" not box ");
	    }
	    printf("at row %d, column %d.\n",
				cu.row_address, cu.column_address);
	}
    }

    conn.rc = conn.function_id = 0;
    conn.session_id = session_id;
    conn.event_queue_id = conn.input_queue_id = 0;
    conn.intercept_options = 0;
    if (api_connect_to_keyboard(&conn) == -1) {
	api_perror("api_connect_to_keyboard");
    } else if (verbose) {
	if (conn.first_connection_identifier) {
	    printf("First keyboard connection.\n");
	} else {
	    printf("Not first keyboard connection.\n");
	}
    }

    disable.rc = disable.function_id = 0;
    disable.session_id = session_id;
    disable.connectors_task_id = 0;
    if (api_disable_input(&disable) == -1) {
	api_perror("api_disable_input");
	return -1;
    } else if (verbose) {
	printf("Disabled.\n");
    }

    if ((enter_index = ascii_to_index("ENTER")) == -1) {
	return -1;
    }
    if ((clear_index = ascii_to_index("CLEAR")) == -1) {
	return -1;
    }

    return 0;				/* all ok */
}

static int
send_key(index)
int	index;
{
    WriteKeystrokeParms wr;
    extern struct astosc astosc[];

    wait_for_unlock();

    wr.rc = wr.function_id = 0;
    wr.session_id = session_id;
    wr.connectors_task_id = 0;
    wr.options = OPTION_SINGLE_KEYSTROKE;
    wr.number_of_keys_sent = 0;
    wr.keystroke_specifier.keystroke_entry.scancode = astosc[index].scancode;
    wr.keystroke_specifier.keystroke_entry.shift_state
						= astosc[index].shiftstate;
    if (api_write_keystroke(&wr) == -1) {
	api_perror("api_write_keystroke");
	return -1;
    } else if (wr.number_of_keys_sent != 1) {
	fprintf(stderr, "write_keystroke claims to have sent %d keystrokes.\n",
		    wr.number_of_keys_sent);
	return -1;
    } else if (verbose) {
	printf("Keystroke sent.\n");
    }
    if (wait_for_ps_or_oia() == -1) {
	return -1;
    }
    return 0;
}

static int
terminate()
{
    EnableInputParms enable;
    DisconnectFromKeyboardParms disc;

    enable.rc = enable.function_id = 0;
    enable.session_id = session_id;
    enable.connectors_task_id = 0;
    if (api_enable_input(&enable) == -1) {
	api_perror("api_enable");
	return -1;
    } else if (verbose) {
	printf("Enabled.\n");
    }

    disc.rc = disc.function_id = 0;
    disc.session_id = session_id;
    disc.connectors_task_id = 0;
    if (api_disconnect_from_keyboard(&disc) == -1) {
	api_perror("api_disconnect_from_keyboard");
	return -1;
    } else if (verbose) {
	printf("Disconnected from keyboard.\n");
    }

    (void) api_finish();

    return 0;
}


static int
get_screen()
{
    CopyStringParms copy;
    /* Time copy services */

    wait_for_unlock();

    copy.copy_mode = 0;
    copy.rc = copy.function_id = 0;
    copy.source.session_id = session_id;
    copy.source.buffer = 0;
    copy.source.characteristics = 0;
    copy.source.session_type = TYPE_DFT;
    copy.source.begin = 0;

    copy.source_end = ScreenSize;

    copy.target.session_id = 0;
    copy.target.buffer = (char *) &Host[0];
    copy.target.characteristics = 0;
    copy.target.session_type = TYPE_DFT;

    if (api_copy_string(&copy) == -1) {
	api_perror("api_copy_string");
	return -1;
    }
    return 0;
}


put_at(offset, from, length, attribute)
int	offset;
char	*from;
int	length;
{
    CopyStringParms copy;

    wait_for_unlock();

    copy.copy_mode = 0;
    copy.rc = copy.function_id = 0;
    copy.source.session_id = 0;
    copy.source.buffer = from;
    copy.source.characteristics = 0;
    copy.source.session_type = TYPE_DFT;
    copy.source.begin = 0;

    copy.source_end = length-1;

    copy.target.session_id = session_id;
    copy.target.buffer = 0;
    copy.target.characteristics = 0;
    copy.target.session_type = TYPE_DFT;
    copy.target.begin = offset;

    if (api_copy_string(&copy) == -1) {
	api_perror("api_copy_string");
	return -1;
    }
    return 0;
}

static void
translate(input, output, table, length)
char *input, *output, table[];
int length;
{
    unsigned char *indices = (unsigned char *) input;

    while (length--) {
	*output++ = table[*indices++];
    }
}

static int
find_input_area(from)
int	from;
{
#define	FieldDec(p)	(0)		/* We don't really use this */
    register int i, attr;

    for (i = from; i < MAXSCREENSIZE; ) {
	if (IsStartField(i)) {
	    attr = FieldAttributes(i);
	    i++;
	    if (!IsProtectedAttr(i, attr)) {
		return i;
	    }
	} else {
	    i++;
	}
    }
    return -1;
}


static void
getascii(offset, to, length)
int	offset;				/* Where in screen */
char	*to;				/* Where it goes to */
int	length;				/* Where to put it */
{
    translate(Host+offset, to, disp_asc, length);
}

static int
putascii(offset, from, length, before)
int	offset;				/* Where in screen */
char	*from;				/* Where it comes from */
int	length;				/* Where to put it */
int	before;				/* How much else should go */
{
    translate(from, Host+offset, asc_disp, length);
    if (put_at(offset-before,
			(char *) Host+offset-before, length+before) == -1) {
	return -1;
    }
    return 0;
}

static int
ack()
{
    static char ack_blanks[sizeof a_ack_sequence] = {0};

    if (ack_blanks[0] == 0) {
	int i;

	for (i = 0; i < sizeof ack_blanks; i++) {
	    ack_blanks[i] = ' ';
	}
    }

    memcpy(a_ack_sequence, ack_blanks, sizeof a_ack_sequence);
    sprintf(a_ack_sequence, "%d", ack_sequence);
    a_ack_sequence[strlen(a_ack_sequence)] = ' ';
    if (putascii(ACK_SEQUENCE, a_ack_sequence, ACK_SEQUENCE_LENGTH, 0) == -1) {
	return -1;
    }
    return 0;
}

static int
formatted_correct()
{
    if ((find_input_area(SEND_SEQUENCE-1) != SEND_SEQUENCE) ||
	    (find_input_area(SEND_SEQUENCE) != ACK_SEQUENCE) ||
	    (find_input_area(ACK_SEQUENCE) != CHECKSUM) ||
	    (find_input_area(CHECKSUM) != DATA)) {
	return -1;
    } else {
	return 0;
    }
}


main(argc, argv)
int	argc;
char	*argv[];
{
    register int i;
    int data_length, input_length;
    char ascii[8];			/* Lots of room */
    FILE *outfile;
    char *data;
    char *argv0 = argv[0];

    argc--;
    argv++;
    /* Process any flags */
    while (argc && (argv[0][0] == '-')) {
	switch (argv[0][1]) {
	case 'v':
	    verbose = 1;
	    break;
	case 'b':
	    blocks = 1;
	    break;
	}
	argc--;
	argv++;
    }

    if ((argc) < 2) {
	fprintf(stderr,
		"usage: %s [-b] [-v] local.file remote.file [remote.options]\n",
			argv0);
	exit(1);
    }

    /* Open the local file */
    if ((outfile = fopen(argv[0], "w")) == NULL) {
	perror("fopen");
	exit(2);
    }
    argc--;
    argv++;

    if (initialize() == -1) {
	return -1;
    }

    /* build the command line */
    data = data_array;
    strcpy(data, "TNCOMP SEND");
    data += strlen(data);
    while (argc--) {
	*data++ = ' ';
	strcpy(data, argv[0]);
	data += strlen(argv[0]);
	argv++;
    }
    if (verbose) {
	printf("%s\n", data_array);
    }
    if (get_screen() == -1) {
	return -1;
    }
    data_length = strlen(data_array);
    if ((i = find_input_area(0)) == -1) {		/* Get an input area */
	if (send_key(clear_index) == -1) {
	    return -1;
	}
	if ((i = find_input_area(0)) == -1) {		/* Try again */
	    fprintf(stderr, "Unable to enter command line.\n");
	    return -1;
	}
    }
    if (putascii(i, data_array, data_length, 0) == -1) {
	return -1;
    }
    if (send_key(enter_index) == -1) {
	return -1;
    }
    do {
	if (get_screen() == -1) {
	    return -1;
	}
    } while (formatted_correct() == -1);

    do {
	if (get_screen() == -1) {
	    return -1;
	}
	/* For each screen */
	if (formatted_correct() == -1) {
	    fprintf(stderr, "Bad screen written by host.\n");
	    return -1;
	}
	/* If MDT isn't reset in the sequence number, go around again */
	if (Host[ACK_SEQUENCE-1]&ATTR_MDT) {
	    if (wait_for_ps_or_oia() == -1) {
		return -1;
	    }
	    continue;
	}
	getascii(SEND_SEQUENCE, a_send_sequence, SEND_SEQUENCE_LENGTH);
	send_sequence = atoi(a_send_sequence);
	getascii(CHECKSUM, a_checksum, CHECKSUM_LENGTH);
	checksum = atoi(a_checksum);
	getascii(DATA, data_array, DATA_LENGTH);
	data = data_array;
	if (send_sequence != (ack_sequence+1)) {
	    if (ack() == -1) {
		return -1;
	    }
	    data = "1234";		/* Keep loop from failing */
	    if (send_key(enter_index) == -1) {
		return -1;
	    }
	    if (get_screen() == -1) {
		return -1;
	    }
	    continue;
	}

	data_length = DATA_LENGTH;
	while (data_length && memcmp(data, " EOF", 4)
						&& memcmp(data, "    ", 4)) {
	    memcpy(ascii, data, 4);
	    data += 4;
	    data_length -= 4;
	    ascii[4] = 0;
	    input_length = atoi(ascii);
	    /* CMS can't live with zero length records */
	    if ((input_length > 1) ||
			((input_length == 1) && (data[0] != ' '))) {
		if (fwrite(data, sizeof (char),
					input_length, outfile) == 0) {
		    perror("fwrite");
		    exit(9);
		}
	    }
	    fprintf(outfile, "\n");
	    data += input_length;
	    data_length -= input_length;
	}

	ack_sequence = send_sequence;
	if (blocks) {
	    printf("#");
	    fflush(stdout);
	}
	if (ack() == -1) {
	    return -1;
	}
	if (send_key(enter_index) == -1) {
	    return -1;
	}
    } while (memcmp(data, " EOF", 4));

    if (blocks) {
	printf("\n");
    }
    if (terminate() == -1) {
	return -1;
    }
    return 0;
}
