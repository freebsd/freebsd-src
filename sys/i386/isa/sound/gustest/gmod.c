/*
 *	gmod.c	- Module player for GUS and Linux.
 *		(C) Hannu Savolainen, 1993
 *
 *	NOTE!	This program doesn't try to be a complete module player.
 *		It's just a too I used while developing the driver. In
 *		addition it can be used as an example on programming
 *		the LInux Sound Driver with GUS.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <machine/ultrasound.h>
#include <fcntl.h>
#include <math.h>
#include <string.h>

#define CMD_ARPEG		0x00
#define CMD_SLIDEUP		0x01
#define CMD_SLIDEDOWN		0x02
#define CMD_SLIDETO		0x03
#define    SLIDE_SIZE		8
#define CMD_VOLSLIDE		0x0a
#define CMD_JUMP		0x0b
#define CMD_VOLUME		0x0c
#define CMD_BREAK		0x0d
#define CMD_SPEED		0x0f
#define CMD_NOP			0xfe
#define CMD_NONOTE		0xff

#define MIN(a, b)		((a) < (b) ? (a) : (b))

#define MAX_TRACK	8
#define MAX_PATTERN	128
#define MAX_POSITION	128

struct note_info
  {
    unsigned char   note;
    unsigned char   vol;
    unsigned char   sample;
    unsigned char   command;
    short           parm1, parm2;
  };

struct voice_info
  {
    int             sample;
    int             note;
    int             volume;
    int             pitchbender;

    /* Pitch sliding */

    int             slide_pitch;
    int             slide_goal;
    int             slide_rate;

    int             volslide;
  };

typedef struct note_info pattern[MAX_TRACK][64];
int             pattern_len[MAX_POSITION];
int             pattern_tempo[MAX_POSITION];
pattern        *pattern_table[MAX_PATTERN];

struct voice_info voices[MAX_TRACK];

int             nr_channels, nr_patterns, songlength;
int             tune[MAX_POSITION];
double          tick_duration;

int             period_table[] =
{
  856, 808, 762, 720, 678, 640, 604, 570, 538, 508, 480, 453,
  428, 404, 381, 360, 339, 320, 302, 285, 269, 254, 240, 226,
  214, 202, 190, 180, 170, 160, 151, 143, 135, 127, 120, 113
};

SEQ_DEFINEBUF (2048);

int             seqfd;
int             sample_ok[128], sample_vol[128];
int             tmp, gus_dev;
double          this_time, next_time;
int             ticks_per_division;
double          clock_rate;	/* HZ */

/*
 * The function seqbuf_dump() must always be provided
 */

void            play_module (char *name);
int             load_module (char *name);
int             play_note (int channel, struct note_info *pat);
void            lets_play_voice (int channel, struct voice_info *v);

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
init_voices ()
{
  int             i;

  for (i = 0; i < MAX_TRACK; i++)
    {
      voices[i].sample = 0;
      voices[i].note = 0;
      voices[i].volume = 64;

      voices[i].slide_pitch = 0;
      voices[i].slide_goal = 0;
      voices[i].slide_rate = 0;
      voices[i].pitchbender = 0;

      voices[i].volslide = 0;
    }
}

int
main (int argc, char *argv[])
{
  int             i, n, j;
  struct synth_info info;

  if ((seqfd = open ("/dev/sequencer", O_WRONLY, 0)) == -1)
    {
      perror ("/dev/sequencer");
      exit (-1);
    }

  if (ioctl (seqfd, SNDCTL_SEQ_NRSYNTHS, &n) == -1)
    {
      perror ("/dev/sequencer");
      exit (-1);
    }

  for (i = 0; i < n; i++)
    {
      info.device = i;

      if (ioctl (seqfd, SNDCTL_SYNTH_INFO, &info) == -1)
	{
	  perror ("/dev/sequencer");
	  exit (-1);
	}

      if (info.synth_type == SYNTH_TYPE_SAMPLE
	  && info.synth_subtype == SAMPLE_TYPE_GUS)
	gus_dev = i;
    }

  if (gus_dev == -1)
    {
      fprintf (stderr, "Gravis Ultrasound not detected\n");
      exit (-1);
    }

  GUS_NUMVOICES (gus_dev, 14);

  for (i = 1; i < argc; i++)
    {
      for (j = 0; j < MAX_PATTERN; j++)
	pattern_table[j] = NULL;

      if (load_module (argv[i]))
	{
	  tick_duration = 100.0 / clock_rate;
	  play_module (argv[i]);
	}

    }

  SEQ_DUMPBUF ();
  close (seqfd);

  exit (0);
}

unsigned short
intelize (unsigned short v)
{
  return ((v & 0xff) << 8) | ((v >> 8) & 0xff);
}

unsigned long
intelize4 (unsigned long v)
{
  return
  (((v >> 16) & 0xff) << 8) | (((v >> 16) >> 8) & 0xff) |
  (((v & 0xff) << 8) | ((v >> 8) & 0xff) << 16);
}

