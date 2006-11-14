/*
 * Copyright (c) 1995-1998 John Birrell <jb@cimlogic.com.au>.
 * All rights reserved.
 * copyright Douglas Santry 1996
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the above copyright is retained
 * in the source form.
 *
 * THIS SOFTWARE IS PROVIDED BY Douglas Santry AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE  
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE   
 * ARE DISCLAIMED.  IN NO EVENT SHALL Douglas Santry OR CONTRIBUTORS BE LIABLE 
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL   
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS      
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY 
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF  
 * SUCH DAMAGE.    
 */

#if defined(LIBC_RCS) && !defined(lint)
	.text
	.asciz "$FreeBSD$"
#endif /* LIBC_RCS and not lint */

#include <machine/asm.h>

/*
 * Atomicly lock a location with an identifier provided the location
 * is not currently locked.
 *
 * long _atomic_lock(long *);
 * eax will contain the return value (zero if lock obtained).
 */
ENTRY(_atomic_lock)
	movl	4(%esp), %ecx
	movl	$1, %eax
	xchg	%eax, (%ecx)
	ret

