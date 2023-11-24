#ifndef	_TERMIOS_H
#define	_TERMIOS_H

typedef unsigned char host_cc_t;
typedef unsigned int host_speed_t;
typedef unsigned int host_tcflag_t;

#define HOST_NCCS 32

#include "termios_arch.h"

#define HOST_TCSANOW		0
#define HOST_TCSADRAIN		1
#define HOST_TCSAFLUSH		2

int host_tcgetattr (int, struct host_termios *);
int host_tcsetattr (int, int, const struct host_termios *);

void host_cfmakeraw(struct host_termios *);
int host_cfsetispeed(struct host_termios *, host_speed_t);
int host_cfsetospeed(struct host_termios *, host_speed_t);
int host_cfsetspeed(struct host_termios *, host_speed_t);

#endif
