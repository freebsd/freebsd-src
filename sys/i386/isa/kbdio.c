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
 * $Id: kbdio.c,v 1.1 1996/11/14 22:19:06 sos Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/syslog.h>
#include <machine/clock.h>
#include <i386/isa/isa.h>
#include <i386/isa/isa_device.h>
#include <i386/isa/kbdio.h>

#ifndef KBDIO_DEBUG
#define KBDIO_DEBUG	0
#endif

static int verbose = KBDIO_DEBUG;

/* 
 * device I/O routines
 */
int
wait_while_controller_busy(int port)
{
    /* CPU will stay inside the loop for 100msec at most */
    int retry = 5000;

    while (inb(port + KBD_STATUS_PORT) & KBDS_INPUT_BUFFER_FULL) {
        DELAY(20);
        if (--retry < 0)
    	return FALSE;
    }
    return TRUE;
}

/*
 * wait for any data; whether it's from the controller, 
 * the keyboard, or the aux device.
 */
int
wait_for_data(int port)
{
    /* CPU will stay inside the loop for 200msec at most */
    int retry = 10000;

    while ((inb(port + KBD_STATUS_PORT) & KBDS_ANY_BUFFER_FULL) == 0) {
        DELAY(20);
        if (--retry < 0)
    	return FALSE;
    }
    DELAY(7);
    return TRUE;
}

/* wait for data from the keyboard */
int
wait_for_kbd_data(int port)
{
    /* CPU will stay inside the loop for 200msec at most */
    int retry = 10000;

    while ((inb(port + KBD_STATUS_PORT) & KBDS_BUFFER_FULL)
           != KBDS_KBD_BUFFER_FULL) {
        DELAY(20);
        if (--retry < 0)
    	return FALSE;
    }
    DELAY(7);
    return TRUE;
}

/* wait for data from the aux device */
int
wait_for_aux_data(int port)
{
    /* CPU will stay inside the loop for 200msec at most */
    int retry = 10000;

    while ((inb(port + KBD_STATUS_PORT) & KBDS_BUFFER_FULL)
           != KBDS_AUX_BUFFER_FULL) {
        DELAY(20);
        if (--retry < 0)
    	return FALSE;
    }
    DELAY(7);
    return TRUE;
}

int
write_controller_command(int port, int c)
{
    if (!wait_while_controller_busy(port))
	return FALSE;
    outb(port + KBD_COMMAND_PORT, c);
    return TRUE;
}

int
write_controller_data(int port, int c)
{
    if (!wait_while_controller_busy(port))
	return FALSE;
    outb(port + KBD_DATA_PORT, c);
    return TRUE;
}

int
write_kbd_command(int port, int c)
{
    if (!wait_while_controller_busy(port))
	return FALSE;
    outb(port + KBD_DATA_PORT, c);
    return TRUE;
}

int
write_aux_command(int port, int c)
{
    if (!write_controller_command(port,KBDC_WRITE_TO_AUX))
	return FALSE;
    return write_controller_data(port, c);
}

int
send_kbd_command(int port, int c)
{
    int retry = KBD_MAXRETRY;
    int res = -1;

    while (retry-- > 0) {
	if (!write_kbd_command(port, c))
	    continue;
        res = read_controller_data(port);
        if (res == KBD_ACK)
    	break;
    }
    return res;
}

int
send_aux_command(int port, int c)
{
    int retry = KBD_MAXRETRY;
    int res = -1;

    while (retry-- > 0) {
	if (!write_aux_command(port, c))
	    continue;
        res = read_aux_data(port);
        if (res == PSM_ACK)
    	break;
    }
    return res;
}

int
send_kbd_command_and_data(int port, int c, int d)
{
    int retry;
    int res = -1;

    for (retry = KBD_MAXRETRY; retry > 0; --retry) {
	if (!write_kbd_command(port, c))
	    continue;
        res = read_controller_data(port);
        if (res == KBD_ACK)
    	break;
        else if (res != PSM_RESEND)
    	    return res;
    }
    if (retry <= 0)
	return res;

    for (retry = KBD_MAXRETRY, res = -1; retry > 0; --retry) {
	if (!write_kbd_command(port, d))
	    continue;
        res = read_controller_data(port);
        if (res != KBD_RESEND)
    	break;
    }
    return res;
}

