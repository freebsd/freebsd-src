/*-
 * Copyright (c) 1991 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Van Jacobson and Steven McCanne of Lawrence Berkeley Laboratory.
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
 * $Header: /home/cvs/386BSD/src/usr.bin/gdb/remote.c,v 1.1.1.1 1993/06/12 14:52:22 rgrimes Exp $;
 */

#ifndef lint
static char sccsid[] = "@(#)remote.c	6.5 (Berkeley) 5/8/91";
#endif /* not lint */

#include "param.h"

#include <stdio.h>
#include <varargs.h>

#include <signal.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/file.h>

#include "defs.h"
#include "frame.h"
#include "inferior.h"
#include "wait.h"

#include "kgdb_proto.h"

static FILE *kiodebug;
static int icache = 1;
extern int kernel_debugging;

static int remote_cache_valid;
static int remote_instub;

static void remote_signal();
static void remote_debug();
static void print_msg();

static int remote_mtu;
static int (*send_msg)();
static int (*recv_msg)();
static void (*closelink)();

static u_char *inbuffer;
static u_char *outbuffer;

/*
 * Statistics.
 */
static int remote_ierrs;
static int remote_oerrs;
static int remote_seqerrs;
static int remote_spurious;

#define PUTCMD(cmd) m_xchg(cmd, (u_char *)0, 0, (u_char *)0, (int *)0)

/*
 * Send an outbound message to the remote machine and read the reply.
 * Either or both message buffers may be NULL.
 */
static int
m_xchg(type, out, outlen, in, inlen)
	int type;
	u_char *out;
	int outlen;
	u_char *in;
	int *inlen;
{
	register int err, (*send)() = send_msg, (*recv)() = recv_msg;
	int ack;
	static int seqbit = 0;

	if (!remote_instub) {
		remote_instub = 1;
		PUTCMD(KGDB_EXEC);
	}

	seqbit ^= KGDB_SEQ;
	while (1) {
		err = (*send)(type | seqbit, out, outlen);
		if (err) {
			++remote_oerrs;
			if (kiodebug)
				remote_debug("send error %d\n", err);
		}
		if (kiodebug)
			print_msg(type | seqbit, out, outlen, 'O');

	recv:
		err = (*recv)(&ack, in, inlen);
		if (err) {
			++remote_ierrs;
			if (kiodebug)
				remote_debug("recv error %d\n", err);
			remote_cache_valid = 0;
		} else if (kiodebug)
			print_msg(ack, in, inlen ? *inlen : 0, 'I');

		if (err)
			continue;

		if ((ack & KGDB_ACK) == 0 || KGDB_CMD(ack) != KGDB_CMD(type)) {
			++remote_spurious;
			continue;
		}
		if ((ack & KGDB_SEQ) ^ seqbit) {
			++remote_seqerrs;
			goto recv;
		}
		return ack;
	}
}

/*
 * Wait for the specified message type.  Discard anything else.
 * (this is used by 'remote-signal' to help us resync with other side.)
 */
static void
m_recv(type, in, inlen)
	int type;
	u_char *in;
	int *inlen;
{
	int reply, err;

	while (1) {
		err = (*recv_msg)(&reply, in, inlen);
		if (err) {
			++remote_ierrs;
			if (kiodebug)
				remote_debug("recv error %d\n", err);
		} else if (kiodebug)
			print_msg(reply, in, inlen ? *inlen : 0, 'I');

		if (KGDB_CMD(reply) == type)
			return;
		++remote_spurious;
	}
}

/*
 * Send a message.  Do not wait for *any* response from the other side.
 * Some other thread of control will pick up the ack that will be generated.
 */
static void
m_send(type, buf, len)
	int type;
	u_char *buf;
	int len;
{
	int err;

	if (!remote_instub) {
		remote_instub = 1;
		PUTCMD(KGDB_EXEC);
	}

	err = (*send_msg)(type, buf, len);
	if (err) {
		++remote_ierrs;
		if (kiodebug)
			remote_debug("[send error %d] ", err);
	}
	if (kiodebug)
		print_msg(type, buf, len, 'O');
}