int
load_stm_module (int mod_fd, char *name)
{

  struct sample_header
    {
      char            name[12];
      unsigned char   instr_disk;
      unsigned short  reserved1;
      unsigned short  length;	/* In bytes */
      unsigned short  loop_start;
      unsigned short  loop_end;
      unsigned char   volume;
      unsigned char   reserved2;
      unsigned short  C2_speed;
      unsigned short  reserved3;

    };

  int             i, total_mem;
  int             sample_ptr;

  int             position;

  unsigned char  *tune_ptr;	/* array 0-127 */

  char            header[1105], sname[21];

  int             nr_samples;	/* 16 or 32 samples (or 64 or ???) */
  int             slen, npat;

  fprintf (stderr, "Loading .STM module: %s\n", name);

  if (read (mod_fd, header, sizeof (header)) != sizeof (header))
    {
      fprintf (stderr, "%s: Short file (header)\n", name);
      close (mod_fd);
      return 0;
    }

  strncpy (sname, header, 20);

  fprintf (stderr, "\nModule: %s - ", sname);

  if (header[28] != 0x1a)
    {
      fprintf (stderr, "Not a STM module\n");
      close (mod_fd);
      return 0;
    }

  npat = header[33];
  slen = 0;
  tune_ptr = &header[48 + (31 * 32)];

  for (i = 0; i < 64; i++)
    {
      tune[i] = tune_ptr[i];
      if (tune[i] < npat)
	slen = i;
    }

  fprintf (stderr, "Song lenght %d, %d patterns.\n", slen, npat);

  nr_samples = 31;

  sample_ptr = 48 + (31 * 32) + 64 + (npat * 1024);	/* Location where the
							 * first sample is
							 * stored */
  total_mem = 0;

  for (i = 0; i < 32; i++)
    sample_ok[i] = 0;

  for (i = 0; i < nr_samples; i++)
    {
      int             len, loop_start, loop_end, base_freq;
      unsigned short  loop_flags = 0;

      struct sample_header *sample;

      struct patch_info *patch;

      sample = (struct sample_header *) &header[48 + (i * 32)];

      len = sample->length;
      loop_start = sample->loop_start;
      loop_end = sample->loop_end;
      base_freq = sample->C2_speed;

      if (strlen (sample->name) > 21)
	{
	  fprintf (stderr, "\nInvalid name for sample #%d\n", i);
	  close (mod_fd);
	  return 0;
	}

      if (len > 0)
	{
	  int             x;

	  if (loop_end > len)
	    loop_end = 1;
	  else if (loop_end < loop_start)
	    {
	      loop_start = 0;
	      loop_end = 0;
	    }
	  else
	    loop_flags = WAVE_LOOPING;

	  total_mem += len;
	  patch = (struct patch_info *) malloc (sizeof (*patch) + len);

	  patch->key = GUS_PATCH;
	  patch->device_no = gus_dev;
	  patch->instr_no = i;
	  patch->mode = loop_flags;
	  patch->len = len;
	  patch->loop_start = loop_start;
	  patch->loop_end = loop_end;
	  patch->base_freq = base_freq;
	  patch->base_note = 261630;	/* Mid C */
	  patch->low_note = 0;
	  patch->high_note = 0x7fffffff;
	  patch->volume = 120;

	  if (lseek (mod_fd, sample_ptr, 0) == -1)
	    {
	      perror (name);
	      close (mod_fd);
	      free (patch);
	      return 0;
	    }

	  sample_ptr += len;

	  if ((x = read (mod_fd, patch->data, len)) != len)
	    {
	      fprintf (stderr, "Short file (sample at %d (%d!=%d)\n", sample_ptr, x, len);
	      close (mod_fd);
	      free (patch);
	      return 0;
	    }

	  fprintf (stderr, "Sample %02d: %05d, %05d, %05d, %07d %s\n",
		   i,
		   len,
		   loop_start,
		   loop_end,
		   base_freq,
		   sample->name);

	  if (write (seqfd, patch, sizeof (*patch) + len) == -1)
	    {
	      perror ("ioctl /dev/sequencer");
	      exit (-1);
	    }
	  else
	    sample_ok[i] = 1;

	  free (patch);
	}
    }

  nr_patterns = slen;
  songlength = slen;
  nr_channels = 4;

  for (position = 0; position < npat; position++)
    {
      unsigned char   patterns[64][4][4];
      int             pat, channel, x;

      int             pp = 1104 + (position * 1024);

      if ((pattern_table[position] = (pattern *) malloc (sizeof (struct note_info) * 64 * nr_channels)) == NULL)
	{
	  fprintf (stderr, "Can't allocate memory for a pattern\n");
	  return 0;
	}

      if (lseek (mod_fd, pp, 0) == -1)
	{
	  perror (name);
	  close (mod_fd);
	  return 0;
	}

      if ((x = read (mod_fd, patterns, 1024)) != 1024)
	{
	  fprintf (stderr, "Short file (pattern at %d), %d!=%d\n", pp, x, 1024);
	  close (mod_fd);
	  return 0;
	}

      for (pat = 0; pat < 64; pat++)
	{

	  for (channel = 0; channel < 4; channel++)
	    {
	      unsigned char  *p;

	      unsigned        vol, note, octave, sample, effect, params;

	      p = &patterns[pat][channel][0];

	      if (p[0] < 251)
		{
		  note = p[0] & 15;
		  octave = p[0] / 16;

		  note = 48 + octave * 12 + note;

		  sample = p[1] / 8;
		  vol = (p[1] & 7) + (p[2] / 2);
		  effect = p[2] & 0xF;
		  params = p[3];
		}
	      else
		{
		  note = 0;
		  octave = 0;

		  sample = 0;
		  vol = 0;
		  effect = CMD_NONOTE;
		  params = 0;
		}

	      (*pattern_table[position])[channel][pat].note = note;
	      (*pattern_table[position])[channel][pat].sample = sample;
	      (*pattern_table[position])[channel][pat].command = effect;
	      (*pattern_table[position])[channel][pat].parm1 = params;
	      (*pattern_table[position])[channel][pat].parm2 = 0;
	      (*pattern_table[position])[channel][pat].vol = vol;
	    }

	}

    }

  close (mod_fd);
  return 1;
}

