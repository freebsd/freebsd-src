/*-
 * Copyright (c) 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	@(#)terminal.h	8.1 (Berkeley) 6/6/93
 */

#define	INCLUDED_TERMINAL

/*
 * In the situation where we have a copy of the terminal screen in front
 * of us, here are some macros to deal with them.
 */

#define TermAttributes(x)	(TermIsStartField(x)? GetTerminal(x)&0xff : \
				    GetTerminal(WhereTermAttrByte(x))&0xff)
#define TermIsStartField(x)	((GetTerminal(x)&ATTR_MASK) == ATTR_MASK)
#define TermNewField(p,a)	SetTerminal(p, (a)|ATTR_MASK)
#define TermDeleteField(p)	SetTerminal(p, 0)
#define TermIsNonDisplay(x)	\
		    ((TermAttributes(x)&ATTR_DSPD_MASK) == ATTR_DSPD_NONDISPLAY)
#define TermIsHighlighted(x) \
		(((TermAttributes(x)&ATTR_DSPD_MASK) == ATTR_DSPD_HIGH) \
				    && !TermIsStartField(x))

#define TerminalCharacterAttr(c,p,a)	(IsNonDisplayAttr(a) ? ' ':c)
#define TerminalCharacter(c,p)	TerminalCharacterAttr(c,p,FieldAttributes(p))

	/*
	 * Is the screen formatted?  Some algorithms change depending
	 * on whether there are any attribute bytes lying around.
	 */
#define	TerminalFormattedScreen() \
	    ((WhereTermAttrByte(0) != 0) || ((GetTerminal(0)&ATTR_MASK) == ATTR_MASK))

#define NeedToRedisplayFields(p) ((TermIsNonDisplay(p) != IsNonDisplay(p)) || \
				(TermIsHighlighted(p) != IsHighlighted(p)))
#define NeedToRedisplayFieldsAttr(p,c) ( \
			(TermIsNonDisplay(p) != IsNonDisplayAttr(c)) || \
			(TermIsHighlighted(p) != IsHighlightedAttr(c)))

#define NotVisuallyCompatibleAttributes(p,c,d) ( \
			(IsNonDisplayAttr(c) != IsNonDisplayAttr(d)) || \
			(IsHighlightedAttr(c) != IsHighlightedAttr(d)))

#define NeedToRedisplayAttr(c,p,a) \
			((c != GetTerminal(p)) || NeedToRedisplayFieldsAttr(p,a))
#define NeedToRedisplay(c,p)	NeedToRedisplayAttr(c,p,FieldAttributes(p))


#define GetTerminal(i)		GetGeneric(i, Terminal)
#define GetTerminalPointer(p)	GetGenericPointer(p)
#define SetTerminal(i,c)	SetGeneric(i,c,Terminal)