/*
 * Open a connection to a remote debugger.  
 * NAME is the filename used for communication.  
 */
void
remote_open(name, from_tty)
	char *name;
	int from_tty;
{
	int bufsize;

	remote_debugging = 0;
	if (sl_open(name, &send_msg, &recv_msg, &closelink, &remote_mtu,
		    &bufsize))
		return;
	if (from_tty)
		printf("Remote debugging using %s\n", name);
	remote_debugging = 1;

	remote_cache_valid = 0;

	inbuffer = (u_char *)malloc(bufsize);
	outbuffer = (u_char *)malloc(bufsize);

	remote_signal();

	remote_ierrs = 0;
	remote_oerrs = 0;
	remote_spurious = 0;
}

/*
 * Close the open connection to the remote debugger. Use this when you want
 * to detach and do something else with your gdb.  
 */
void
remote_close(from_tty)
	int from_tty;
{
	if (!remote_debugging)
		error("remote debugging not enabled");

	remote_debugging = 0;
	/*
	 * Take remote machine out of debug mode.
	 */
	(void)PUTCMD(KGDB_KILL);
	(*closelink)();
	if (from_tty)
		printf("Ending remote debugging\n");

	free((char *)inbuffer);
	free((char *)outbuffer);
}

/*
 * Tell the remote machine to resume.
 */
int
remote_resume(step, signal)
	int step, signal;
{
	if (!step) {
		(void)PUTCMD(KGDB_CONT);
		remote_instub = 0;
	} else {
#ifdef NO_SINGLE_STEP
		single_step(0);
#else
		(void)PUTCMD(KGDB_STEP);
#endif
	}
}

/*
 * Wait until the remote machine stops, then return, storing status in STATUS
 * just as `wait' would.
 */
int
remote_wait(status)
	WAITTYPE *status;
{
	int len;

	WSETEXIT((*status), 0);
	/*
	 * When the machine stops, it will send us a KGDB_SIGNAL message,
	 * so we wait for one of these.
	 */
	m_recv(KGDB_SIGNAL, inbuffer, &len);
	WSETSTOP((*status), inbuffer[0]);
}

/*
 * Register context as of last remote_fetch_registers().
 */
static char reg_cache[REGISTER_BYTES];

/*
 * Read the remote registers into the block REGS.
 */
void
remote_fetch_registers(regs)
	char *regs;
{
	int regno, len, rlen, ack;
	u_char *cp, *ep;

	regno = -1;
	do {
		outbuffer[0] = regno + 1;
		ack = m_xchg(remote_cache_valid ? 
			         KGDB_REG_R|KGDB_DELTA : KGDB_REG_R,
			     outbuffer, 1, inbuffer, &len);
		cp = inbuffer;
		ep = cp + len;
		while (cp < ep) {
			regno = *cp++;
			rlen = REGISTER_RAW_SIZE(regno);
			bcopy((char *)cp, 
			      &reg_cache[REGISTER_BYTE(regno)], rlen);
			cp += rlen;
		}
	} while (ack & KGDB_MORE);

	remote_cache_valid = 1;
	bcopy(reg_cache, regs, REGISTER_BYTES);
}

/*
 * Store the remote registers from the contents of the block REGS.
 */
void
remote_store_registers(regs)
	char *regs;
{
	u_char *cp, *ep;
	int regno, off, rlen;

	cp = outbuffer;
	ep = cp + remote_mtu;

	for (regno = 0; regno < NUM_REGS; ++regno) {
		off = REGISTER_BYTE(regno);
		rlen = REGISTER_RAW_SIZE(regno);
		if (!remote_cache_valid ||
		    bcmp(&regs[off], &reg_cache[off], rlen) != 0) {
			if (cp + rlen + 1 >= ep) {
				(void)m_xchg(KGDB_REG_W, 
					     outbuffer, cp - outbuffer, 
					     (u_char *)0, (int *)0);
				cp = outbuffer;
			}
			*cp++ = regno;
			bcopy(&regs[off], cp, rlen);
			cp += rlen;
		}
	}
	if (cp != outbuffer)
		(void)m_xchg(KGDB_REG_W, outbuffer, cp - outbuffer, 
			     (u_char *)0, (int *)0);
	bcopy(regs, reg_cache, REGISTER_BYTES);
}

