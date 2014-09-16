/*-
 * Copyright (c) 2014 SRI International
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract (FA8750-10-C-0237)
 * ("CTSRD"), as part of the DARPA CRASH research programme.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/time.h>

#include <machine/cheri.h>
#include <machine/cheric.h>

#include <cheri/cheri_system.h>

#include <time.h>

int
clock_gettime(clockid_t clock_id, struct timespec *tp)
{

	return(cheri_system_clock_gettime(clock_id,
	    cheri_ptr(tp, sizeof(struct timespec))));
}

/*
 * Implement gettimeofday() in terms of clock_gettime() to reduce system
 * interfaces.  As with FreeBSD's standard gettimeofday() tzp is ignored.
 */
int
gettimeofday(struct timeval *tp, struct timezone *tzp __unused)
{
	struct timespec t;

	if (tp == NULL)
		return (0);

	if (cheri_system_clock_gettime(CLOCK_REALTIME,
	    cheri_ptr(&t, sizeof(t))) != 0)
		return (-1);

	tp->tv_sec = t.tv_sec;
	tp->tv_usec = t.tv_nsec / 1000;

	return (0);
}

/*
 * Implement time() in terms of clock_gettime() to reduce system interfaces.
 */
time_t
time(time_t *tloc)
{
	struct timespec t;

	if (cheri_system_clock_gettime(CLOCK_REALTIME, &t) != 0)
		return ((time_t)-1);

	if (tloc != NULL)
		*tloc = t.tv_sec;

	return (t.tv_sec);
}
