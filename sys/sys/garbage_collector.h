#ifndef __garbage_collector_h__
#define __garbage_collector_h__
/*-
 * * Copyright (c) 2012, by Adara Networks. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * a) Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * b) Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the distribution.
 *
 * c) Neither the name of Adara Networks,nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/time.h>
#include <sys/queue.h>


typedef void (*garbage_func) (void *);

struct garbage {
	TAILQ_ENTRY(garbage) next;
	garbage_func gf;
	struct timeval purge_time;
	struct timeval purged_at;
	void *junk;
	char *func;
	int line;
	uint8_t on_gc_list;
};

TAILQ_HEAD(garbage_list, garbage);
#ifdef _KERNEL
#define TV_TO_TICKS(tv) (((tv)->tv_sec * hz) + ((tv)->tv_usec/hz))
/*
 * You must pass a gc init'd structure that is
 * saved someplace by you, normally inside the structure
 * the garbage collector will be calling your function
 * to delete within. You should call garbage_collect_init() on
 * this structure before use.
 *
 * The garbage function f is simply something that
 * GC calls to free the garbage you want thrown
 * away. The timeval expire is how long it must
 * be kept alive until. Note that in selecting this
 * time keep in mine it may be longer until GC gets
 * to it.
 *
 * When using reference counting, you should make sure
 * the function you call f() checks the reference. If you
 * have picked way too short of time, its possible that
 * you had a collision between someone allocating and
 * someone deleting the "last" reference. This will show
 * up in two ways.
 * a) The gc will get called a second time to garbage_collect_add()
 *    which will cause an extension to the time.
 * <or>
 * b) Your function will get called to destroy the 
 *    memory, and it will have a reference up. This
 *    is less likely <a> is more typical but can happen
 *    if the expire time is very short.
 *
 * You can re-add the memory if <b> occurs, but be sure
 * to re-call garbage_collect_init() before re-submitting
 * the memory to the GC.
 */

int 
garbage_collect_add(struct garbage *m, garbage_func f,
	void *gar, struct timeval *expire, const char *func, int line);


/* Call this *before* you use your structure */
int 
garbage_collect_init(struct garbage *m);

#endif
#endif
