/*
 * Copyright (c) 1999, 2000
 * Intel Corporation.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 * 
 *    This product includes software developed by Intel Corporation and
 *    its contributors.
 * 
 * 4. Neither the name of Intel Corporation or its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY INTEL CORPORATION AND CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL INTEL CORPORATION OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 * 
 */

#ifndef lint
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

#include <time.h>
#include <sys/time.h>

/*
// Accurate only for the past couple of centuries;
// that will probably do.
//
// (#defines From FreeBSD 3.2 lib/libc/stdtime/tzfile.h)
*/

#define isleap(y)	(((y) % 4) == 0 && (((y) % 100) != 0 || ((y) % 400) == 0))
#define SECSPERHOUR ( 60*60 )
#define SECSPERDAY	(24 * SECSPERHOUR)


time_t
time(time_t *tloc)
{
	return 0;		/* XXX */
}