int
load_669_module (int mod_fd, char *name)
{
  struct sample_header
    {
      char            name[13];
      unsigned long   length;	/* In bytes */
      unsigned long   loop_start;
      unsigned long   loop_end;
    };

  int             i, total_mem;
  int             sample_ptr;

  int             position;

  unsigned char  *tune_ptr, *len_ptr, *tempo_ptr;	/* array 0-127 */

  char            header[1084];
  char            msg[110];

  int             nr_samples;	/* 16 or 32 samples */
  int             slen, npat;

  clock_rate = 25.0;

  fprintf (stderr, "Loading .669 module: %s\n", name);

  if (read (mod_fd, header, sizeof (header)) != sizeof (header))
    {
      fprintf (stderr, "%s: Short file (header)\n", name);
      close (mod_fd);
      return 0;
    }

  if (*(unsigned short *) &header[0] != 0x6669)
    {
      fprintf (stderr, "Not a 669 file\n");
      close (mod_fd);
      return 0;
    }

  strncpy (msg, &header[2], 108);

  for (i = 0; i < strlen (msg); i++)
    if ((msg[i] >= ' ' && msg[i] <= 'z') || msg[i] == '\n')
      printf ("%c", msg[i]);
  printf ("\n");

  npat = header[0x6f];

  tune_ptr = &header[0x71];

  for (slen = 0; slen < 128 && tune_ptr[slen] != 0xff; slen++);
  slen--;

  for (i = 0; i < slen; i++)
    tune[i] = tune_ptr[i];

  len_ptr = &header[0x171];
  for (i = 0; i < slen; i++)
    pattern_len[i] = len_ptr[i] - 1;

  tempo_ptr = &header[0xf1];
  for (i = 0; i < slen; i++)
    pattern_tempo[i] = tempo_ptr[i];

  nr_samples = header[0x6e];

  fprintf (stderr, "Song lenght %d, %d patterns, %d samples.\n", slen, npat, nr_samples);

  sample_ptr = 0x1f1 + (nr_samples * 0x19) + (npat * 0x600);	/* Location where the
								 * first sample is
								 * stored */
  total_mem = 0;

  for (i = 0; i < 64; i++)
    sample_ok[i] = 0;

  for (i = 0; i < nr_samples; i++)
    {
      int             len, loop_start, loop_end;
      unsigned short  loop_flags = 0;

      struct sample_header *sample;
      char            sname[14];

      struct patch_info *patch;

      sample = (struct sample_header *) &header[0x1f1 + (i * 0x19)];

      len = *(unsigned long *) &sample->name[13];
      loop_start = *(unsigned long *) &sample->name[17];
      loop_end = *(unsigned long *) &sample->name[21];
      if (loop_end > len)
	loop_end = 1;
      else if (loop_end == len)
	loop_end--;

      if (loop_end < loop_start)
	{
	  loop_start = 0;
	  loop_end = 0;
	}

      strncpy (sname, sample->name, 13);

      if (len > 0 && len < 200000)
	{
	  total_mem += len;

	  fprintf (stderr, "Sample %02d: %05d, %05d, %05d   %s\n",
		   i,
		   len,
		   loop_start,
		   loop_end,
		   sname);

	  patch = (struct patch_info *) malloc (sizeof (*patch) + len);

	  if (loop_end == 0)
	    loop_end = 1;
	  if (loop_end >= len)
	    loop_end = 1;

	  if (loop_end > 1) loop_flags = WAVE_LOOPING;

	  patch->key = GUS_PATCH;
	  patch->device_no = gus_dev;
	  patch->instr_no = i;
	  patch->mode = WAVE_UNSIGNED | loop_flags;
	  patch->len = len;
	  patch->loop_start = loop_start;
	  patch->loop_end = loop_end;
	  patch->base_freq = 8448;
	  patch->base_note = 261630;
	  patch->low_note = 1000;
	  patch->high_note = 0x7fffffff;
	  patch->volume = 120;

	  if (lseek (mod_fd, sample_ptr, 0) == -1)
	    {
	      fprintf (stderr, "Seek failed\n");
	      perror (name);
	      close (mod_fd);
	      free (patch);
	      return 0;
	    }

	  sample_ptr += len;

	  if (read (mod_fd, patch->data, len) != len)
	    {
	      fprintf (stderr, "Short file (sample at %d)\n", sample_ptr);
	      close (mod_fd);
	      free (patch);
	      return 0;
	    }

	  if (write (seqfd, patch, sizeof (*patch) + len) == -1)
	    {
	      perror ("ioctl /dev/sequencer");
	      /* exit (-1);	 */
	    }
	  else
	    sample_ok[i] = 1;

	  free (patch);
	}
    }

  nr_patterns = slen;
  songlength = slen;
  nr_channels = 8;

  for (position = 0; position < npat; position++)
    {
      unsigned char   patterns[0x600];
      int             pat, channel, x;

      int             pp = 0x1f1 + (nr_samples * 0x19) + (position * 0x600);

      if ((pattern_table[position] = (pattern *) malloc (sizeof (struct note_info) * 64 * nr_channels)) == NULL)
	{
	  fprintf (stderr, "Can't allocate memory for a pattern\n");
	  return 0;
	}


      if (lseek (mod_fd, pp, 0) == -1)
	{
	  perror (name);
	  close (mod_fd);
	  return 0;
	}

      if ((x = read (mod_fd, patterns, 1024)) != 1024)
	{
	  fprintf (stderr, "Short file (pattern at %d) %d!=1024\n", pp, x);
	  close (mod_fd);
	  return 0;
	}

      for (pat = 0; pat < 64; pat++)
	{

	  for (channel = 0; channel < 8; channel++)
	    {
	      unsigned char  *p;

	      unsigned        vol, period, sample, effect, params;

	      p = &patterns[pat * 24 + channel * 3];

	      if (p[0] >= 0xfe ||
		  (p[0] == 0xff && p[1] == 0xff && p[2] == 0xff) ||
		  (p[0] == 0 && p[1] == 0 && p[2] == 0) ||
		  *(int *) p == -1)
		{
		  period = 0;
		  effect = CMD_NONOTE;
		  sample = 0;
		  vol = 0;
		  params = 0;

		  if (p[0] == 0)
		    {
		      effect = CMD_BREAK;
		      params = -2;
		    }
		}
	      else
		{
		  period = (p[0] >> 2) + 48;
		  effect = (p[2] >> 4);
		  params = p[2] & 0x0f;
		  vol = p[1] & 0x0f;

		  if (p[2] == 0xfe)
		    {
		      effect = CMD_VOLUME;
		      params = vol;
		    }
		  else if (p[2] == 0xff)
		    {
		      effect = CMD_NOP;
		    }
		  else
		    switch (effect)
		      {
		      case 0:	/* a - Portamento up */
			effect = CMD_SLIDEUP;
			break;

		      case 1:	/* b - Portamento down */
			effect = CMD_SLIDEDOWN;
			break;

		      case 2:	/* c - Port to note */
			effect = CMD_SLIDETO;
			break;

		      case 3:	/* d - Frequency adjust */
			effect = CMD_NOP;	/* To be implemented */
			break;

		      case 4:	/* e - Frequency vibrato */
			effect = CMD_NOP;	/* To be implemented */
			break;

		      case 5:	/* f - Set tempo */
			effect = CMD_SPEED;
			break;

		      default:
			effect = CMD_NOP;
		      }

		  sample = (((p[0] << 4) & 0x30) | ((p[1] >> 4) & 0x0f)) + 1;
		}

	      (*pattern_table[position])[channel][pat].note = period;
	      (*pattern_table[position])[channel][pat].sample = sample;
	      (*pattern_table[position])[channel][pat].command = effect;
	      (*pattern_table[position])[channel][pat].parm1 = params;
	      (*pattern_table[position])[channel][pat].parm2 = 0;
	      (*pattern_table[position])[channel][pat].vol = vol;
	    }

	}

    }

  close (mod_fd);
  return 1;
}

