/*
 * Copyright (c) 1998 John Birrell <jb@cimlogic.com.au>.
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
 *
 * $Id: _atomic_lock.S,v 1.1 1998/04/29 09:36:03 jb Exp $
 *
 */

#include "SYS.h"

/*
 * Atomicly lock a location with an identifier provided the location
 * is not currently locked.
 *
 * long _atomic_lock(long *);
 * v0 will contain the return value (zero if lock obtained).
 */
LEAF(_atomic_lock,0)
	LDGP(pv)

	/* Get the existing lock value and lock memory: */
	ldq_l	v0, 0(a0)

	/* Branch if already locked: */
	bne	v0, already_locked

	/* Not locked, so store 1: */
	mov	1, t0
	stq_c	t0, 0(a0)

	/* Obtained the lock: */
	br	done

already_locked:
	/* Already locked so put the value back and unlock memory: */
	stq_c	v0, 0(a0)
	
done:
	RET
END(_atomic_lock)
