/*
 * Copyright (c) 2003, 2004, 2005
 *	John Wehle <john@feith.com>.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by John Wehle.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.	IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 *  $FreeBSD$
 */

/*
 * Set the channel of the tuner card.
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/ioctl.h>
#include <unistd.h>

#if __FreeBSD_version < 503001
#  include <machine/ioctl_meteor.h>
#  include <machine/ioctl_bt848.h>
#else
#  include <dev/bktr/ioctl_meteor.h>
#  include <dev/bktr/ioctl_bt848.h>
#endif


static void
usage()
{
  printf("Usage: setchannel [-a {on|off}] [-c | -r | -s | -t] [-g geom] [-m chnl_set] [chnl | freq]\n"
         "  -a    Enable / disable AFC.\n"
         "  -c    Select composite input.\n"
         "  -r    Select radio input.\n"
         "  -s    Select svideo input.\n"
         "  -t    Select tuner.\n"
         "  -g    Select geometry.\n"
         "          352x240 or 352x288 = VCD\n"
         "          480x480 or 480x576 = SVCD\n"
         "          352x480 or 352x576 = DVD (half D1)\n"
         "          720x480 or 720x576 = DVD (full D1)\n"
         "  -m    Select channel set / system.\n"
         "          0 = Tuner Default\n"
         "          %u = US Broadcast / NTSC\n"
         "          %u = US Cable / NTSC\n"
         "          %u = Western Europe / PAL\n"
         "          %u = Japan Broadcast / NTSC\n"
         "          %u = Japan Cable / NTSC\n"
         "          %u = Australia / PAL\n"
         "          %u = France / SECAM\n"
         "  chnl  Channel\n"
         "  freq  Frequency in MHz (must include decimal point).\n",
         CHNLSET_NABCST, CHNLSET_CABLEIRC,
         CHNLSET_WEUROPE, CHNLSET_JPNBCST, CHNLSET_JPNCABLE,
         CHNLSET_AUSTRALIA, CHNLSET_FRANCE);
}

int main( int argc, char *argv[] )
{
  char *ptr;
  char *endptr;
  int afc;
  int audio;
  int c;
  int channel_set;
  int i;
  int status;
  int tfd;
  unsigned int channel;
  unsigned int fraction;
  unsigned int freq;
  unsigned int x_size;
  unsigned int y_size;
  unsigned long device;
  struct bktr_capture_area cap;

  afc = -1;
  audio = -1;
  channel = 0;
  channel_set = -1;
  device = 0;
  freq = 0;
  status = 0;
  x_size = 0;
  y_size = 0;

  while ((c = getopt (argc, argv, "a:crstg:m:")) != -1)
    switch (c) {
      case 'a':
        if (strcasecmp (optarg, "on") == 0)
          afc = 1;
        else if (strcasecmp (optarg, "off") == 0)
          afc = 0;
        else {
          usage ();
          exit (1);
          }
        break;

      case 'c':
        device = METEOR_INPUT_DEV2;
        audio = -1;
        break;

      case 'r':
        device = 0;
        audio = AUDIO_INTERN;
        break;

      case 's':
        device = METEOR_INPUT_DEV_SVIDEO;
        audio = -1;
        break;

      case 't':
        device = METEOR_INPUT_DEV1;
        audio = -1;
        break;

      case 'g':
        if (sscanf (optarg, "%ux%u", &x_size, &y_size) != 2
            || x_size == 0 || y_size == 0) {
          usage ();
          exit (1);
          }
        break;

      case 'm':
        channel_set = atoi (optarg);
        if (channel_set < 0 || channel_set > CHNLSET_MAX) {
          usage ();
          exit (1);
          }
        break;

      default:
        usage ();
        exit (1);
      }

  if ( optind < argc) {

    /*
     * A number containing a decimal point is the frequency in MHz.
     */

    if ((ptr = strchr( argv[optind], '.' )) != NULL) {
      freq = strtol( argv[optind], &endptr, 10) * 1000;
      if (ptr != endptr) {
        usage ();
        exit (1);
        }

      ptr++;

      fraction = strtol( ptr, &endptr, 10);
      if (! isdigit( *ptr ) || *endptr != '\0') {
        usage ();
        exit (1);
        }

      for (i = endptr - ptr; i > 3; i--)
        fraction /= 10;
      for ( ; i < 3; i++)
        fraction *= 10;

      freq += fraction;
      }

    /*
     * An integer is the channel.
     */

    else
      channel = atoi( argv[optind] );
    }

  if (afc == -1 && audio == -1 && ! device && x_size == 0 && y_size == 0
      && channel_set == -1 && ! channel && ! freq) {
    usage ();
    exit (1);
    }

  tfd = open( "/dev/cxm0", O_RDONLY );
  if ( tfd < 0 ) {
    perror( "open() of /dev/cxm0 failed." );
    exit(1);
    }

  if (afc != -1)
    if ( ioctl( tfd, TVTUNER_SETAFC, &afc ) < 0 ) {
      perror( "ioctl( tfd, TVTUNER_SETAFC ) failed." );
      status = 1;
      }

  if (device)
    if ( ioctl( tfd, METEORSINPUT, &device ) < 0 ) {
      perror( "ioctl( tfd, METEORSINPUT ) failed." );
      status = 1;
      }

  if (audio != -1)
    if ( ioctl( tfd, BT848_SAUDIO, &audio ) < 0 ) {
      perror( "ioctl( tfd, BT848_SAUDIO ) failed." );
      status = 1;
      }

  if ( ioctl( tfd, BT848_GAUDIO, &audio ) < 0 ) {
    perror( "ioctl( tfd, BT848_GAUDIO ) failed." );
    status = 1;
    }

  if (x_size && y_size ) {
    memset (&cap, 0, sizeof (cap));
    cap.x_size = x_size;
    cap.y_size = y_size;
    if ( ioctl( tfd, BT848_SCAPAREA, &cap ) < 0 ) {
      perror( "ioctl( tfd, BT848_SCAPAREA ) failed." );
      status = 1;
      }
    }

  if (channel_set != -1)
    if ( ioctl( tfd, TVTUNER_SETTYPE, &channel_set ) < 0 ) {
      perror( "ioctl( tfd, TVTUNER_SETTYPE ) failed." );
      status = 1;
      }

  if (channel) {
    if ( ioctl( tfd, TVTUNER_SETCHNL, &channel ) < 0 ) {
      perror( "ioctl( tfd, TVTUNER_SETCHNL ) failed." );
      status = 1;
      }
    }
  else if (freq) {
    if (audio == AUDIO_INTERN) {
      /* Convert from kHz to MHz * 100 */
      freq = freq / 10;

      if ( ioctl( tfd, RADIO_SETFREQ, &freq ) < 0 ) {
        perror( "ioctl( tfd, RADIO_SETFREQ ) failed." );
        status = 1;
        }
      }
    else {
      /* Convert from kHz to MHz * 16 */
      freq = (freq * 16) / 1000;

      if ( ioctl( tfd, TVTUNER_SETFREQ, &freq ) < 0 ) {
        perror( "ioctl( tfd, TVTUNER_SETFREQ ) failed." );
        status = 1;
        }
      }
    }

  close ( tfd );
  exit ( status );
}