int
load_mmd0_module (int mod_fd, char *name)
{

  struct sample_header
    {
      unsigned short  loop_start;
      unsigned short  loop_end;
      unsigned char   midich;
      unsigned char   midipreset;
      unsigned char   volume;
      unsigned char   strans;
    };

  int             i, total_mem;
  int             sample_ptr;

  int             position;

  unsigned char  *tune_ptr;	/* array 0-127 */

  char            header[1105];

  int             nr_samples;	/* 16 or 32 samples (or 64 or ???) */
  int             slen, npat;

  fprintf (stderr, "Loading .MED module: %s\n", name);

  if (read (mod_fd, header, sizeof (header)) != sizeof (header))
    {
      fprintf (stderr, "%s: Short file (header)\n", name);
      close (mod_fd);
      return 0;
    }

  if (strncmp (header, "MMD0", 4))
    {
      fprintf (stderr, "Not a MED module\n");
      close (mod_fd);
      return 0;
    }

  printf ("Module len %d\n", intelize4 (*(long *) &header[4]));
  printf ("Song info %d\n", intelize4 (*(long *) &header[8]));
  printf ("Song len %d\n", intelize4 (*(long *) &header[12]));
  printf ("Blockarr %x\n", intelize4 (*(long *) &header[16]));
  printf ("Blockarr len %d\n", intelize4 (*(long *) &header[20]));
  printf ("Sample array %x\n", intelize4 (*(long *) &header[24]));
  printf ("Sample array len %d\n", intelize4 (*(long *) &header[28]));
  printf ("Exp data %x\n", intelize4 (*(long *) &header[32]));
  printf ("Exp size %d\n", intelize4 (*(long *) &header[36]));
  printf ("Pstate %d\n", intelize (*(long *) &header[40]));
  printf ("Pblock %d\n", intelize (*(long *) &header[42]));

  return 0;

  npat = header[33];
  slen = 0;
  tune_ptr = &header[48 + (31 * 32)];

  for (i = 0; i < 64; i++)
    {
      tune[i] = tune_ptr[i];
      if (tune[i] < npat)
	slen = i;
    }

  fprintf (stderr, "Song lenght %d, %d patterns.\n", slen, npat);

  nr_samples = 31;

  sample_ptr = 48 + (31 * 32) + 64 + (npat * 1024);	/* Location where the
							 * first sample is
							 * stored */
  total_mem = 0;

  for (i = 0; i < 32; i++)
    sample_ok[i] = 0;

  for (i = 0; i < nr_samples; i++)
    {
      int             len, loop_start, loop_end, base_freq;
      unsigned short  loop_flags = 0;

      struct sample_header *sample;

      struct patch_info *patch;

      sample = (struct sample_header *) &header[48 + (i * 32)];

      /*
       * len = sample->length; loop_start = sample->loop_start; loop_end =
       * sample->loop_end; base_freq = sample->C2_speed;
       * 
       * if (strlen (sample->name) > 21) { fprintf (stderr, "\nInvalid name for
       * sample #%d\n", i); close (mod_fd); return 0; }
       */
      if (len > 0)
	{
	  int             x;

	  if (loop_end > len)
	    loop_end = 1;

	  if (loop_end < loop_start)
	    {
	      loop_start = 0;
	      loop_end = 0;
	    }

	  if (loop_end > 2) loop_flags = WAVE_LOOPING;

	  total_mem += len;
	  patch = (struct patch_info *) malloc (sizeof (*patch) + len);

	  patch->key = GUS_PATCH;
	  patch->device_no = gus_dev;
	  patch->instr_no = i;
	  patch->mode = loop_flags;
	  patch->len = len;
	  patch->loop_start = loop_start;
	  patch->loop_end = loop_end;
	  patch->base_freq = base_freq;
	  patch->base_note = 261630;	/* Mid C */
	  patch->low_note = 0;
	  patch->high_note = 0x7fffffff;
	  patch->volume = 120;

	  if (lseek (mod_fd, sample_ptr, 0) == -1)
	    {
	      perror (name);
	      close (mod_fd);
	      free (patch);
	      return 0;
	    }

	  sample_ptr += len;

	  if ((x = read (mod_fd, patch->data, len)) != len)
	    {
	      fprintf (stderr, "Short file (sample at %d (%d!=%d)\n", sample_ptr, x, len);
	      close (mod_fd);
	      free (patch);
	      return 0;
	    }
	  /*
	   * fprintf (stderr, "Sample %02d: %05d, %05d, %05d, %07d %s\n", i,
	   * len, loop_start, loop_end, base_freq, sample->name);
	   */
	  if (write (seqfd, patch, sizeof (*patch) + len) == -1)
	    {
	      perror ("ioctl /dev/sequencer");
	      exit (-1);
	    }
	  else
	    sample_ok[i] = 1;

	  free (patch);
	}
    }

  nr_patterns = slen;
  songlength = slen;
  nr_channels = 4;

  for (position = 0; position < npat; position++)
    {
      unsigned char   patterns[64][4][4];
      int             pat, channel, x;

      int             pp = 1104 + (position * 1024);

      if ((pattern_table[position] = (pattern *) malloc (sizeof (struct note_info) * 64 * nr_channels)) == NULL)
	{
	  fprintf (stderr, "Can't allocate memory for a pattern\n");
	  return 0;
	}

      if (lseek (mod_fd, pp, 0) == -1)
	{
	  perror (name);
	  close (mod_fd);
	  return 0;
	}

      if ((x = read (mod_fd, patterns, 1024)) != 1024)
	{
	  fprintf (stderr, "Short file (pattern at %d), %d!=%d\n", pp, x, 1024);
	  close (mod_fd);
	  return 0;
	}

      for (pat = 0; pat < 64; pat++)
	{

	  for (channel = 0; channel < 4; channel++)
	    {
	      unsigned char  *p;

	      unsigned        vol, note, octave, sample, effect, params;

	      p = &patterns[pat][channel][0];

	      if (p[0] < 251)
		{
		  note = p[0] & 15;
		  octave = p[0] / 16;

		  note = 48 + octave * 12 + note;

		  sample = p[1] / 8;
		  vol = (p[1] & 7) + (p[2] / 2);
		  effect = p[2] & 0xF;
		  params = p[3];
		}
	      else
		{
		  note = 0;
		  octave = 0;

		  sample = 0;
		  vol = 0;
		  effect = CMD_NONOTE;
		  params = 0;
		}

	      (*pattern_table[position])[channel][pat].note = note;
	      (*pattern_table[position])[channel][pat].sample = sample;
	      (*pattern_table[position])[channel][pat].command = effect;
	      (*pattern_table[position])[channel][pat].parm1 = params;
	      (*pattern_table[position])[channel][pat].parm2 = 0;
	      (*pattern_table[position])[channel][pat].vol = vol;
	    }

	}

    }

  close (mod_fd);
  return 1;
}

