/*
 * $Id$
 */

#define SAMPLE_PERIOD 5

struct pppThroughput {
  time_t uptime;
  u_long OctetsIn;
  u_long OctetsOut;
  u_long SampleOctets[SAMPLE_PERIOD];
  int OctetsPerSecond;
  int BestOctetsPerSecond;
  int nSample;
  struct pppTimer Timer;
};

extern void throughput_init(struct pppThroughput *);
extern void throughput_disp(struct pppThroughput *, FILE *);
extern void throughput_log(struct pppThroughput *, int, const char *);
extern void throughput_start(struct pppThroughput *);
extern void throughput_stop(struct pppThroughput *);
extern void throughput_addin(struct pppThroughput *, int);
extern void throughput_addout(struct pppThroughput *, int);
