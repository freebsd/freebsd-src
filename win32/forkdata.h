/*$Header: /p/tcsh/cvsroot/tcsh/win32/forkdata.h,v 1.4 2004/05/19 18:22:27 christos Exp $*/
/*-
 * Copyright (c) 1980, 1991 The Regents of the University of California.
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
 * 3. Neither the name of the University nor the names of its contributors
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


#ifndef FORK_DATA_H
#define FORK_DATA_H

#include <setjmp.h>

/* 
 * This structure is copied by fork() to the child process. It 
 * contains variables of national importance
 *
 * Thanks to Mark Tucker for the idea. tcsh now finally works on
 * alphas.
 * -amol
 */
typedef struct _fork_data {
	unsigned long _forked;
	void  *_fork_stack_begin;
	void  *_fork_stack_end;
	unsigned long _heap_size;
	HANDLE _hforkparent, _hforkchild;
	void * _heap_base;
	void * _heap_top;
	jmp_buf _fork_context;
} ForkData;

#define __forked gForkData._forked
#define __fork_stack_begin gForkData._fork_stack_begin
#define __fork_stack_end gForkData._fork_stack_end
#define __hforkparent gForkData._hforkparent
#define __hforkchild gForkData._hforkchild
#define __fork_context gForkData._fork_context
#define __heap_base gForkData._heap_base
#define __heap_size gForkData._heap_size
#define __heap_top gForkData._heap_top

extern ForkData gForkData;

#ifdef NTDBG
#define FORK_TIMEOUT INFINITE
#else
#define FORK_TIMEOUT (50000)
#endif /*!NTDBG */



#endif FORK_DATA_H