/*
 * Store a chunk of memory into the remote host.
 * 'remote_addr' is the address in the remote memory space.
 * 'cp' is the address of the buffer in our space, and 'len' is
 * the number of bytes.  Returns an errno status.
 */
int
remote_write_inferior_memory(remote_addr, cp, len)
	CORE_ADDR remote_addr;
	u_char *cp;
	int len;
{
	int cnt;

	while (len > 0) {
		cnt = min(len, remote_mtu - 4);
		bcopy((char *)&remote_addr, outbuffer, 4);
		bcopy(cp, outbuffer + 4, cnt);
		(void)m_xchg(KGDB_MEM_W, outbuffer, cnt + 4, inbuffer, &len);

		if (inbuffer[0])
			return inbuffer[0];

		remote_addr += cnt;
		cp += cnt;
		len -= cnt;
	}
	return 0;
}

/*
 * Read memory data directly from the remote machine.
 * 'remote_addr' is the address in the remote memory space.
 * 'cp' is the address of the buffer in our space, and 'len' is
 * the number of bytes.  Returns an errno status.
 */
static int
remote_read_memory(remote_addr, cp, len)
	CORE_ADDR remote_addr;
	u_char *cp;
	int len;
{
	int cnt, inlen;

	while (len > 0) {
		cnt = min(len, remote_mtu - 1);
		outbuffer[0] = cnt;
		bcopy((char *)&remote_addr, (char *)&outbuffer[1], 4);

		(void)m_xchg(KGDB_MEM_R, outbuffer, 5, inbuffer, &inlen);

		if (inbuffer[0] != 0)
			return inbuffer[0];

		if (cnt != inlen - 1)
			/* XXX */
			error("remote_read_memory() request botched");

		bcopy((char *)&inbuffer[1], (char *)cp, cnt);

		remote_addr += cnt;
		cp += cnt;
		len -= cnt;
	}
	return 0;
}

int
remote_read_inferior_memory(remote_addr, cp, len)
	CORE_ADDR remote_addr;
	char *cp;
	int len;
{
	int stat = 0;

	if (icache) {
		extern CORE_ADDR text_start, text_end;
		CORE_ADDR xferend = remote_addr + len;

		if (remote_addr < text_end && text_start < xferend) {
			/*
			 * at least part of this xfer is in the text
			 * space -- xfer the overlap from the exec file.
			 */
			if (remote_addr >= text_start && xferend < text_end)
				return (xfer_core_file(remote_addr, cp, len));
			if (remote_addr >= text_start) {
				int i = text_end - remote_addr;

				if (stat = xfer_core_file(remote_addr, cp, i))
					return (stat);
				remote_addr += i;
				cp += i;
				len -= i;
			} else if (xferend <= text_end) {
				int i = xferend - text_start;

				len = text_start - remote_addr;
				if (stat = xfer_core_file(text_start,
							  cp + len, i))
					return (stat);
			}
		}
	}
	return remote_read_memory(remote_addr, cp, len);
}

/*
 * Signal the remote machine.  The remote end might be idle or it might
 * already be in debug mode -- we need to handle both case.  Thus, we use
 * the framing character as the wakeup byte, and send a SIGNAL packet.
 * If the remote host is idle, the framing character will wake it up.
 * If it is in the kgdb stub, then we will get a SIGNAL reply.
 */
static void
remote_signal()
{
	if (!remote_debugging)
		printf("Remote debugging not enabled.\n");
	else {
		remote_instub = 0;
		m_send(KGDB_SIGNAL, (u_char *)0, 0);
	}
}

