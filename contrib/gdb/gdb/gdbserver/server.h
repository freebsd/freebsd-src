/* Common definitions for remote server for GDB.
   Copyright 1993, 1995, 1997, 1998, 1999, 2000, 2002
   Free Software Foundation, Inc.

   This file is part of GDB.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

#ifndef SERVER_H
#define SERVER_H

#include "config.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>


/* FIXME:  Both of these should be autoconf'd for.  */
#define NORETURN
typedef long long CORE_ADDR;

#include "regcache.h"
#include "gdb/signals.h"

#include <setjmp.h>

/* Target-specific functions */

int create_inferior (char *program, char **allargs);
void kill_inferior (void);
void fetch_inferior_registers (int regno);
void store_inferior_registers (int regno);
int mythread_alive (int pid);
void myresume (int step, int signo);
unsigned char mywait (char *status);
void read_inferior_memory (CORE_ADDR memaddr, char *myaddr, int len);
int write_inferior_memory (CORE_ADDR memaddr, char *myaddr, int len);
int create_inferior ();
void initialize_low ();

/* Target-specific variables */

extern char *registers;

/* Public variables in server.c */

extern int cont_thread;
extern int general_thread;
extern int thread_from_wait;
extern int old_thread_from_wait;

extern jmp_buf toplevel;
extern int inferior_pid;

/* Functions from remote-utils.c */

int putpkt (char *buf);
int getpkt (char *buf);
void remote_open (char *name);
void remote_close (void);
void write_ok (char *buf);
void write_enn (char *buf);
void enable_async_io (void);
void disable_async_io (void);
void convert_ascii_to_int (char *from, char *to, int n);
void convert_int_to_ascii (char *from, char *to, int n);
void prepare_resume_reply (char *buf, char status, unsigned char sig);

void decode_m_packet (char *from, CORE_ADDR * mem_addr_ptr,
		      unsigned int *len_ptr);
void decode_M_packet (char *from, CORE_ADDR * mem_addr_ptr,
		      unsigned int *len_ptr, char *to);

/* Functions from ``signals.c''.  */
enum target_signal target_signal_from_host (int hostsig);
int target_signal_to_host_p (enum target_signal oursig);
int target_signal_to_host (enum target_signal oursig);

/* Functions from utils.c */

void perror_with_name (char *string);
void error (const char *string,...);
void fatal (const char *string,...);
void warning (const char *string,...);



/* Maximum number of bytes to read/write at once.  The value here
   is chosen to fill up a packet (the headers account for the 32).  */
#define MAXBUFBYTES(N) (((N)-32)/2)

/* Buffer sizes for transferring memory, registers, etc.  Round up PBUFSIZ to
   hold all the registers, at least.  */
#define	PBUFSIZ ((registers_length () + 32 > 2000) \
		 ? (registers_length () + 32) \
		 : 2000)

#endif /* SERVER_H */