int
load_module (char *name)
{

  struct sample_header
    {
      char            name[22];
      unsigned short  length;	/* In words */

      unsigned char   finetune;
      unsigned char   volume;

      unsigned short  repeat_point;	/* In words */
      unsigned short  repeat_length;	/* In words */
    };

  int             i, mod_fd, total_mem;
  int             sample_ptr, pattern_loc;

  int             position;

  unsigned char  *tune_ptr;	/* array 0-127 */

  char            header[1084];

  int             nr_samples;	/* 16 or 32 samples */
  int             slen, npat;
  char            mname[23];

  ioctl (seqfd, SNDCTL_SEQ_SYNC, 0);
  ioctl (seqfd, SNDCTL_SEQ_RESETSAMPLES, &gus_dev);

  clock_rate = 50.0;

  for (i = 0; i < MAX_POSITION; i++)
    pattern_len[i] = 64;

  for (i = 0; i < MAX_POSITION; i++)
    pattern_tempo[i] = 0;

  if ((mod_fd = open (name, O_RDONLY, 0)) == -1)
    {
      perror (name);
      return 0;
    }

  if (read (mod_fd, header, sizeof (header)) != sizeof (header))
    {
      fprintf (stderr, "%s: Short file (header)\n", name);
      close (mod_fd);
      return 0;
    }

  if (lseek (mod_fd, 0, 0) == -1)
    {
      perror (name);
      close (mod_fd);
      return 0;
    }

  if (header[28] == 0x1a)
    return load_stm_module (mod_fd, name);

  if (*(unsigned short *) &header[0] == 0x6669)
    return load_669_module (mod_fd, name);

  if (!strncmp (header, "MMD0", 4))
    return load_mmd0_module (mod_fd, name);

  fprintf (stderr, "Loading .MOD module: %s\n", name);

  strncpy (mname, header, 22);
  fprintf (stderr, "\nModule: %s - ", mname);

  if (!strncmp (&header[1080], "M.K.", 4) || !strncmp (&header[1080], "FLT8", 4))
    {
      fprintf (stderr, "31 samples\n");
      nr_samples = 31;
    }
  else
    {
      fprintf (stderr, "15 samples\n");
      nr_samples = 15;
    }

  if (nr_samples == 31)
    {
      sample_ptr = pattern_loc = 1084;
      slen = header[950];
      tune_ptr = (unsigned char *) &header[952];
    }
  else
    {
      sample_ptr = pattern_loc = 600;
      slen = header[470];
      tune_ptr = (unsigned char *) &header[472];
    }

  npat = 0;
  for (i = 0; i < 128; i++)
    {
      tune[i] = tune_ptr[i];

      if (tune_ptr[i] > npat)
	npat = tune_ptr[i];
    }
  npat++;

  fprintf (stderr, "Song lenght %d, %d patterns.\n", slen, npat);

  sample_ptr += (npat * 1024);	/* Location where the first sample is stored */
  total_mem = 0;

  for (i = 0; i < 32; i++)
    sample_ok[i] = 0;

  for (i = 0; i < nr_samples; i++)
    {
      int             len, loop_start, loop_end;
      unsigned short  loop_flags = 0;
      char            pname[22];

      struct sample_header *sample;

      struct patch_info *patch;

      sample = (struct sample_header *) &header[20 + (i * 30)];

      len = intelize (sample->length) * 2;
      loop_start = intelize (sample->repeat_point) * 2;
      loop_end = loop_start + (intelize (sample->repeat_length) * 2);

      if (loop_start > len)
	loop_start = 0;
      if (loop_end > len)
	loop_end = len;

      if (loop_end <= loop_start)
	loop_end = loop_start + 1;

      if (loop_end > 2 && loop_end > loop_start)
         loop_flags = WAVE_LOOPING;

      strncpy (pname, sample->name, 20);

      if (len > 0)
	{
	  fprintf (stderr, "Sample %02d: L%05d, S%05d, E%05d V%02d %s\n",
		   i,
		   len,
		   loop_start,
		   loop_end,
		   sample->volume,
		   pname);

	  total_mem += len;

	  patch = (struct patch_info *) malloc (sizeof (*patch) + len);

	  patch->key = GUS_PATCH;
	  patch->device_no = gus_dev;
	  patch->instr_no = i;
	  patch->mode = loop_flags;
	  patch->len = len;
	  patch->loop_start = loop_start;
	  patch->loop_end = loop_end;
	  patch->base_note = 261630;	/* Middle C */
	  patch->base_freq = 8448;
	  patch->low_note = 0;
	  patch->high_note = 20000000;
	  patch->volume = 120;
	  patch->panning = 0;

	  if (lseek (mod_fd, sample_ptr, 0) == -1)
	    {
	      perror (name);
	      close (mod_fd);
	      free (patch);
	      return 0;
	    }

	  sample_ptr += len;

	  if (read (mod_fd, patch->data, len) != len)
	    {
	      fprintf (stderr, "Short file (sample) %d\n", sample_ptr);
	      close (mod_fd);
	      free (patch);
	      return 0;
	    }

	  SEQ_WRPATCH (patch, sizeof (*patch) + len);

	  sample_ok[i] = 1;
	  if (sample->volume == 0) sample->volume = 64;
	  sample_vol[i] = sample->volume;

	  free (patch);
	}
    }

  nr_patterns = npat;
  songlength = slen;
  nr_channels = 4;

  for (position = 0; position < npat; position++)
    {
      unsigned char   patterns[64][4][4];
      int             pat, channel;

      int             pp = pattern_loc + (position * 1024);

      if (lseek (mod_fd, pp, 0) == -1)
	{
	  perror (name);
	  close (mod_fd);
	  return 0;
	}

      if (read (mod_fd, patterns, 1024) != 1024)
	{
	  fprintf (stderr, "Short file (pattern %d) %d\n", tune[position], pp);
	  close (mod_fd);
	  return 0;
	}

      if ((pattern_table[position] = (pattern *) malloc (sizeof (struct note_info) * 64 * nr_channels)) == NULL)
	{
	  fprintf (stderr, "Can't allocate memory for a pattern\n");
	  return 0;
	}

      for (pat = 0; pat < 64; pat++)
	{
	  for (channel = 0; channel < 4; channel++)
	    {
	      unsigned short  tmp;
	      unsigned char  *p;

	      unsigned        period, sample, effect, params, note, vol;

	      p = &patterns[pat][channel][0];

	      tmp = (p[0] << 8) | p[1];
	      sample = (tmp >> 8) & 0x10;
	      period =
		MIN (tmp & 0xFFF, 1023);
	      tmp = (p[2] << 8) | p[3];
	      sample |= tmp >> 12;
	      effect = (tmp >> 8) & 0xF;
	      params = tmp & 0xFF;

	      note = 0;

	      if (period)
		{
		  /*
		   * Convert period to a Midi note number
		   */

		  for (note = 0; note < 37 && period != period_table[note]; note++);
		  if (note >= 37)
		    note = 0;

		  note += 48;
		}

	      vol = 64;

	      if (sample)
		if (effect == 0xc)
		  {
		    vol = params;
		  }
		else
		  vol = sample_vol[sample - 1];

	      vol *= 2;
	      if (vol>64)vol--;

	      (*pattern_table[position])[channel][pat].note = note;
	      (*pattern_table[position])[channel][pat].sample = sample;
	      (*pattern_table[position])[channel][pat].command = effect;
	      (*pattern_table[position])[channel][pat].parm1 = params;
	      (*pattern_table[position])[channel][pat].parm2 = 0;
	      (*pattern_table[position])[channel][pat].vol = vol;
	    }
	}
    }

  close (mod_fd);
  return 1;
}

