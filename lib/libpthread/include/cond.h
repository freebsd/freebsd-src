/* ==== cond.h ============================================================
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
 * Description : mutex header.
 *
 *  1.00 93/10/30 proven
 *      -Started coding this file.
 */

/*
 * New cond structures
 */
enum pthread_cond_type {
	COND_TYPE_FAST,
	COND_TYPE_STATIC_FAST,
	COND_TYPE_METERED,
	COND_TYPE_DEBUG, /* Debug conds will have lots of options */
	COND_TYPE_MAX
};

typedef struct pthread_cond {
	enum pthread_cond_type	c_type;
	struct pthread_queue	c_queue;
	semaphore				c_lock;	
	void				  * c_data;
	long					c_flags;
} pthread_cond_t;

typedef struct pthread_cond_attr {
	enum pthread_cond_type	c_type;
	long					c_flags;
} pthread_condattr_t;

/*
 * Flags for conds.
 */
#define COND_FLAGS_PRIVATE	0x01
#define COND_FLAGS_INITED	0x02
#define COND_FLAGS_BUSY		0x04

/*
 * Static cond initialization values.
 */
#define PTHREAD_COND_INITIALIZER	\
{ COND_TYPE_STATIC_FAST, PTHREAD_QUEUE_INITIALIZER, \
	 NULL, SEMAPHORE_CLEAR, COND_FLAGS_INITED }

/*
 * New functions
 */

__BEGIN_DECLS

int     pthread_cond_init  		__P((pthread_cond_t *, pthread_condattr_t *));
int     pthread_cond_wait      	__P((pthread_cond_t *, pthread_mutex_t *));
int     pthread_cond_signal    	__P((pthread_cond_t *));
int     pthread_cond_broadcast	__P((pthread_cond_t *));
int     pthread_cond_destroy   	__P((pthread_cond_t *));

__END_DECLS
	
