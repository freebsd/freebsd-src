/* Non-X based CD player by Jean-Marc Zucconi */
/* Modifications by Andrew A. Chernov         */

#include <stdio.h>
#include <errno.h>
#include <sys/file.h>
#include <sys/cdio.h>
#include <sys/ioctl.h>

#define command(s) strncmp(cmd,s,strlen(s))==0

struct cd_toc_entry toc_buffer[100];

int cd_fd = -1;
int standalone;

char *cmd, *cdname;

int  pause (), resume (), stop (), eject (), setvol (int, int),
     read_toc_header (struct ioc_toc_header *), read_toc_entry (int),
     play_msf (int, int, int, int, int, int), play_track (int, int),
     get_vol (int *, int *), status (int *, int *, int *, int *);
void open_cd ();
int input ();

main (int argc, char **argv)
{
    int rc;

    if (argc != 2) {
	fprintf(stderr, "Usage: cdplay <cd>\n<cd> is device name such as cd0 or mcd0\n");
	exit(1);
    }
    cdname = argv[1];
    standalone = isatty (0);
    open_cd ();
    while (input ()) {
	rc = 0;
	open_cd ();
	if (command ("play")) {
	    int start, end;
	    sscanf (cmd+4, "%d%d", &start, &end);
	    rc = play_track (start, end);
	}
	else if (command ("reset"))
	    rc = reset ();
	else if (command ("pause"))
	    rc = cdpause ();
	else if (command ("resume"))
	    rc = resume ();
	else if (command ("stop"))
	    rc = stop ();
	else if (command ("setdebug"))
	    rc = setdebug ();
	else if (command ("clrdebug"))
	    rc = clrdebug ();
	else if (command ("eject")) {
	    rc = eject ();
	    close (cd_fd);
	    cd_fd = -1;
	} else if (command ("setvol")) {
	    int l, r;
	    sscanf (cmd+6, "%d %d", &l, &r);
	    rc = setvol (l, r);
	} else if (command ("getvol")) {
	    int r, l;
	    rc = getvol (&l, &r);
	    if (rc > -1)
		printf ("%d %d\n", l, r);
	} else if (command ("tochdr")) {
	    struct ioc_toc_header h;
	    rc = read_toc_header (&h);
	    if (rc > -1) {
		if (standalone)
			printf("start end length\n");
		printf ("%d %d %d\n", h.starting_track, h.ending_track, h.len);
	    }
	} else if (command ("msfplay")) {
	    int m1, m2, s1, s2, f1, f2;
	    sscanf(cmd+7, "%d%d%d%d%d%d", &m1, &s1, &f1, &m2, &s2, &f2);
	    rc = play_msf (m1, s1, f1, m2, s2, f2);
	} else if (command ("tocentry")) {
	    struct ioc_toc_header h;
	    int i, n;
	    rc = read_toc_header (&h);
	    if (rc > -1) {
		n =  h.ending_track - h.starting_track + 1;
		rc = read_toc_entrys ((n+1)*sizeof(struct cd_toc_entry));
		toc_buffer[n].track = 255;
		if (standalone)
		    printf("track minute second frame\n");
		for (i = 0; i <= n; i++)
		    printf ("%5d %6d %6d %5d\n", toc_buffer[i].track, toc_buffer[i].addr.msf.minute,
			    toc_buffer[i].addr.msf.second, toc_buffer[i].addr.msf.frame);
	     }
	} else if (command ("status")) {
	    int trk, m, s, f;
	    if (cd_fd < 0) 
		rc = -1; /* assume ejected */
	    else
		rc = status (&trk, &m, &s, &f);
	    if (standalone)
		printf("status track minute second frame\n");
	    printf ("%d %02d %d %d %d\n", rc, trk, m, s, f);
	} else if (command("quit"))
	    break;
	else if (command("help"))
	    printf(
"play <start_trk> <end_trk>, reset, pause, resume, stop, setdebug, clrdebug,\n\
eject, setvol <l> <r>, getvol, tochdr, msfplay <m1> <s1> <f1> <m2> <s2> <f2>,\n\
tocentry, status, quit, help\n");
	else
	    printf("No such command, enter 'help' for commands list\n");
	fflush (stdout);
	if (rc < 0 && standalone)
	    perror("cdplay");
    }
    exit (0);
}
int
play_track (int start, int end)
{
    struct ioc_play_track t;

    t.start_track = start;
    t.start_index = 1;
    t.end_track = end;
    t.end_index = 1;
    return ioctl (cd_fd, CDIOCPLAYTRACKS, &t);
}
int
reset ()
{
    return ioctl (cd_fd, CDIOCRESET);
}
int
cdpause ()
{
    return ioctl (cd_fd, CDIOCPAUSE);
}
int
setdebug ()
{
    return (ioctl (cd_fd, CDIOCSETDEBUG));
}
int
clrdebug ()
{
    return (ioctl (cd_fd, CDIOCCLRDEBUG));
}
int
resume ()
{
    return (ioctl (cd_fd, CDIOCRESUME));
}
int
stop ()
{
    return ioctl (cd_fd, CDIOCSTOP);
}
int
eject ()
{
    (void) ioctl (cd_fd, CDIOCALLOW);
    return ioctl (cd_fd, CDIOCEJECT);
}
int
setvol (int l, int r)
{
    struct ioc_vol v;
    
    v.vol[0] = l;
    v.vol[1] = r;
    v.vol[2] = 0;
    v.vol[3] = 0;
    return ioctl (cd_fd, CDIOCSETVOL, &v);
}
int
getvol (int  *l, int *r) 
{
    struct ioc_vol v;
    if (ioctl (cd_fd, CDIOCGETVOL, &v) < 0) 
	return -1;
    *l = v.vol[0];
    *r = v.vol[1];
    return 0;
}
int
read_toc_header (struct ioc_toc_header *h)
{
    return ioctl (cd_fd, CDIOREADTOCHEADER, (char *) h);
}
int
read_toc_entrys (int len)
{
    struct ioc_read_toc_entry t;

    t.address_format = CD_MSF_FORMAT;
    t.starting_track = 1;
    t.data_len = len;
    t.data = toc_buffer;
    return ioctl (cd_fd, CDIOREADTOCENTRYS, (char *) &t);
}
int
play_msf (int start_m, int start_s, int start_f, 
	  int end_m, int end_s, int end_f)
{
    struct ioc_play_msf a;

    a.start_m = start_m;
    a.start_s = start_s;
    a.start_f = start_f;
    a.end_m = end_m;
    a.end_s = end_s;
    a.end_f = end_f;
    return ioctl (cd_fd, CDIOCPLAYMSF, (char *) &a);
}
int
status (int *trk, int *min, int *sec, int *frame)
{
    struct ioc_read_subchannel s;
    struct cd_sub_channel_info data;
    bzero(&s, sizeof(s));
    s.data = &data;
    s.data_len = sizeof (data);
    s.address_format = CD_MSF_FORMAT;
    s.data_format = CD_CURRENT_POSITION;
    open_cd ();
    if (ioctl (cd_fd, CDIOCREADSUBCHANNEL, (char *) &s) < 0) 
	    return -1;
    *trk = s.data->what.position.track_number;
    *min = s.data->what.position.reladdr.msf.minute;
    *sec = s.data->what.position.reladdr.msf.second;
    *frame = s.data->what.position.reladdr.msf.frame;
    return s.data->header.audio_status;
}
    
int
input ()
{
    static char buf[80];
    int l;

    if (standalone)
	fprintf (stderr, "CD>");
    cmd = fgets (buf, sizeof(buf), stdin);
    if (cmd == NULL)
	return 0;
    l = strlen(cmd);
    if (l > 0 && cmd[l-1] == '\n')
	cmd[l-1] = '\0';
    return 1;
}
void
open_cd ()
{
    int trk, m, s, f;
    extern int errno;
    char devbuf[20];

    if (cd_fd > -1)
	return;
    sprintf(devbuf, "/dev/r%sc", cdname);
    cd_fd = open (devbuf, O_RDONLY);
    if (cd_fd < 0) {
	if (errno == ENXIO) {
	    /* open says 'Device not configured if there is no cd in */
	    fprintf(stderr, "open: No cd in\n");
	    return;
	}
	perror(devbuf);
	exit (1);
    }
    if (status (&trk, &m, &s, &f) < 0 ) {
	close (cd_fd);
	cd_fd = -1;
    }
}
