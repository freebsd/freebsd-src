/*
 * pcmio.c -- a simple utility for controlling audio I/O
 *     (rate, channels, resolution...)
 *
 * (C) Luigi Rizzo 1998
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <machine/soundcard.h>

char * usage_string =
"Usage: %s [-f device] [+parameters] [len:skip] file { file }\n"
"where device is the device to be used (default /dev/audio)\n"
"[len:skip] is the subsection of the file to be played\n"
"with a '-' indicating the default\n"
"and parameters is a comma-separated list containing one or more of\n"
"  N             the sampling speed\n"
"  stereo|mono   \n"
"  cd|u8|alaw|ulaw|s16   data format\n"
"  loop            loop (play cyclically)\n"
;

#define BLKSZ	32768
int format=AFMT_MU_LAW;
int rate=8000 ;
int stereo = 0 ;
char dev[128];
int audiodev;
int play = 1 ; /* default */
int loop = 0 ;
int skip = 0;
int size = -1 ;

extern char *optarg;
extern int optind, optopt, opterr, optreset;

int
usage(char *s)
{
    printf(usage_string, s);
    exit(0);
}

int
parse_len(char *s)
{
    int par = -1;
    int mul = 1;
    int p = strlen(s);

    if (p == 0)
	return -1 ;

    if (*s != '-')
	par = atoi(s);
    else
	return -1 ;
    switch(s[p-1]) {
    case 'k':
    case 'K':
	mul = 1024;
	break;
    case 'm':
    case 'M':
	mul = 1024*1024;
	break;
    case 's':
	mul = rate * (stereo+1);
	if (format == AFMT_S16_LE)
	    mul += mul;
	break;
    }
    return par*mul;
}

void
parse_fmt(char *fmt)
{
    char *s;
    int v, last = 0 ;

again:
    while (*fmt && (*fmt == ' ' || *fmt == '\t')) fmt++;
    s = fmt;
    while (*s && ! (*s == ',' || *s == ':' || *s == ';') ) s++;
    if (*s)
	*s='\0';
    else
	last = 1 ;
    v = atoi(fmt) ;
    if (v > 0 && v < 1000000)
	rate = v ;
    else {
	if (!strcmp(fmt, "ulaw")) format = AFMT_MU_LAW;
	else if (!strcmp(fmt, "alaw")) format = AFMT_A_LAW;
	else if (!strcmp(fmt, "u8")) format = AFMT_U8 ;
	else if (!strcmp(fmt, "s16")) format = AFMT_S16_LE ;
	else if (!strcmp(fmt, "mono")) stereo = 0;
	else if (!strcmp(fmt, "stereo")) stereo = 1;
	else if (!strcmp(fmt, "loop")) loop = 1;
	else if (!strcmp(fmt, "rec")) play = 0;
	else if (!strcmp(fmt, "play")) play = 1;
	else if (!strcmp(fmt, "cd")) {
		stereo = 1 ;
		format = AFMT_S16_LE ;
		rate = 44100;
	}
    }
    if (last == 0) {
	fmt = s+1;
	goto again;
    }
}

char buf[BLKSZ];

int
main(int argc, char *argv[])
{
    int i,c;
    int ac = argc;
    char *p;

    strcpy(dev, "/dev/audio");

    while ( (c= getopt(argc, argv, "f:") ) != EOF ) {
	switch (c) {
	case 'f' :
	    if (optarg[0] >='0' && optarg[0] <='9')
		sprintf(dev, "/dev/audio%s", optarg);
	    else
		strcpy(dev, optarg);
	    break;
	}
    }
    if (*argv[optind] == '+') {
	parse_fmt(argv[optind]+1);
	optind++;
    }
    /*
     * assume a string with a "," and no "/" as a command
     */
    if (strstr(argv[optind],",") && !strstr(argv[optind],"/") ) { 
	parse_fmt(argv[optind]);
	optind++;
    }
    /*
     * assume a string with a ":" and no "/" as a time limit
     */
    if ( (p = strstr(argv[optind] , ":")) && !strstr(argv[optind],"/") ) {
	*p = '\0';
	size = parse_len(argv[optind]);
	skip = parse_len(p+1);
	optind++;
    }
    printf("Using device %s, speed %d, mode 0x%08x, %s\n",
	dev, rate, format, stereo ? "stereo":"mono");
    printf("using files: ");
    for (i=optind; i< argc ; i++)
	printf("[%d] %s, ",i, argv[i]);
    printf("\n");

    audiodev = open(dev, play ? 1 : 0);
    if (audiodev < 0) {
	printf("failed to open %d\n", dev);
	exit(2);
    }
    ioctl(audiodev, SNDCTL_DSP_SETFMT, &format);
    ioctl(audiodev, SNDCTL_DSP_STEREO, &stereo);
    ioctl(audiodev, SNDCTL_DSP_SPEED, &rate);
    printf("-- format %d,%s,0x%08x, len %d skip %d\n",
	rate, stereo? "stereo":"mono",format, size, skip);
    if (play) {
	off_t ofs;
	int limit;
again:
	for (i=optind; i< argc ; i++) {
	    int l = -2;
	    int f = open(argv[i], O_RDONLY);
	    int sz ;

	    printf("opened %s returns %d\n", argv[i], f);
	    if (f < 0)
		continue;
	    limit = size;
	    if (skip > 0) {
		ofs = skip;
		lseek(f, ofs, 0 /* begin */ );
	    }
	    sz = BLKSZ;
	    if (limit > 0 && limit < sz)
		sz = limit ;
	    while ( (l = read(f, buf, sz) ) > 0 ) {
		write(audiodev, buf, l);
		if (limit > 0) {
		    limit -= l ;
		    if (limit > 0 && limit < sz)
			sz = limit ;
		    if (limit <= 0 )
			break;
		    if (limit < sz)
			sz = limit ;
		}
	    }
	    close(f);
	}
	if (loop)
	    goto again;
    } else { /* record */
	int l = -2;
	int f ;
	if (!strcmp(argv[optind], "-") )
	    f = 1;
	else
	    f = open(argv[optind], O_WRONLY | O_CREAT | O_TRUNC, 0664);
	fprintf(stderr,"open %s returns %d\n", argv[optind], f);

	while ( size > 0 && (l = read(audiodev, buf, BLKSZ) ) > 0 ) {
	    if (l <= skip) {
		skip -= l ; /* at most skip = 0 */
		continue;
	    } else { /* l > skip */
		l -= skip ;
		if (l > size)
		    l = size ;
		write(f, buf+skip, l);
		skip = 0 ;
		size -= l ;
	    }
	}
	close(f);
    }
}
