/*-
 * Copyright (c) 2016-2019 Netflix, Inc.
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Author: Randall Stewart <rrs@netflix.com>
 */

#include <sys/types.h>
#include <sys/time.h>
#include <sys/errno.h>
#include <sys/tim_filter.h>

void
reset_time(struct time_filter *tf, uint32_t time_len)
{
	tf->cur_time_limit = time_len;
}

void
reset_time_small(struct time_filter_small *tf, uint32_t time_len)
{
	tf->cur_time_limit = time_len;
}

/*
 * A time filter can be a filter for MIN or MAX. 
 * You call setup_time_filter() with the pointer to
 * the filter structure, the type (FILTER_TYPE_MIN/MAX) and
 * the time length. You can optionally reset the time length
 * later with reset_time().
 *
 * You generally call apply_filter_xxx() to apply the new value
 * to the filter. You also provide a time (now). The filter will
 * age out entries based on the time now and your time limit
 * so that you are always maintaining the min or max in that
 * window of time. Time is a relative thing, it might be ticks
 * in milliseconds, it might be round trip times, its really
 * up to you to decide what it is.
 *
 * To access the current flitered value you can use the macro
 * get_filter_value() which returns the correct entry that
 * has the "current" value in the filter.
 *
 * One thing that used to be here is a single apply_filter(). But
 * this meant that we then had to store the type of filter in
 * the time_filter structure. In order to keep it at a cache
 * line size I split it to two functions. 
 *
 */
int
setup_time_filter(struct time_filter *tf, int fil_type, uint32_t time_len)
{
	uint64_t set_val;
	int i;

	/* 
	 * You must specify either a MIN or MAX filter,
	 * though its up to the user to use the correct
	 * apply.
	 */
	if ((fil_type != FILTER_TYPE_MIN) &&
	    (fil_type != FILTER_TYPE_MAX))
		return(EINVAL);

	if (time_len < NUM_FILTER_ENTRIES)
		return(EINVAL);
		       
	if (fil_type == FILTER_TYPE_MIN)
		set_val = 0xffffffffffffffff;
	else
		set_val = 0;

	for(i=0; i<NUM_FILTER_ENTRIES; i++) {
		tf->entries[i].value = set_val;
		tf->entries[i].time_up = 0;
	}
	tf->cur_time_limit = time_len;
	return(0);
}

int
setup_time_filter_small(struct time_filter_small *tf, int fil_type, uint32_t time_len)
{
	uint32_t set_val;
	int i;

	/* 
	 * You must specify either a MIN or MAX filter,
	 * though its up to the user to use the correct
	 * apply.
	 */
	if ((fil_type != FILTER_TYPE_MIN) &&
	    (fil_type != FILTER_TYPE_MAX))
		return(EINVAL);

	if (time_len < NUM_FILTER_ENTRIES)
		return(EINVAL);
		       
	if (fil_type == FILTER_TYPE_MIN)
		set_val = 0xffffffff;
	else
		set_val = 0;

	for(i=0; i<NUM_FILTER_ENTRIES; i++) {
		tf->entries[i].value = set_val;
		tf->entries[i].time_up = 0;
	}
	tf->cur_time_limit = time_len;
	return(0);
}

static void
check_update_times(struct time_filter *tf, uint64_t value, uint32_t now)
{
	int i, j, fnd;
	uint32_t tim;
	uint32_t time_limit;
	for(i=0; i<(NUM_FILTER_ENTRIES-1); i++) {
		tim = now - tf->entries[i].time_up;
		time_limit = (tf->cur_time_limit * (NUM_FILTER_ENTRIES-i))/NUM_FILTER_ENTRIES;
		if (tim >= time_limit) {
			fnd = 0;
			for(j=(i+1); j<NUM_FILTER_ENTRIES; j++) {
				if (tf->entries[i].time_up < tf->entries[j].time_up) {
					tf->entries[i].value = tf->entries[j].value;
					tf->entries[i].time_up = tf->entries[j].time_up;
					fnd = 1;
					break;
				}
			}
			if (fnd == 0) {
				/* Nothing but the same old entry */
				tf->entries[i].value = value;
				tf->entries[i].time_up = now;
			}
		}
	}
	i = NUM_FILTER_ENTRIES-1;
	tim = now - tf->entries[i].time_up;
	time_limit = (tf->cur_time_limit * (NUM_FILTER_ENTRIES-i))/NUM_FILTER_ENTRIES;
	if (tim >= time_limit) {
		tf->entries[i].value = value;
		tf->entries[i].time_up = now;
	}
}