int
send_aux_command_and_data(int port, int c, int d)
{
    int retry;
    int res = -1;

    for (retry = KBD_MAXRETRY; retry > 0; --retry) {
	if (!write_aux_command(port, c))
	    continue;
        res = read_aux_data(port);
        if (res == PSM_ACK)
    	break;
        else if (res != PSM_RESEND)
    	return res;
    }
    if (retry <= 0)
	return res;

    for (retry = KBD_MAXRETRY, res = -1; retry > 0; --retry) {
	if (!write_aux_command(port, d))
	    continue;
        res = read_aux_data(port);
        if (res != PSM_RESEND)
    	break;
    }
    return res;
}

/* 
 * read one byte from any source; whether from the controller,
 * the keyboard, or the aux device
 */
int
read_controller_data(int port)
{
    if (!wait_for_data(port))
        return -1;		/* timeout */
    return inb(port + KBD_DATA_PORT);
}

/* read one byte from the keyboard */
int
read_kbd_data(int port)
{
    if (!wait_for_kbd_data(port))
        return -1;		/* timeout */
    return inb(port + KBD_DATA_PORT);
}

/* read one byte from the keyboard, but return immediately if 
 * no data is waiting
 */
int
read_kbd_data_no_wait(int port)
{
    if ((inb(port + KBD_STATUS_PORT) & KBDS_BUFFER_FULL)
    	!= KBDS_KBD_BUFFER_FULL) 
        return -1;		/* no data */
    DELAY(7);
    return inb(port + KBD_DATA_PORT);
}

/* read one byte from the aux device */
int
read_aux_data(int port)
{
    if (!wait_for_aux_data(port))
        return -1;		/* timeout */
    return inb(port + KBD_DATA_PORT);
}

/* discard data from the keyboard */
void
empty_kbd_buffer(int port, int t)
{
    int b;
    int c = 0;
    int delta = 2;

    for (; t > 0; t -= delta) { 
        if ((inb(port + KBD_STATUS_PORT) & KBDS_BUFFER_FULL)
    	   == KBDS_KBD_BUFFER_FULL) {
	    DELAY(7);
        b = inb(port + KBD_DATA_PORT);
        ++c;
    }
        DELAY(delta*1000);
    }
    if ((verbose >= 2) && (c > 0))
    log(LOG_DEBUG,"kbdio: %d char read (empty_kbd_buffer)\n",c);
}

/* discard data from the aux device */
void
empty_aux_buffer(int port, int t)
{
    int b;
    int c = 0;
    int delta = 2;

    for (; t > 0; t -= delta) { 
        if ((inb(port + KBD_STATUS_PORT) & KBDS_BUFFER_FULL)
    	    == KBDS_AUX_BUFFER_FULL) {
	    DELAY(7);
        b = inb(port + KBD_DATA_PORT);
        ++c;
    }
	DELAY(delta*1000);
    }
    if ((verbose >= 2) && (c > 0))
    log(LOG_DEBUG,"kbdio: %d char read (empty_aux_buffer)\n",c);
}

/* discard any data from the keyboard or the aux device */
void
empty_both_buffers(int port, int t)
{
    int b;
    int c = 0;
    int delta = 2;

    for (; t > 0; t -= delta) { 
        if (inb(port + KBD_STATUS_PORT) & KBDS_ANY_BUFFER_FULL) {
	    DELAY(7);
        b = inb(port + KBD_DATA_PORT);
        ++c;
    }
	DELAY(delta*1000);
    }
    if ((verbose >= 2) && (c > 0))
    log(LOG_DEBUG,"kbdio: %d char read (empty_both_buffers)\n",c);
}

/* keyboard and mouse device control */

/* NOTE: enable the keyboard port but disable the keyboard 
 * interrupt before calling "reset_kbd()".
 */
int
reset_kbd(int port)
{
    int retry = KBD_MAXRETRY;
    int again = KBD_MAXWAIT;
    int c = KBD_RESEND;		/* keep the compiler happy */

    while (retry-- > 0) {
        empty_both_buffers(port, 10);
        if (!write_kbd_command(port, KBDC_RESET_KBD))
	    continue;
        c = read_controller_data(port);
	if (verbose)
        log(LOG_DEBUG,"kbdio: RESET_KBD return code:%04x\n",c);
        if (c == KBD_ACK)	/* keyboard has agreed to reset itself... */
    	break;
    }
    if (retry < 0)
        return FALSE;

    while (again-- > 0) {
        /* wait awhile, well, in fact we must wait quite loooooooooooong */
        DELAY(KBD_RESETDELAY*1000);
        c = read_controller_data(port);	/* RESET_DONE/RESET_FAIL */
        if (c != -1) 	/* wait again if the controller is not ready */
    	break;
    }
    if (verbose)
    log(LOG_DEBUG,"kbdio: RESET_KBD status:%04x\n",c);
    if (c != KBD_RESET_DONE)
        return FALSE;
    return TRUE;
}

