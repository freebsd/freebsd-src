/* Common definitions for remote server for GDB.
   Copyright (C) 1993 Free Software Foundation, Inc.

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
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#include "defs.h"
#include <setjmp.h>

void read_inferior_memory ();
unsigned char mywait ();
void myresume();
int mythread_alive ();
int create_inferior ();

extern char registers[];
int inferior_pid;
extern int cont_thread;
extern int general_thread;
extern int thread_from_wait;
extern int old_thread_from_wait;

int remote_send ();
int putpkt ();
int getpkt ();
void remote_open ();
void write_ok ();
void write_enn ();
void convert_ascii_to_int ();
void convert_int_to_ascii ();
void prepare_resume_reply ();
void decode_m_packet ();
void decode_M_packet ();

jmp_buf toplevel;

void perror_with_name ();
