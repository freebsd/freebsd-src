/*
 * Copyright (c) University of British Columbia, 1984
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Laboratory for Computation Vision and the Computer Science Department
 * of the University of British Columbia.
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
 *	from: @(#)x25err.h	7.2 (Berkeley) 5/11/90
 *	$Id: x25err.h,v 1.3 1993/11/07 17:47:31 wollman Exp $
 */

#ifndef _NETCCITT_X25ERR_H_
#define _NETCCITT_X25ERR_H_ 1

/* 
 *  
 *  X.25 Reset and Clear errors and diagnostics.  These values are 
 *  returned in the u_error field of the u structure.
 *
 */

#define EXRESET		100	/* Reset: call reset			*/
#define EXROUT		101	/* Reset: out of order			*/
#define EXRRPE		102	/* Reset: remote procedure error	*/
#define EXRLPE		103	/* Reset: local procedure error		*/
#define EXRNCG		104	/* Reset: network congestion		*/

#define EXCLEAR		110	/* Clear: call cleared			*/
#define EXCBUSY 	111	/* Clear: number busy			*/
#define EXCOUT		112	/* Clear: out of order			*/
#define EXCRPE		113	/* Clear: remote procedure error	*/
#define EXCRRC		114	/* Clear: collect call refused		*/
#define EXCINV		115	/* Clear: invalid call			*/
#define EXCAB		116	/* Clear: access barred			*/
#define EXCLPE		117	/* Clear: local procedure error		*/
#define EXCNCG		118	/* Clear: network congestion		*/
#define EXCNOB		119	/* Clear: not obtainable		*/

#endif /* _NETCCITT_X25ERR_H_ */