int
panning (int ch)
{
  static int      panning_tab[] =
  {-110, 110, 110, -110};

  return panning_tab[ch % 4];
}

void
set_speed (int parm)
{
  if (!parm)
    parm = 1;

  if (parm < 32)
    {
      ticks_per_division = parm;
    }
  else
    {
      tick_duration = (60.0 / parm) * 10.0;
    }

}

void
play_module (char *name)
{
  int             i, position, jump_to_pos;

  init_voices ();

  SEQ_START_TIMER ();
#if 1
  for (i=0;i<32;i++)
  {
  	SEQ_EXPRESSION(gus_dev, i, 127);
  	SEQ_MAIN_VOLUME(gus_dev, i, 100);
  }
#endif
  next_time = 0.0;

  set_speed (6);

  for (position = 0; position < songlength; position++)
    {
      int             tick, pattern, channel, pos, go_to;

      pos = tune[position];
      if (pattern_tempo[position])
	set_speed (pattern_tempo[position]);

      jump_to_pos = -1;
      for (pattern = 0; pattern < pattern_len[position] && jump_to_pos == -1; pattern++)
	{
	  this_time = 0.0;

	  for (channel = 0; channel < nr_channels; channel++)
	    {
	      if ((go_to = play_note (channel, &(*pattern_table[pos])[channel][pattern])) != -1)
		jump_to_pos = go_to;

	    }

	  next_time += tick_duration;

	  for (tick = 1; tick < ticks_per_division; tick++)
	    {
	      for (channel = 0; channel < nr_channels; channel++)
		lets_play_voice (channel, &voices[channel]);
	      next_time += tick_duration;
	    }

	}

      if (jump_to_pos >= 0)
	position = jump_to_pos;
    }

  SEQ_WAIT_TIME ((int) next_time + 200);	/* Wait extra 2 secs */

  for (i = 0; i < nr_channels; i++)
    SEQ_STOP_NOTE (gus_dev, i, 0, 127);
  SEQ_DUMPBUF ();

  for (i = 0; i < nr_patterns; i++)
    free (pattern_table[i]);
}

