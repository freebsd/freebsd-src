/*
 * Copyright (c) 1995, 1996, 1997 Kungliga Tekniska Högskolan
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by the Kungliga Tekniska
 *      Högskolan and its contributors.
 * 
 * 4. Neither the name of the Institute nor the names of its contributors
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

RCSID("$Id: common.c,v 1.12 1997/05/02 14:28:06 assar Exp $");

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
     for (;;) {
	  fd_set fdset;
	  int ret, len;

	  FD_ZERO(&fdset);
	  FD_SET(tundev, &fdset);
	  FD_SET(netdev, &fdset);

	  ret = select (max(tundev, netdev)+1, &fdset, NULL, NULL, NULL);
	  if (ret < 0 && errno != EINTR) {
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
 */

int
tunnel_open (void)
{
     int fd;
     int i;
     char name[64];

     for (i = 0; i < 256; ++i) {
	  snprintf (name, sizeof(name), "%s%s%d", _PATH_DEV, TUNDEV, i);
	  fd = open (name, O_RDWR, 0);
	  if (fd >= 0)
	       break;
	  if (errno == ENOENT || errno == ENODEV) {
	      warn("open %s", name);
	      return fd;
	  }
     }
     if (fd < 0)
	 warn("open %s" ,name);
     return fd;
}
