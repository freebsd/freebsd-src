/* ==== pthread_attr.h ========================================================
 * Copyright (c) 1993 by Chris Provenzano, proven@mit.edu
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
 *  This product includes software developed by Chris Provenzano.
 * 4. The name of Chris Provenzano may not be used to endorse or promote 
 *	  products derived from this software without specific prior written
 *	  permission.
 *
 * THIS SOFTWARE IS PROVIDED BY CHRIS PROVENZANO ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL CHRIS PROVENZANO BE LIABLE FOR ANY 
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR 
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT 
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF 
 * SUCH DAMAGE.
 *
 * Description : Basic pthread attributes header.
 *
 *  1.00 93/11/03 proven
 *      -Started coding this file.
 */

#define _POSIX_THREAD_ATTR_STACKSIZE

#define	PTHREAD_STACK_DEFAULT	65536

/*
 * New pthread attribute types.
 */
enum pthread_sched_attr {
	SCHED_RR,
	SCHED_IO,
	SCHED_FIFO,
	SCHED_OTHER,
};

typedef struct pthread_attr {
	enum pthread_sched_attr	sched_attr;
	void *					stackaddr_attr;
	size_t					stacksize_attr;
} pthread_attr_t;

/*
 * New functions
 */

__BEGIN_DECLS

int	pthread_attr_init			__P((pthread_attr_t *));
int	pthread_attr_destroy		__P((pthread_attr_t *));
int	pthread_attr_setstacksize	__P((pthread_attr_t *, size_t));
int	pthread_attr_getstacksize	__P((pthread_attr_t *, size_t *));
int	pthread_attr_setstackaddr	__P((pthread_attr_t *, void *));
int	pthread_attr_getstackaddr	__P((pthread_attr_t *, void **));
		
__END_DECLS
