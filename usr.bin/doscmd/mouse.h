/*
 * Copyright (c) 1992, 1993, 1996
 *	Berkeley Software Design, Inc.  All rights reserved.
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
 *	This product includes software developed by Berkeley Software
 *	Design, Inc.
 *
 * THIS SOFTWARE IS PROVIDED BY Berkeley Software Design, Inc. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Berkeley Software Design, Inc. BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	BSDI mouse.h,v 2.2 1996/04/08 19:32:58 bostic Exp
 *
 * $FreeBSD$
 */

typedef struct {
    u_short		hardcursor:1;
    u_short		installed:1;
    u_short		cursor:1;
    u_short		show:1;
    u_short		buttons:3;

    u_short		init;
    u_short		start;
    u_short		end;
    u_short		hmickey;
    u_short		vmickey;
    u_short		doubling;
    u_long		handler;
    u_short		mask;
    u_long		althandler[3];
    u_short		altmask[3];
    struct {
	u_short	x;
	u_short	y;
	u_short	w;
	u_short	h;
    }	range, exclude;
    u_short		x;
    u_short		y;
    u_short		lastx;
    u_short		lasty;
    
    u_short		downs[3];
    u_short		ups[3];
} mouse_t;

extern mouse_t mouse_status;
extern u_char *mouse_area;