/* NOTE: enable the aux port but disable the aux interrupt
 * before calling `reset_aux_dev()'.
 */
int
reset_aux_dev(int port)
{
    int retry = KBD_MAXRETRY;
    int again = KBD_MAXWAIT;
    int c = PSM_RESEND;		/* keep the compiler happy */

    while (retry-- > 0) {
        empty_both_buffers(port, 10);
        if (!write_aux_command(port, PSMC_RESET_DEV))
	    continue;
        c = read_controller_data(port);
        if (verbose)
        log(LOG_DEBUG,"kbdio: RESET_AUX return code:%04x\n",c);
        if (c == PSM_ACK)	/* aux dev is about to reset... */
    	break;
    }
    if (retry < 0)
        return FALSE;

    while (again-- > 0) {
        /* wait awhile, well, quite looooooooooooong */
        DELAY(KBD_RESETDELAY*1000);
        c = read_aux_data(port);	/* RESET_DONE/RESET_FAIL */
        if (c != -1) 	/* wait again if the controller is not ready */
    	break;
    }
    if (verbose)
    log(LOG_DEBUG,"kbdio: RESET_AUX status:%04x\n",c);
    if (c != PSM_RESET_DONE)	/* reset status */
        return FALSE;

    c = read_aux_data(port);	/* device ID */
    if (verbose)
    log(LOG_DEBUG,"kbdio: RESET_AUX ID:%04x\n",c);
	/* NOTE: we could check the device ID now, but leave it later... */
    return TRUE;
}

/* controller diagnostics and setup */

int
test_controller(int port)
{
    int retry = KBD_MAXRETRY;
    int again = KBD_MAXWAIT;
    int c = KBD_DIAG_FAIL;

    while (retry-- > 0) {
        empty_both_buffers(port, 10);
        if (write_controller_command(port, KBDC_DIAGNOSE))
    	    break;
    }
    if (retry < 0)
        return FALSE;

    while (again-- > 0) {
        /* wait awhile */
        DELAY(KBD_RESETDELAY*1000);
    c = read_controller_data(port);	/* DIAG_DONE/DIAG_FAIL */
        if (c != -1) 	/* wait again if the controller is not ready */
    	    break;
    }
    if (verbose)
    log(LOG_DEBUG,"kbdio: DIAGNOSE status:%04x\n",c);
    return (c == KBD_DIAG_DONE);
}

int
test_kbd_port(int port)
{
    int retry = KBD_MAXRETRY;
    int again = KBD_MAXWAIT;
    int c = -1;

    while (retry-- > 0) {
        empty_both_buffers(port, 10);
        if (write_controller_command(port, KBDC_TEST_KBD_PORT))
    	    break;
    }
    if (retry < 0)
        return FALSE;

    while (again-- > 0) {
    c = read_controller_data(port);
        if (c != -1) 	/* try again if the controller is not ready */
    	    break;
    }
    if (verbose)
    log(LOG_DEBUG,"kbdio: TEST_KBD_PORT status:%04x\n",c);
    return c;
}

int
test_aux_port(int port)
{
    int retry = KBD_MAXRETRY;
    int again = KBD_MAXWAIT;
    int c = -1;

    while (retry-- > 0) {
        empty_both_buffers(port, 10);
        if (write_controller_command(port, KBDC_TEST_AUX_PORT))
    	    break;
    }
    if (retry < 0)
        return FALSE;

    while (again-- > 0) {
    c = read_controller_data(port);
        if (c != -1) 	/* try again if the controller is not ready */
    	    break;
    }
    if (verbose)
    log(LOG_DEBUG,"kbdio: TEST_AUX_PORT status:%04x\n",c);
    return c;
}

int
set_controller_command_byte(int port, int command, int flag)
{
    if ((command | flag) & KBD_DISABLE_KBD_PORT) {
	if (!write_controller_command(port, KBDC_DISABLE_KBD_PORT))
	    return FALSE;
    }
    if (!write_controller_command(port, KBDC_SET_COMMAND_BYTE))
	return FALSE;
    if (!write_controller_data(port, command | flag))
	return FALSE;
    wait_while_controller_busy(port);
    return TRUE;
}
