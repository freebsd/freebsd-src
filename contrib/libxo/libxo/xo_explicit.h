/*
 * Copyright (c) 2015, Juniper Networks, Inc.
 * All rights reserved.
 * This SOFTWARE is licensed under the LICENSE provided in the
 * ../Copyright file. By downloading, installing, copying, or otherwise
 * using the SOFTWARE, you agree to be bound by the terms of that
 * LICENSE.
 *
 * Phil Shafer, March 2019
 */

#ifndef XO_EXPLICIT_H
#define XO_EXPLICIT_H

/*
 * NOTE WELL: This file is needed to software that implements an
 * explicit transition between libxo states on its internal stack.
 * General libxo code should _never_ include this header file.
 */


/*
 * A word about states: We use a finite state machine (FMS) approach
 * to help remove fragility from the caller's code.  Instead of
 * requiring a specific order of calls, we'll allow the caller more
 * flexibility and make the library responsible for recovering from
 * missed steps.  The goal is that the library should not be capable
 * of emitting invalid xml or json, but the developer shouldn't need
 * to know or understand all the details about these encodings.
 *
 * You can think of states as either states or events, since they
 * function rather like both.  None of the XO_CLOSE_* events will
 * persist as states, since the matching stack frame will be popped.
 * Same is true of XSS_EMIT, which is an event that asks us to
 * prep for emitting output fields.
 */

/* Stack frame states */
typedef unsigned xo_state_t;	/* XSS_* values */
#define XSS_INIT		0      	/* Initial stack state */
#define XSS_OPEN_CONTAINER	1
#define XSS_CLOSE_CONTAINER	2
#define XSS_OPEN_LIST		3
#define XSS_CLOSE_LIST		4
#define XSS_OPEN_INSTANCE	5
#define XSS_CLOSE_INSTANCE	6
#define XSS_OPEN_LEAF_LIST	7
#define XSS_CLOSE_LEAF_LIST	8
#define XSS_DISCARDING		9	/* Discarding data until recovered */
#define XSS_MARKER		10	/* xo_open_marker's marker */
#define XSS_EMIT		11	/* xo_emit has a leaf field */
#define XSS_EMIT_LEAF_LIST	12	/* xo_emit has a leaf-list ({l:}) */
#define XSS_FINISH		13	/* xo_finish was called */

#define XSS_MAX			13

void
xo_explicit_transition (xo_handle_t *xop, xo_state_t new_state,
			const char *tag, xo_xof_flags_t flags);

#endif /* XO_EXPLICIT_H */