static void
check_update_times_small(struct time_filter_small *tf, uint32_t value, uint32_t now)
{
	int i, j, fnd;
	uint32_t tim;
	uint32_t time_limit;
	for(i=0; i<(NUM_FILTER_ENTRIES-1); i++) {
		tim = now - tf->entries[i].time_up;
		time_limit = (tf->cur_time_limit * (NUM_FILTER_ENTRIES-i))/NUM_FILTER_ENTRIES;
		if (tim >= time_limit) {
			fnd = 0;
			for(j=(i+1); j<NUM_FILTER_ENTRIES; j++) {
				if (tf->entries[i].time_up < tf->entries[j].time_up) {
					tf->entries[i].value = tf->entries[j].value;
					tf->entries[i].time_up = tf->entries[j].time_up;
					fnd = 1;
					break;
				}
			}
			if (fnd == 0) {
				/* Nothing but the same old entry */
				tf->entries[i].value = value;
				tf->entries[i].time_up = now;
			}
		}
	}
	i = NUM_FILTER_ENTRIES-1;
	tim = now - tf->entries[i].time_up;
	time_limit = (tf->cur_time_limit * (NUM_FILTER_ENTRIES-i))/NUM_FILTER_ENTRIES;
	if (tim >= time_limit) {
		tf->entries[i].value = value;
		tf->entries[i].time_up = now;
	}
}

void
filter_reduce_by(struct time_filter *tf, uint64_t reduce_by, uint32_t now)
{
	int i;
	/* 
	 * Reduce our filter main by reduce_by and
	 * update its time. Then walk other's and
	 * make them the new value too.
	 */
	if (reduce_by < tf->entries[0].value)
		tf->entries[0].value -= reduce_by;
	else
		tf->entries[0].value = 0;
	tf->entries[0].time_up = now;
	for(i=1; i<NUM_FILTER_ENTRIES; i++) {
		tf->entries[i].value = tf->entries[0].value;
		tf->entries[i].time_up = now;
	}
}

void
filter_reduce_by_small(struct time_filter_small *tf, uint32_t reduce_by, uint32_t now)
{
	int i;
	/* 
	 * Reduce our filter main by reduce_by and
	 * update its time. Then walk other's and
	 * make them the new value too.
	 */
	if (reduce_by < tf->entries[0].value)
		tf->entries[0].value -= reduce_by;
	else
		tf->entries[0].value = 0;
	tf->entries[0].time_up = now;
	for(i=1; i<NUM_FILTER_ENTRIES; i++) {
		tf->entries[i].value = tf->entries[0].value;
		tf->entries[i].time_up = now;
	}
}

void
filter_increase_by(struct time_filter *tf, uint64_t incr_by, uint32_t now)
{
	int i;
	/* 
	 * Increase our filter main by incr_by and
	 * update its time. Then walk other's and
	 * make them the new value too.
	 */
	tf->entries[0].value += incr_by;
	tf->entries[0].time_up = now;
	for(i=1; i<NUM_FILTER_ENTRIES; i++) {
		tf->entries[i].value = tf->entries[0].value;
		tf->entries[i].time_up = now;
	}
}

void
filter_increase_by_small(struct time_filter_small *tf, uint32_t incr_by, uint32_t now)
{
	int i;
	/* 
	 * Increase our filter main by incr_by and
	 * update its time. Then walk other's and
	 * make them the new value too.
	 */
	tf->entries[0].value += incr_by;
	tf->entries[0].time_up = now;
	for(i=1; i<NUM_FILTER_ENTRIES; i++) {
		tf->entries[i].value = tf->entries[0].value;
		tf->entries[i].time_up = now;
	}
}

void
forward_filter_clock(struct time_filter *tf, uint32_t ticks_forward)
{
	/*
	 * Bring forward all time values by N ticks. This
	 * postpones expiring slots by that amount.
	 */
	int i;

	for(i=0; i<NUM_FILTER_ENTRIES; i++) {
		tf->entries[i].time_up += ticks_forward;
	}
}

void
forward_filter_clock_small(struct time_filter_small *tf, uint32_t ticks_forward)
{
	/*
	 * Bring forward all time values by N ticks. This
	 * postpones expiring slots by that amount.
	 */
	int i;

	for(i=0; i<NUM_FILTER_ENTRIES; i++) {
		tf->entries[i].time_up += ticks_forward;
	}
}