static void
remote_signal_command()
{
	extern int stop_after_attach;

	if (!remote_debugging)
		error("Not debugging remote.");
	remote_cache_valid = 0;
	remote_signal();
	restart_remote();
}

/*
 * Print a message for debugging.
 */
static void
print_msg(type, buf, len, dir)
	int type;
	u_char *buf;
	int len;
	int dir;
{
	int i;
	char *s;

	switch (KGDB_CMD(type)) {
	case KGDB_MEM_R:	s = "memr"; break;
	case KGDB_MEM_W:	s = "memw"; break;
	case KGDB_REG_R:	s = "regr"; break;
	case KGDB_REG_W:	s = "regw"; break;
	case KGDB_CONT:		s = "cont"; break;
	case KGDB_STEP:		s = "step"; break;
	case KGDB_KILL:		s = "kill"; break;
	case KGDB_SIGNAL:	s = "sig "; break;
	case KGDB_EXEC:		s = "exec"; break;
	default:		s = "unk "; break;
	}
	remote_debug("%c %c%c%c%c %s (%02x): ", dir,
		     (type & KGDB_ACK) ? 'A' : '.',
		     (type & KGDB_DELTA) ? 'D' : '.',
		     (type & KGDB_MORE) ? 'M' : '.',
		     (type & KGDB_SEQ) ? '-' : '+',
		     s, type);
	if (buf)
		for (i = 0; i < len; ++i)
			remote_debug("%02x", buf[i]);
	remote_debug("\n");
}

static void
set_remote_text_refs_command(arg, from_tty)
	char *arg;
	int from_tty;
{
	icache = !parse_binary_operation("set remote-text-refs", arg);
}

static void
remote_debug_command(arg, from_tty)
	char *arg;
	int from_tty;
{
	char *name;

	if (kiodebug != 0 && kiodebug != stderr)
		(void)fclose(kiodebug);

	if (arg == 0) {
		kiodebug = 0;
		printf("Remote debugging off.\n");
		return;
	}
	if (arg[0] == '-') {
		kiodebug = stderr;
		name = "stderr";
	} else {
		kiodebug = fopen(arg, "w");
		if (kiodebug == 0) {
			printf("Cannot open '%s'.\n", arg);
			return;
		}
		name = arg;
	}
	printf("Remote debugging output routed to %s.\n", name);
}

/* ARGSUSED */
static void
remote_info(arg, from_tty)
	char *arg;
	int from_tty;
{
	printf("Using %s for text references.\n",
		icache? "local executable" : "remote");
	printf("Protocol debugging is %s.\n", kiodebug? "on" : "off");
	printf("%d spurious input messages.\n", remote_spurious);
	printf("%d input errors; %d output errors; %d sequence errors.\n", 
	       remote_ierrs, remote_oerrs, remote_seqerrs);
}

/* VARARGS */
static void
remote_debug(va_alist)
	va_dcl
{
	register char *cp;
	va_list ap;

	va_start(ap);
	cp = va_arg(ap, char *);
	(void)vfprintf(kiodebug, cp, ap);
	va_end(ap);
	fflush(kiodebug);
}

extern struct cmd_list_element *setlist;

void
_initialize_remote()
{
	add_com("remote-signal", class_run, remote_signal_command,
		"If remote debugging, send interrupt signal to remote.");
	add_cmd("remote-text-refs", class_support, 
		set_remote_text_refs_command,
"Enable/disable use of local executable for text segment references.\n\
If on, all memory read/writes go to remote.\n\
If off, text segment reads use the local executable.",
		&setlist);

	add_com("remote-debug", class_run, remote_debug_command,
"With a file name argument, enables output of remote protocol debugging\n\
messages to said file.  If file is `-', stderr is used.\n\
With no argument, remote debugging is disabled.");

	add_info("remote", remote_info,
		 "Show current settings of remote debugging options.");
}

