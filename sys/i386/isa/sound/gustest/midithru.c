#include <stdio.h>
#include <sys/soundcard.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/errno.h>

SEQ_DEFINEBUF (2048);
SEQ_PM_DEFINES;

int seqfd, dev = 0;
unsigned char buf[100];
int bufp;

/* LRU list for free operators */

unsigned char free_list[256];
int fhead=0, ftail=0, flen=0;

/* LRU list for still playing notes */

unsigned char note_list[256];
int nhead=0, ntail=0, nlen=0;
unsigned char oper_note[32];

int pgm = 0;
int num_voices;
int bender = 0;	/* Initially off */

void
seqbuf_dump ()
{
  if (_seqbufptr)
    if (write (seqfd, _seqbuf, _seqbufptr) == -1)
      {
	perror ("write /dev/sequencer");
	exit (-1);
      }
  _seqbufptr = 0;
}

void
stop_note(int note, int velocity)
{
	int i, op;

	op=255;

	for (i=0;i<num_voices && op==255;i++)
	{
		if (oper_note[i]== note) op=i;
	}

	if (op==255) 
	 {
	   fprintf(stderr, "Note %d off, note not started\n", note);
	   fprintf(stderr, "%d, %d\n", flen, nlen);
	   return;	/* Has already been killed ??? */
	 }

	SEQ_STOP_NOTE(dev, op, note, velocity);
	SEQ_DUMPBUF();

	oper_note[op] = 255;

	free_list[ftail]=op;
	flen++;
	ftail = (ftail+1) % num_voices;

	for (i=0;i<16;i++)
	if (note_list[i] == op) note_list[i] = 255;

	while (nlen && note_list[nhead] == 255)
	{
		nlen--;
		/* printf("Remove from note queue %d, len %d\n", nhead, nlen);	*/
		nhead = (nhead+1) % 256;
	}
}

void
kill_one_note()
{
	int oldest;

	if (!nlen) {fprintf(stderr, "Free list empty but no notes playing\n");return;}	/* No notes playing */

	oldest = note_list[nhead];
	nlen--;
	nhead = (nhead+1) % 256;

        fprintf(stderr, "Killing oper %d, note %d\n", oldest, oper_note[oldest]);

	if (oldest== 255) return;	/* Was already stopped. Why? */

	stop_note(oper_note[oldest], 127);
}

void
start_note(int note, int velocity)
{
	int free;

	if (!flen) kill_one_note();

	if (!flen) {printf("** no free voices\n");return;}	/* Panic??? */

	free = free_list[fhead];
	flen--;
	fhead = (fhead+1) % num_voices;

	note_list[ntail] = free;

	if (nlen>255) 
	 {
#if 0
	   fprintf(stderr, "Note list overflow %d, %d, %d\n",
	                   nlen, nhead, ntail);	
#endif
	   nlen=0;	/* Overflow -> hard reset */
	 }
	nlen++;
	ntail = (ntail+1) % 256;

	oper_note[free] = note;

	SEQ_SET_PATCH(dev, free, pgm);
	SEQ_PITCHBEND(dev, free, bender);
	SEQ_START_NOTE(dev, free, note, velocity);
	SEQ_DUMPBUF();
}

void
channel_pressure(int ch, int pressure)
{
	int i;

	for (i=0;i<num_voices;i++)
	{
		if (oper_note[i] != 255)
		{
#if 1
			SEQ_CHN_PRESSURE(dev, i, pressure);
#else
			SEQ_EXPRESSION(dev, i, pressure);
#endif
			SEQ_DUMPBUF();
		}
	}
}

void
pitch_bender(int ch, int value)
{
	int i;

	value -= 8192;

	bender = value;

	for (i=0;i<num_voices;i++)
	{
		if (oper_note[i] != 255)
		{
			bender = value;
			SEQ_PITCHBEND(dev, i, value);
			SEQ_DUMPBUF();
		}
	}
}

