/*
 * Copyright (c) 1995 - 2000 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "kip.h"

RCSID("$Id: common.c,v 1.13.2.4 2000/10/18 23:31:51 assar Exp $");

sig_atomic_t disconnect = 0;
int isserver = 0;

/*
 * Copy packets from `tundev' to `netdev' or vice versa.
 * Mode is used when reading from `tundev'
 */

int
copy_packets (int tundev, int netdev, int mtu, des_cblock *iv,
	      des_key_schedule schedule)
{
     des_cblock iv1, iv2;
     int num1 = 0, num2 = 0;
     u_char *buf;

     buf = malloc (mtu + 2);
     if (buf == NULL) {
	 warnx("malloc(%d) failed", mtu);
	 return 1;
     }

     memcpy (&iv1, iv, sizeof(iv1));
     memcpy (&iv2, iv, sizeof(iv2));
     while(!disconnect) {
	  fd_set fdset;
	  int ret, len;

	  if (tundev >= FD_SETSIZE || netdev >= FD_SETSIZE) {
	      warnx ("fd too large");
	      return 1;
	  }

	  FD_ZERO(&fdset);
	  FD_SET(tundev, &fdset);
	  FD_SET(netdev, &fdset);

	  ret = select (max(tundev, netdev)+1, &fdset, NULL, NULL, NULL);
	  if (ret < 0) {
	      if (errno == EINTR)
		  continue;
	      warn ("select");
	      return 1;
	  }
	  if (FD_ISSET(tundev, &fdset)) {
	       ret = read (tundev, buf + 2, mtu);
	       if (ret == 0)
		    return 0;
	       if (ret < 0) {
		    if (errno == EINTR)
			 continue;
		    else { 
			warn("read");
			return ret;
		    }
	       }
	       buf[0] = ret >> 8;
	       buf[1] = ret & 0xFF;
	       ret += 2;
	       des_cfb64_encrypt (buf, buf, ret, schedule,
				  &iv1, &num1, DES_ENCRYPT);
	       ret = krb_net_write (netdev, buf, ret);
	       if (ret < 0) {
		   warn("write");
		   return ret;
	       }
	  }
	  if (FD_ISSET(netdev, &fdset)) {
	       ret = read (netdev, buf, 2);
	       if (ret == 0)
		    return 0;
	       if (ret < 0) {
		    if (errno == EINTR)
			 continue;
		    else { 
			warn("read");
			return ret;
		    }
	       }
	       des_cfb64_encrypt (buf, buf, 2, schedule,
				  &iv2, &num2, DES_DECRYPT);
	       len = (buf[0] << 8 ) | buf[1];
	       if (len > mtu) {
		   fatal (-1, "buffer too large", schedule, &iv2);
		   return -1;
	       }

	       if (len == 0) {
		   len = read (netdev, buf, mtu);
		   if (len < 1)
		       len = 1;
		   buf[len-1] = '\0';

		   fatal (-1, buf, schedule, &iv2);
		   return -1;
	       }

	       ret = krb_net_read (netdev, buf + 2, len);
	       if (ret == 0)
		    return 0;
	       if (ret < 0) {
		    if (errno == EINTR)
			 continue;
		    else { 
			warn("read");
			return ret;
		    }
	       }
	       des_cfb64_encrypt (buf + 2, buf + 2, len, schedule,
				  &iv2, &num2, DES_DECRYPT);
	       ret = krb_net_write (tundev, buf + 2, len);
	       if (ret < 0) {
		   warn("write");
		   return ret;
	       }
	  }
     }
     return 0;
}

/*
 * Signal handler that justs waits for the children when they die.
 */

RETSIGTYPE
childhandler (int sig)
{
     pid_t pid;
     int status;

     do { 
	  pid = waitpid (-1, &status, WNOHANG|WUNTRACED);
     } while(pid > 0);
     signal (SIGCHLD, childhandler);
     SIGRETURN(0);
}

/*
 * Find a free tunnel device and open it.
 * Return the interface name in `name, len'.
 */

int
tunnel_open (char *name, size_t len)
{
     int fd;
     int i;
     char devname[256];

     for (i = 0; i < 256; ++i) {
	  snprintf (devname, len, "%s%s%d", _PATH_DEV, TUNDEV, i);
	  fd = open (devname, O_RDWR, 0);
	  if (fd >= 0)
	       break;
	  if (errno == ENOENT || errno == ENODEV) {
	      warn("open %s", name);
	      return fd;
	  }
     }
     if (fd < 0)
	 warn("open %s" ,name);
     else
	 snprintf (name, len, "%s%d", TUNDEV, i);
     return fd;
}

/*
 * run the command `cmd' with (...).  return 0 if succesful or error
 * otherwise (and copy an error messages into `msg, len')
 */

int
kip_exec (const char *cmd, char *msg, size_t len, ...)
{
    pid_t pid;
    char **argv;
    va_list ap;

    va_start(ap, len);
    argv = vstrcollect(&ap);
    va_end(ap);

    pid = fork();
    switch (pid) {
    case -1:
	snprintf (msg, len, "fork: %s", strerror(errno));
	return errno;
    case 0: {
	int fd = open (_PATH_DEVNULL, O_RDWR, 0600);
	if (fd < 0) {
	    snprintf (msg, len, "open " _PATH_DEVNULL ": %s", strerror(errno));
	    return errno;
	}

	close (STDIN_FILENO);
	close (STDOUT_FILENO);
	close (STDERR_FILENO);
	
	dup2 (fd, STDIN_FILENO);
	dup2 (fd, STDOUT_FILENO);
	dup2 (fd, STDERR_FILENO);

	execvp (cmd, argv);
	snprintf (msg, len, "execvp %s: %s", cmd, strerror(errno));
	return errno;
    }
    default: {
	int status;

	while (waitpid(pid, &status, 0) < 0)
	    if (errno != EINTR) {
		snprintf (msg, len, "waitpid: %s", strerror(errno));
		return errno;
	    }

	if (WIFEXITED(status)) {
	    if (WEXITSTATUS(status) == 0) {
		return 0;
	    } else {
		snprintf (msg, len, "child returned with %d", 
			  WEXITSTATUS(status));
		return 1;
	    }
	} else if (WIFSIGNALED(status)) {
#ifndef WCOREDUMP
#define WCOREDUMP(X) 0
#endif
	    snprintf (msg, len, "terminated by signal num %d %s",
		      WTERMSIG(status), 
		      WCOREDUMP(status) ? " coredumped" : "");
	    return 1;
	} else if (WIFSTOPPED(status)) {
	    snprintf (msg, len, "process stoped by signal %d",
		      WSTOPSIG(status));
	    return 1;
	} else {
	    snprintf (msg, len, "child died in mysterious circumstances");
	    return 1;
	}
    }
    }
}

/*
 * fatal error `s' occured.
 */

void
fatal (int fd, const char *s, des_key_schedule schedule, des_cblock *iv)
{
     int16_t err = 0;
     int num = 0;

     if (fd != -1) {
	 des_cfb64_encrypt ((unsigned char*) &err, (unsigned char*) &err,
			    sizeof(err), schedule, iv, &num, DES_ENCRYPT);

	 write (fd, &err, sizeof(err));
	 write (fd, s, strlen(s)+1);
     }
     if (isserver)
	 syslog(LOG_ERR, "%s", s);
     else
	 warnx ("fatal error: %s", s);
}
