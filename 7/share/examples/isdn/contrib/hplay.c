/*---------------------------------------------------------------------------*
 *
 *	rsynth driver to output to 
 *		- an open isdn4bsd telephone connection		or
 *		- an output file				or
 *		- the /dev/audio device
 *      ----------------------------------------------------------------
 *
 *	tested with rsynth-2.0
 *
 *	written by Hellmuth Michaelis (hm@kts.org)
 *
 *	last edit-date: [Fri May 25 15:21:33 2001]
 *
 * $FreeBSD$
 *
 *---------------------------------------------------------------------------*/

#include <config.h>
#include <useconfig.h>
#include <stdio.h>
#include <math.h>
#include <errno.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/signal.h>
#include <sys/ioctl.h>

#include <i4b/i4b_tel_ioctl.h>

#include "proto.h"
#include "getargs.h"
#include "hplay.h"
#include "l2u.h"

#define SAMP_RATE 8000
long samp_rate = SAMP_RATE;

char *prog = "hplay";

static int use_audio = 1;
static int use_isdn = 0;
static int unit_no = 0;

static int audio_fd = -1;
static int isdn_fd = -1;
static int file_fd = -1;

char *audio_dev = "/dev/dsp";
char *isdn_dev = "/dev/i4btel";
static char *ulaw_file = NULL;

int
audio_init(int argc, char *argv[])
{
	char dev[64];
	int format = CVT_ALAW2ULAW;

	prog = argv[0];

	argc = getargs("FreeBSD audio/i4b/file output driver",argc, argv,
                "a", NULL, &use_audio, "use /dev/audio (default)",
                "i", NULL, &use_isdn,  "use /dev/i4btel",
		"u", "%d", &unit_no,   "/dev/i4btel unit number (def = 0)",
                "f", "",   &ulaw_file, "u-law output to file",
                NULL);

	if(help_only)
		return argc;

	if(ulaw_file)
	{
		if(strcmp(ulaw_file, "-") == 0)
		{
			file_fd = 1;                 /* stdout */
		}
		else
		{
			file_fd = open(ulaw_file, O_WRONLY | O_CREAT | O_TRUNC, 0666);
			if(file_fd < 0)
				fprintf(stderr, "ERROR: cannot open %s, error = %s\n", ulaw_file, strerror(errno));
		}
	}

	if(use_isdn)
	{
		sprintf(dev, "%s%d", isdn_dev, unit_no);
	
		if((isdn_fd = open(dev, O_WRONLY)) < 0)
		{
			fprintf(stderr, "ERROR: cannot open %s, error = %s\n", dev, strerror(errno));
		}
	
		if((ioctl(isdn_fd, I4B_TEL_SETAUDIOFMT, &format)) < 0)
	        {
			fprintf(stderr, "ioctl I4B_TEL_SETAUDIOFMT failed: %s", strerror(errno));
		}
	}

	if(use_audio)
	{
		audio_fd = open(audio_dev, O_WRONLY | O_NDELAY);
		if(audio_fd < 0)
  		{
			fprintf(stderr, "ERROR: cannot open %s, error = %s\n", audio_dev, strerror(errno));
		}
	}

	return argc;
}

void
audio_term()
{
	int format = CVT_NONE;

	if(isdn_fd >= 0)
	{
		if((ioctl(isdn_fd, I4B_TEL_SETAUDIOFMT, &format)) < 0)
        	{
			fprintf(stderr, "ioctl I4B_TEL_SETAUDIOFMT failed: %s", strerror(errno));
		}
		close(isdn_fd);
		isdn_fd = -1;
	}

	if(audio_fd >= 0)
	{
#if 0
		ioctl(audio_fd, SNDCTL_DSP_SYNC, &dummy);
#endif
		close(audio_fd);
		audio_fd = -1;
	}

	if(file_fd >= 0)
	{
		close(file_fd);
		file_fd = -1;
	}
}

void
audio_play(int n, short *data)
{
	int ret;
	unsigned char *p;

	if (n > 0)
	{
		unsigned char *converted = (unsigned char *) malloc(n);
		int i;

		if(converted == NULL)
		{
			fprintf(stderr, "Could not allocate memory for conversion\n");
			exit(3);
    		}

		for (i = 0; i < n; i++)
		{
			converted[i] = short2ulaw(data[i]);
		}

   		if (isdn_fd >= 0)
    		{
			p = converted;
			errno = 0;
    			
     			while((ret = write(isdn_fd, p, n)) != n)
     			{
     				if(!errno)
     				{
     					p += ret;
     					if(p > (converted + n))
     						break;
     				}
     				else
     				{
	     				fprintf(stderr, "write /dev/i4btel ERROR: ret (%d) != n (%d), error = %s\n", ret, n, strerror(errno));
	     				break;
	     			}
			}
    		}

		for (i = 0; i < n; i++)
			converted[i] = (data[i] - 32768) / 256;

   		if(audio_fd >= 0)
    		{
			p = converted;

			errno = 0;
    			
     			while((ret = write(audio_fd, p, n)) != n)
     			{
     				if(!errno)
     				{
     					p += ret;
     					if(p > (converted + n))
     						break;
     				}
     				else
     				{
	     				fprintf(stderr, "write /dev/dsp ERROR: ret (%d) != n (%d), error = %s\n", ret, n, strerror(errno));
	     				break;
	     			}
			}
    		}

   		if(file_fd >= 0)
    		{
    			int ret;
			p = converted;

			errno = 0;
    			
     			while((ret = write(file_fd, p, n)) != n)
     			{
     				if(!errno)
     				{
     					p += ret;
     					if(p > (converted + n))
     						break;
     				}
     				else
     				{
	     				fprintf(stderr, "write file ERROR: ret (%d) != n (%d), error = %s\n", ret, n, strerror(errno));
	     				break;
	     			}
			}
    		}

   		free(converted);
  	}
}

/* EOF */