void
do_buf()
{
	int ch = buf[0] & 0x0f;
	int value;

	switch (buf[0] & 0xf0)
	{
	case 0x90:	/* Note on */
		if (bufp < 3) break;
	/*	printf("Note on %d %d %d\n", ch, buf[1], buf[2]);	*/
		if (buf[2])
		   start_note(buf[1], buf[2]);
		else
		   stop_note(buf[1], buf[2]);
		bufp=1;
		break;

	case 0xb0:	/* Control change */
		if (bufp < 3) break;
	/*	printf("Control change %d %d %d\n", ch, buf[1], buf[2]);	*/
		bufp=1;
		break;

	case 0x80:	/* Note off */
		if (bufp < 3) break;
	/*	printf("Note off %d %d %d\n", ch, buf[1], buf[2]);	*/
		stop_note(buf[1], buf[2]);
		bufp=1;
		break;

	case 0xe0:	/* Pitch bender */
		if (bufp < 3) break;
		value = ((buf[2] & 0x7f) << 7) | (buf[1] & 0x7f);
	/*	printf("Pitch bender %d %d\n", ch, value >> 7);		*/
		pitch_bender(ch, value);
		bufp=1;
		break;

	case 0xc0:	/* Pgm change */
		if (bufp < 2) break;
	/*	printf("Pgm change %d %d\n", ch, buf[1]);	*/
		pgm = buf[1];
		if (PM_LOAD_PATCH(dev, 0, pgm) < 0)
		 if (errno != ESRCH)	/* No such process */
		     perror("PM_LOAD_PATCH");
		bufp=0;
		break;

	case 0xd0:	/* Channel pressure */
		if (bufp < 2) break;
	/*	printf("Channel pressure %d %d\n", ch, buf[1]);	*/
		channel_pressure(ch, buf[1]);
		bufp=1;
		break;

	default:
		bufp=0;
	}
}

int
main (int argc, char *argv[])
{
  int i, n, max_voice = 999;

  struct synth_info info;

  unsigned char ev[4], *p;

  if (argc >= 2) dev = atoi(argv[1]);

  for (i=0;i<16;i++) oper_note[i] = 255;

  if ((seqfd = open ("/dev/sequencer", O_RDWR, 0)) == -1)
    {
      perror ("open /dev/sequencer");
      exit (-1);
    }

  if (argc >= 3) 
  {
        int d = dev;
  	ioctl(seqfd, SNDCTL_FM_4OP_ENABLE, &d);
  }

  info.device = dev;
  
  if (ioctl(seqfd, SNDCTL_SYNTH_INFO, &info)==-1)
    {
      perror ("info /dev/sequencer");
      exit (-1);
    }

  num_voices = info.nr_voices;
  if (num_voices>max_voice)num_voices = max_voice;
  fprintf(stderr, "Output to synth device %d (%s)\n", dev, info.name);
  fprintf(stderr, "%d voices available\n", num_voices);

  for (i=0;i<num_voices;i++)
  {
  	flen++;
  	free_list[fhead] = i;
  	fhead = (fhead+1) % num_voices;
  }

  bufp = 0;
  if (PM_LOAD_PATCH(dev, 0, 0) < 0) /* Load the default instrument */
  if (errno != ESRCH)	/* No such process */
     perror("PM_LOAD_PATCH");

  while (1)
    {
      if ((n = read (seqfd, ev, sizeof (ev))) == -1)
	{
	  perror ("read /dev/sequencer");
	  exit (-1);
	}

      for (i = 0; i <= (n / 4); i++)
	{
	  p = &ev[i * 4];

	  if (p[0] == SEQ_MIDIPUTC && p[2] == 0 /* Midi if# == 0 */)
	    {
/*	      printf("%02x ", p[1]);fflush(stdout);		*/
	      if (p[1] & 0x80)	/* Status */
		{
		  if (bufp)
		    do_buf ();
		  buf[0] = p[1];
		  bufp = 1;
		}
	      else if (bufp)
		{
		  buf[bufp++] = p[1];
		  if ((buf[0] & 0xf0) == 0x90 || (buf[0] & 0xf0) == 0x80 || (buf[0] & 0xf0) == 0xb0 ||
		      (buf[0] & 0xf0) == 0xe0)
		    {
		      if (bufp == 3)
			do_buf ();
		    }
		  else
		    if ((buf[0] & 0xf0) == 0xc0 || (buf[0] & 0xf0) == 0xd0)
		    {
		      if (bufp == 2) do_buf();
		    }
		}
	    }
	}
    }

  exit (0);
}
