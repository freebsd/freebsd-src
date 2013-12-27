/*  Copyright (C) 1996 N.M. Maclaren
    Copyright (C) 1996 The University of Cambridge

This includes all of the 'safe' headers and definitions used across modules.
No changes should be needed for any system that is even remotely like Unix. */



#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>



#define VERSION         "1.6"          /* Just the version string */
#define MAX_SOCKETS        10          /* Maximum number of addresses */

#ifndef LOCKNAME
#    define LOCKNAME "/etc/sntp.pid"  /* Stores the pid */
#endif
#ifndef SAVENAME
#    define SAVENAME "/etc/sntp.state" /* Stores the recovery state */
#endif



/* Defined in main.c */

#define op_client           1          /* Behave as a challenge client */
#define op_listen           2          /* Behave as a listening client */

extern const char *argv0;

extern int verbose, operation;

extern const char *lockname;

extern void fatal (int syserr, const char *message, const char *insert);



/* Defined in unix.c */

extern void do_nothing (int seconds);

extern int ftty (FILE *file);

extern void set_lock (int lock);

extern void log_message (const char *message);



/* Defined in internet.c */

/* extern void find_address (struct in_addr *address, int *port, char *hostname,
    int timespan); */

#define PREF_FAM_INET	1
#define PREF_FAM_INET6	2
extern void preferred_family(int);


/* Defined in socket.c */

extern void open_socket (int which, char *hostnames, int timespan);

extern void write_socket (int which, void *packet, int length);

extern int read_socket (int which, void *packet, int length, int waiting);

extern int flush_socket (int which);

extern void close_socket (int which);



/* Defined in timing.c */

extern double current_time (double offset);

extern time_t convert_time (double value, int *millisecs);

extern void adjust_time (double difference, int immediate, double ignore);
