/*
* Copyright (c) 1983, 1993	The Regents of the University of California.
* Copyright (c) 1993 Digital Equipment Corporation.
* Copyright (c) 2012 G. Vanem <gvanem@yahoo.no>.
* Copyright (c) 2017 Ali Abdulkadir <autostart.ini@gmail.com>.
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
*/

#ifndef ND_GETSERVENT_H
#define ND_GETSERVENT_H

#ifdef _NETDB_H_
/* Just in case... */
#error netdb.h and getservent.h are incompatible
#else
#define _NETDB_H_
#endif

#ifdef _WIN32
#define __PATH_SYSROOT "SYSTEMROOT"
#define __PATH_ETC_INET "\\System32\\drivers\\etc\\"
#define __PATH_SERVICES "services"
#else
/*
* The idea here is to be able to replace "PREFIX" in __PATH_SYSROOT with a variable
* that could, for example, point to an alternative install location.
*/
#define __PATH_SYSROOT "PREFIX"
#define __PATH_ETC_INET "/etc/"
#define __PATH_SERVICES __PATH_ETC_INET"services"
#endif

#define MAXALIASES 35

void endservent (void);
struct servent *getservent(void);
void setservent (int f);

#endif /* ! ND_GETSERVENT_H */