void
sync_time ()
{
  if (next_time > this_time)
    {
      SEQ_WAIT_TIME ((long) next_time);
      this_time = next_time;
    }
}

void
set_volslide (int channel, struct note_info *pat)
{
  int             n;

  voices[channel].volslide = 0;

  if ((n = (pat->parm1 & 0xf0) >> 4))
    voices[channel].volslide = n;
  else
    voices[channel].volslide = pat->parm1 & 0xf;
}

void
set_slideto (int channel, struct note_info *pat)
{
  int             size, rate, dir, range = 200;

  rate = pat->parm1;
  size = voices[channel].note - pat->note;
  if (!size)
    return;

  if (size < 0)
    {
      size *= -1;
      dir = -1;
    }
  else
    dir = 1;

  if (size > 2)
    {
      range = size * 100;
      rate = rate * size / 200;
    }

  rate = pat->parm1 * dir / 30;
  if (!rate)
    rate = 1;

  voices[channel].slide_pitch = 1;
  voices[channel].slide_goal = (dir * 8192 * 200 * 2 / size) / range;
  voices[channel].pitchbender = 0;
  voices[channel].slide_rate = rate;
  SEQ_BENDER_RANGE (gus_dev, channel, range);
}

int
play_note (int channel, struct note_info *pat)
{
  int             jump = -1;
  int             sample;

  if (pat->sample == 0x3f)
    pat->sample = 0;

  if (pat->command == CMD_NONOTE)
    return -1;			/* Undefined */

  sample = pat->sample;

  if (sample && !pat->note)
    {
      pat->note = voices[channel].note;
    }

  if (sample)
    voices[channel].sample = sample;
  else
    sample = voices[channel].sample;

  sample--;

  if (pat->note && pat->command != 3)	/* Have a note -> play */
    {
      if (sample < 0)
	sample = voices[channel].sample - 1;

      if (!sample_ok[sample])
	sample = voices[channel].sample - 1;

      if (sample < 0)
	sample = 0;

      if (sample_ok[sample])
	{
	  sync_time ();

          if (pat->vol > 127) pat->vol=127;
	  SEQ_SET_PATCH (gus_dev, channel, sample);
	  SEQ_PANNING (gus_dev, channel, panning (channel));
	  SEQ_PITCHBEND (gus_dev, channel, 0);
	  SEQ_START_NOTE (gus_dev, channel, pat->note, pat->vol);

	  voices[channel].volume = pat->vol;
	  voices[channel].note = pat->note;
	  voices[channel].slide_pitch = 0;
	}
      else
	SEQ_STOP_NOTE (gus_dev, channel, pat->note, pat->vol);
    }

  switch (pat->command)
    {

    case CMD_NOP:;
      break;

    case CMD_JUMP:
      jump = pat->parm1;
      break;

    case CMD_BREAK:
      jump = -2;
      break;

    case CMD_SPEED:
      set_speed (pat->parm1);
      break;

    case CMD_SLIDEUP:
      voices[channel].slide_pitch = 1;
      voices[channel].slide_goal = 8191;
      voices[channel].pitchbender = 0;
      voices[channel].slide_rate = pat->parm1 * SLIDE_SIZE;
      SEQ_BENDER_RANGE (gus_dev, channel, 200);
      break;

    case CMD_SLIDEDOWN:
      voices[channel].slide_pitch = 1;
      voices[channel].slide_goal = -8192;
      voices[channel].pitchbender = 0;
      voices[channel].slide_rate = -pat->parm1 * SLIDE_SIZE;
      SEQ_BENDER_RANGE (gus_dev, channel, 200);
      break;

    case CMD_SLIDETO:
      set_slideto (channel, pat);
      break;

    case CMD_VOLUME:
      {
        int vol = pat->parm1*2;
        if (vol>127) vol=127;
      if (pat->note && pat->command != 3)
	break;
      SEQ_START_NOTE (gus_dev, channel, 255, vol);
      }
      break;

    case CMD_ARPEG:
      break;

    case 0x0e:
      /* printf ("Cmd 0xE%02x\n", pat->parm1);	*/
      break;

    case CMD_VOLSLIDE:
      set_slideto (channel, pat);
      break;

    default:
      /* printf ("Command %x %02x\n", pat->command, pat->parm1);	*/
    }

  return jump;
}

void
lets_play_voice (int channel, struct voice_info *v)
{
  if (v->slide_pitch)
    {
      v->pitchbender += v->slide_rate;
      if (v->slide_goal < 0)
	{
	  if (v->pitchbender <= v->slide_goal)
	    {
	      v->pitchbender = v->slide_goal;
	      v->slide_pitch = 0;	/* Stop */
	    }
	}
      else
	{
	  if (v->pitchbender >= v->slide_goal)
	    {
	      v->pitchbender = v->slide_goal;
	      v->slide_pitch = 0;	/* Stop */
	    }
	}

      sync_time ();
      SEQ_PITCHBEND (gus_dev, channel, v->pitchbender);
    }

  if (v->volslide)
    {
      v->volume += v->volslide;
      sync_time ();

      if (v->volume > 127) v->volume = 127;
      SEQ_START_NOTE (gus_dev, channel, 255, v->volume);
    }
}
