/*
 * $Id$
 */

#include <sys/param.h>

#include <stdio.h>
#include <time.h>
#include <netinet/in.h>

#include "timer.h"
#include "throughput.h"
#include "defs.h"
#include "loadalias.h"
#include "command.h"
#include "vars.h"
#include "mbuf.h"
#include "log.h"

void
throughput_init(struct pppThroughput *t)
{
  int f;

  t->OctetsIn = t->OctetsOut = 0;
  for (f = 0; f < SAMPLE_PERIOD; f++)
    t->SampleOctets[f] = 0;
  t->OctetsPerSecond = t->BestOctetsPerSecond = t->nSample = 0;
  throughput_stop(t);
}

void
throughput_disp(struct pppThroughput *t, FILE *f)
{
  int secs_up;

  secs_up = time(NULL) - t->uptime;
  fprintf(f, "Connect time: %d secs\n", secs_up);
  if (secs_up == 0)
    secs_up = 1;
  fprintf(f, "%ld octets in, %ld octets out\n", t->OctetsIn, t->OctetsOut);
  if (Enabled(ConfThroughput)) {
    fprintf(f, "  overall   %5ld bytes/sec\n",
            (t->OctetsIn+t->OctetsOut)/secs_up);
    fprintf(f, "  currently %5d bytes/sec\n", t->OctetsPerSecond);
    fprintf(f, "  peak      %5d bytes/sec\n", t->BestOctetsPerSecond);
  } else
    fprintf(f, "Overall %ld bytes/sec\n", (t->OctetsIn+t->OctetsOut)/secs_up);
}


void
throughput_log(struct pppThroughput *t, int level, const char *title)
{
  if (t->uptime) {
    int secs_up;

    secs_up = time(NULL) - t->uptime;
    if (title)
      LogPrintf(level, "%s: Connect time: %d secs: %ld octets in, %ld octets"
                " out\n", title, secs_up, t->OctetsIn, t->OctetsOut);
    else
      LogPrintf(level, "Connect time: %d secs: %ld octets in, %ld octets out\n",
                secs_up, t->OctetsIn, t->OctetsOut);
    if (secs_up == 0)
      secs_up = 1;
    if (Enabled(ConfThroughput))
      LogPrintf(level, " total %ld bytes/sec, peak %d bytes/sec\n",
                (t->OctetsIn+t->OctetsOut)/secs_up, t->BestOctetsPerSecond);
    else
      LogPrintf(level, " total %ld bytes/sec\n",
                (t->OctetsIn+t->OctetsOut)/secs_up);
  }
}

static void
throughput_sampler(struct pppThroughput *t)
{
  u_long old;

  StopTimer(&t->Timer);
  t->Timer.state = TIMER_STOPPED;

  old = t->SampleOctets[t->nSample];
  t->SampleOctets[t->nSample] = t->OctetsIn + t->OctetsOut;
  t->OctetsPerSecond = (t->SampleOctets[t->nSample] - old) / SAMPLE_PERIOD;
  if (t->BestOctetsPerSecond < t->OctetsPerSecond)
    t->BestOctetsPerSecond = t->OctetsPerSecond;
  if (++t->nSample == SAMPLE_PERIOD)
    t->nSample = 0;

  StartTimer(&t->Timer);
}

void
throughput_start(struct pppThroughput *t)
{
  throughput_init(t);
  time(&t->uptime);
  if (Enabled(ConfThroughput)) {
    t->Timer.state = TIMER_STOPPED;
    t->Timer.load = SECTICKS;
    t->Timer.func = throughput_sampler;
    t->Timer.arg = t;
    StartTimer(&t->Timer);
  }
}

void
throughput_stop(struct pppThroughput *t)
{
  if (Enabled(ConfThroughput))
    StopTimer(&t->Timer);
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
