/*-
 * Copyright (c) 1997 Brian Somers <brian@Awfulhak.org>
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
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	$Id: throughput.c,v 1.5 1998/05/21 21:48:41 brian Exp $
 */

#include <sys/types.h>

#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <time.h>

#include "log.h"
#include "timer.h"
#include "throughput.h"
#include "descriptor.h"
#include "prompt.h"

void
throughput_init(struct pppThroughput *t)
{
  int f;

  t->OctetsIn = t->OctetsOut = 0;
  for (f = 0; f < SAMPLE_PERIOD; f++)
    t->SampleOctets[f] = 0;
  t->OctetsPerSecond = t->BestOctetsPerSecond = t->nSample = 0;
  t->BestOctetsPerSecondTime = time(NULL);
  memset(&t->Timer, '\0', sizeof t->Timer);
  t->Timer.name = "throughput";
  t->uptime = 0;
  t->rolling = 0;
  throughput_stop(t);
}

void
throughput_disp(struct pppThroughput *t, struct prompt *prompt)
{
  int secs_up;

  secs_up = t->uptime ? time(NULL) - t->uptime : 0;
  prompt_Printf(prompt, "Connect time: %d secs\n", secs_up);
  if (secs_up == 0)
    secs_up = 1;
  prompt_Printf(prompt, "%ld octets in, %ld octets out\n",
                t->OctetsIn, t->OctetsOut);
  if (t->rolling) {
    prompt_Printf(prompt, "  overall   %5ld bytes/sec\n",
                  (t->OctetsIn+t->OctetsOut)/secs_up);
    prompt_Printf(prompt, "  currently %5d bytes/sec\n", t->OctetsPerSecond);
    prompt_Printf(prompt, "  peak      %5d bytes/sec on %s\n",
                  t->BestOctetsPerSecond, ctime(&t->BestOctetsPerSecondTime));
  } else
    prompt_Printf(prompt, "Overall %ld bytes/sec\n",
                  (t->OctetsIn+t->OctetsOut)/secs_up);
}


void
throughput_log(struct pppThroughput *t, int level, const char *title)
{
  if (t->uptime) {
    int secs_up;

    secs_up = t->uptime ? time(NULL) - t->uptime : 0;
    if (title)
      log_Printf(level, "%s: Connect time: %d secs: %ld octets in, %ld octets"
                " out\n", title, secs_up, t->OctetsIn, t->OctetsOut);
    else
      log_Printf(level, "Connect time: %d secs: %ld octets in, %ld octets out\n",
                secs_up, t->OctetsIn, t->OctetsOut);
    if (secs_up == 0)
      secs_up = 1;
    if (t->rolling)
      log_Printf(level, " total %ld bytes/sec, peak %d bytes/sec on %s\n",
                (t->OctetsIn+t->OctetsOut)/secs_up, t->BestOctetsPerSecond,
                ctime(&t->BestOctetsPerSecondTime));
    else
      log_Printf(level, " total %ld bytes/sec\n",
                (t->OctetsIn+t->OctetsOut)/secs_up);
  }
}

static void
throughput_sampler(void *v)
{
  struct pppThroughput *t = (struct pppThroughput *)v;
  u_long old;

  timer_Stop(&t->Timer);

  old = t->SampleOctets[t->nSample];
  t->SampleOctets[t->nSample] = t->OctetsIn + t->OctetsOut;
  t->OctetsPerSecond = (t->SampleOctets[t->nSample] - old) / SAMPLE_PERIOD;
  if (t->BestOctetsPerSecond < t->OctetsPerSecond) {
    t->BestOctetsPerSecond = t->OctetsPerSecond;
    t->BestOctetsPerSecondTime = time(NULL);
  }
  if (++t->nSample == SAMPLE_PERIOD)
    t->nSample = 0;

  timer_Start(&t->Timer);
}

void
throughput_start(struct pppThroughput *t, const char *name, int rolling)
{
  timer_Stop(&t->Timer);
  throughput_init(t);
  t->rolling = rolling ? 1 : 0;
  time(&t->uptime);
  if (t->rolling) {
    t->Timer.load = SECTICKS;
    t->Timer.func = throughput_sampler;
    t->Timer.name = name;
    t->Timer.arg = t;
    timer_Start(&t->Timer);
  }
}

void
throughput_stop(struct pppThroughput *t)
{
  timer_Stop(&t->Timer);
}

void
throughput_addin(struct pppThroughput *t, int n)
{
  t->OctetsIn += n;
}

void
throughput_addout(struct pppThroughput *t, int n)
{
  t->OctetsOut += n;
}