void
tick_filter_clock(struct time_filter *tf, uint32_t now)
{
	int i;
	uint32_t tim, time_limit;

	/*
	 * We start at two positions back. This
	 * is because the oldest worst value is
	 * preserved always, i.e. it can't expire
	 * due to clock ticking with no updated value.
	 *
	 * The other choice would be to fill it in with
	 * zero, but I don't like that option since
	 * some measurement is better than none (even
	 * if its your oldest measurement).
	 */
	for(i=(NUM_FILTER_ENTRIES-2); i>=0 ; i--) {
		tim = now - tf->entries[i].time_up;
		time_limit = (tf->cur_time_limit * (NUM_FILTER_ENTRIES-i))/NUM_FILTER_ENTRIES;
		if (tim >= time_limit) {
			/*
			 * This entry is expired, pull down
			 * the next one up.
			 */
			tf->entries[i].value = tf->entries[(i+1)].value;
			tf->entries[i].time_up = tf->entries[(i+1)].time_up;
		}
	}
}

void
tick_filter_clock_small(struct time_filter_small *tf, uint32_t now)
{
	int i;
	uint32_t tim, time_limit;

	/*
	 * We start at two positions back. This
	 * is because the oldest worst value is
	 * preserved always, i.e. it can't expire
	 * due to clock ticking with no updated value.
	 *
	 * The other choice would be to fill it in with
	 * zero, but I don't like that option since
	 * some measurement is better than none (even
	 * if its your oldest measurement).
	 */
	for(i=(NUM_FILTER_ENTRIES-2); i>=0 ; i--) {
		tim = now - tf->entries[i].time_up;
		time_limit = (tf->cur_time_limit * (NUM_FILTER_ENTRIES-i))/NUM_FILTER_ENTRIES;
		if (tim >= time_limit) {
			/*
			 * This entry is expired, pull down
			 * the next one up.
			 */
			tf->entries[i].value = tf->entries[(i+1)].value;
			tf->entries[i].time_up = tf->entries[(i+1)].time_up;
		}
	}
}

uint32_t
apply_filter_min(struct time_filter *tf, uint64_t value, uint32_t now)
{
	int i, j;

	if (value <= tf->entries[0].value) {
		/* Zap them all */
		for(i=0; i<NUM_FILTER_ENTRIES; i++) {
			tf->entries[i].value = value;
			tf->entries[i].time_up = now;
		}
		return (tf->entries[0].value);
	}
	for (j=1; j<NUM_FILTER_ENTRIES; j++) {
		if (value <= tf->entries[j].value) {
			for(i=j; i<NUM_FILTER_ENTRIES; i++) {
				tf->entries[i].value = value;
				tf->entries[i].time_up = now;
			}
			break;
		}
	}
	check_update_times(tf, value, now);
	return (tf->entries[0].value);
}

uint32_t
apply_filter_min_small(struct time_filter_small *tf,
		       uint32_t value, uint32_t now)
{
	int i, j;

	if (value <= tf->entries[0].value) {
		/* Zap them all */
		for(i=0; i<NUM_FILTER_ENTRIES; i++) {
			tf->entries[i].value = value;
			tf->entries[i].time_up = now;
		}
		return (tf->entries[0].value);
	}
	for (j=1; j<NUM_FILTER_ENTRIES; j++) {
		if (value <= tf->entries[j].value) {
			for(i=j; i<NUM_FILTER_ENTRIES; i++) {
				tf->entries[i].value = value;
				tf->entries[i].time_up = now;
			}
			break;
		}
	}
	check_update_times_small(tf, value, now);
	return (tf->entries[0].value);
}

uint32_t
apply_filter_max(struct time_filter *tf, uint64_t value, uint32_t now)
{
	int i, j;

	if (value >= tf->entries[0].value) {
		/* Zap them all */
		for(i=0; i<NUM_FILTER_ENTRIES; i++) {
			tf->entries[i].value = value;
			tf->entries[i].time_up = now;
		}
		return (tf->entries[0].value);
	}
	for (j=1; j<NUM_FILTER_ENTRIES; j++) {
		if (value >= tf->entries[j].value) {
			for(i=j; i<NUM_FILTER_ENTRIES; i++) {
				tf->entries[i].value = value;
				tf->entries[i].time_up = now;
			}
			break;
		}
	}
	check_update_times(tf, value, now);
	return (tf->entries[0].value);
}

uint32_t
apply_filter_max_small(struct time_filter_small *tf,
		       uint32_t value, uint32_t now)
{
	int i, j;

	if (value >= tf->entries[0].value) {
		/* Zap them all */
		for(i=0; i<NUM_FILTER_ENTRIES; i++) {
			tf->entries[i].value = value;
			tf->entries[i].time_up = now;
		}
		return (tf->entries[0].value);
	}
	for (j=1; j<NUM_FILTER_ENTRIES; j++) {
		if (value >= tf->entries[j].value) {
			for(i=j; i<NUM_FILTER_ENTRIES; i++) {
				tf->entries[i].value = value;
				tf->entries[i].time_up = now;
			}
			break;
		}
	}
	check_update_times_small(tf, value, now);
	return (tf->entries[0].value);
}
