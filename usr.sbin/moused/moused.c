/**
 ** Copyright (c) 1995 Michael Smith, All rights reserved.
 **
 ** Redistribution and use in source and binary forms, with or without
 ** modification, are permitted provided that the following conditions
 ** are met:
 ** 1. Redistributions of source code must retain the above copyright
 **    notice, this list of conditions and the following disclaimer as
 **    the first lines of this file unmodified.
 ** 2. Redistributions in binary form must reproduce the above copyright
 **    notice, this list of conditions and the following disclaimer in the
 **    documentation and/or other materials provided with the distribution.
 ** 3. All advertising materials mentioning features or use of this software
 **    must display the following acknowledgment:
 **      This product includes software developed by Michael Smith.
 ** 4. The name of the author may not be used to endorse or promote products
 **    derived from this software without specific prior written permission.
 **
 **
 ** THIS SOFTWARE IS PROVIDED BY Michael Smith ``AS IS'' AND ANY
 ** EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 ** IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 ** PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL Michael Smith BE LIABLE FOR
 ** ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 ** CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 ** SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 ** BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 ** WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 ** OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 ** EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 **
 **/

/**
 ** MOUSED.C
 **
 ** Mouse daemon : listens to a serial port, the bus mouse interface, or
 ** the PS/2 mouse port for mouse data stream, interprets data and passes 
 ** ioctls off to the console driver.
 **
 ** The mouse interface functions are derived closely from the mouse
 ** handler in the XFree86 X server.  Many thanks to the XFree86 people
 ** for their great work!
 ** 
 **/

#ifndef lint
static const char rcsid[] =
	"$Id: moused.c,v 1.15 1998/02/04 06:46:33 ache Exp $";
#endif /* not lint */

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <setjmp.h>
#include <termios.h>
#include <syslog.h>

#include <machine/console.h>
#include <machine/mouse.h>

#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>

#define MAX_CLICKTHRESHOLD	2000	/* 2 seconds */

#define TRUE		1
#define FALSE		0

#define MOUSE_XAXIS	(-1)
#define MOUSE_YAXIS	(-2)

#define	ChordMiddle	0x0001
#define Emulate3Button	0x0002
#define ClearDTR	0x0004
#define ClearRTS	0x0008
#define NoPnP		0x0010
	
#define ID_NONE		0
#define ID_PORT		1
#define ID_IF		2
#define ID_TYPE 	4
#define ID_MODEL	8
#define ID_ALL		(ID_PORT | ID_IF | ID_TYPE | ID_MODEL)

#define debug(fmt,args...) \
	if (debug&&nodaemon) warnx(fmt, ##args)

#define logerr(e, fmt, args...) {				\
	if (background) {					\
	    syslog(LOG_DAEMON | LOG_ERR, fmt ": %m", ##args);	\
	    exit(e);						\
	} else							\
	    err(e, fmt, ##args);				\
}

#define logerrx(e, fmt, args...) {				\
	if (background) {					\
	    syslog(LOG_DAEMON | LOG_ERR, fmt, ##args);		\
	    exit(e);						\
	} else							\
	    errx(e, fmt, ##args);				\
}

#define logwarn(fmt, args...) {					\
	if (background)						\
	    syslog(LOG_DAEMON | LOG_WARNING, fmt ": %m", ##args); \
	else							\
	    warn(fmt, ##args);					\
}

#define logwarnx(fmt, args...) {				\
	if (background)						\
	    syslog(LOG_DAEMON | LOG_WARNING, fmt, ##args);	\
	else							\
	    warnx(fmt, ##args);					\
}

/* structures */

/* symbol table entry */
typedef struct {
    char *name;
    int val;
    int val2;
} symtab_t;

/* serial PnP ID string */
typedef struct {
    int revision;	/* PnP revision, 100 for 1.00 */
    char *eisaid;	/* EISA ID including mfr ID and product ID */
    char *serial;	/* serial No, optional */
    char *class;	/* device class, optional */
    char *compat;	/* list of compatible drivers, optional */
    char *description;	/* product description, optional */
    int neisaid;	/* length of the above fields... */
    int nserial;
    int nclass;
    int ncompat;
    int ndescription;
} pnpid_t;

/* global variables */

int	debug = 0;
int	nodaemon = FALSE;
int	background = FALSE;
int	identify = ID_NONE;
int	extioctl = FALSE;

/* local variables */

/* interface (the table must be ordered by MOUSE_IF_XXX in mouse.h) */
static symtab_t rifs[] = {
    { "serial",		MOUSE_IF_SERIAL },
    { "bus",		MOUSE_IF_BUS },
    { "inport",		MOUSE_IF_INPORT },
    { "ps/2",		MOUSE_IF_PS2 },
    { "sysmouse",	MOUSE_IF_SYSMOUSE },
    { NULL,		MOUSE_IF_UNKNOWN },
};

/* types (the table must be ordered by MOUSE_PROTO_XXX in mouse.h) */
static char *rnames[] = {
    "microsoft",
    "mousesystems",
    "logitech",
    "mmseries",
    "mouseman",
    "busmouse",
    "inportmouse",
    "ps/2",
    "mmhitab",
    "glidepoint",
    "intellimouse",
    "thinkingmouse",
    "sysmouse",
#if notyet
    "mariqua",
#endif
    NULL
};

/* models */
static symtab_t	rmodels[] = {
    { "NetScroll",	MOUSE_MODEL_NETSCROLL },
    { "NetMouse",	MOUSE_MODEL_NET },
    { "GlidePoint",	MOUSE_MODEL_GLIDEPOINT },
    { "ThinkingMouse",	MOUSE_MODEL_THINK },
    { "IntelliMouse",	MOUSE_MODEL_INTELLI },
    { "EasyScroll",	MOUSE_MODEL_EASYSCROLL },
    { "MouseMan+",	MOUSE_MODEL_MOUSEMANPLUS },
    { "generic",	MOUSE_MODEL_GENERIC },
    { NULL, 		MOUSE_MODEL_UNKNOWN },
};

/* PnP EISA/product IDs */
static symtab_t pnpprod[] = {
    /* Kensignton ThinkingMouse */
    { "KML0001",	MOUSE_PROTO_THINK,	MOUSE_MODEL_THINK },
    /* MS IntelliMouse */
    { "MSH0001",	MOUSE_PROTO_INTELLI,	MOUSE_MODEL_INTELLI },
    /* MS IntelliMouse TrackBall */
    { "MSH0004",	MOUSE_PROTO_INTELLI,	MOUSE_MODEL_INTELLI },
    /* Genius PnP Mouse */
    { "KYE0001",	MOUSE_PROTO_MS,		MOUSE_MODEL_GENERIC },
    /* Genius NetMouse */
    { "KYE0003",	MOUSE_PROTO_INTELLI,	MOUSE_MODEL_NET },
    /* Genius EZScroll */
    { "KYEEZ00",	MOUSE_PROTO_MS,		MOUSE_MODEL_EASYSCROLL },  
    /* Logitech MouseMan (new 4 button model) */
    { "LGI800C",	MOUSE_PROTO_INTELLI,	MOUSE_MODEL_MOUSEMANPLUS },
    /* Logitech MouseMan+ */
    { "LGI8050",	MOUSE_PROTO_INTELLI,	MOUSE_MODEL_MOUSEMANPLUS },
    /* Logitech FirstMouse+ */
    { "LGI8051",	MOUSE_PROTO_INTELLI,	MOUSE_MODEL_MOUSEMANPLUS },
    /* Logitech serial */
    { "LGI8001",	MOUSE_PROTO_LOGIMOUSEMAN, MOUSE_MODEL_GENERIC },

    /* MS bus */
    { "PNP0F00",	MOUSE_PROTO_BUS,	MOUSE_MODEL_GENERIC },
    /* MS serial */
    { "PNP0F01",	MOUSE_PROTO_MS,		MOUSE_MODEL_GENERIC },     
    /* MS InPort */
    { "PNP0F02",	MOUSE_PROTO_INPORT,	MOUSE_MODEL_GENERIC }, 
    /* MS PS/2 */
    { "PNP0F03",	MOUSE_PROTO_PS2,	MOUSE_MODEL_GENERIC },    
    /*
     * EzScroll returns PNP0F04 in the compatible device field; but it
     * doesn't look compatible... XXX
     */
    /* MouseSystems */ 
    { "PNP0F04",	MOUSE_PROTO_MSC,	MOUSE_MODEL_GENERIC },    
    /* MouseSystems */ 
    { "PNP0F05",	MOUSE_PROTO_MSC,	MOUSE_MODEL_GENERIC },    
#if notyet
    /* Genius Mouse */ 
    { "PNP0F06",	MOUSE_PROTO_???,	MOUSE_MODEL_GENERIC },    
    /* Genius Mouse */ 
    { "PNP0F07",	MOUSE_PROTO_???,	MOUSE_MODEL_GENERIC },    
#endif
    /* Logitech serial */
    { "PNP0F08",	MOUSE_PROTO_LOGIMOUSEMAN, MOUSE_MODEL_GENERIC },
    /* MS BallPoint serial */
    { "PNP0F09",	MOUSE_PROTO_MS,		MOUSE_MODEL_GENERIC },     
    /* MS PnP serial */
    { "PNP0F0A",	MOUSE_PROTO_MS,		MOUSE_MODEL_GENERIC },     
    /* MS PnP BallPoint serial */
    { "PNP0F0B",	MOUSE_PROTO_MS,		MOUSE_MODEL_GENERIC },     
    /* MS serial comatible */
    { "PNP0F0C",	MOUSE_PROTO_MS,		MOUSE_MODEL_GENERIC },     
    /* MS InPort comatible */
    { "PNP0F0D",	MOUSE_PROTO_INPORT,	MOUSE_MODEL_GENERIC }, 
    /* MS PS/2 comatible */
    { "PNP0F0E",	MOUSE_PROTO_PS2,	MOUSE_MODEL_GENERIC },    
    /* MS BallPoint comatible */
    { "PNP0F0F",	MOUSE_PROTO_MS,		MOUSE_MODEL_GENERIC },     
#if notyet
    /* TI QuickPort */
    { "PNP0F10",	MOUSE_PROTO_???,	MOUSE_MODEL_GENERIC },     
#endif
    /* MS bus comatible */
    { "PNP0F11",	MOUSE_PROTO_BUS,	MOUSE_MODEL_GENERIC },    
    /* Logitech PS/2 */
    { "PNP0F12",	MOUSE_PROTO_PS2,	MOUSE_MODEL_GENERIC },
    /* PS/2 */
    { "PNP0F13",	MOUSE_PROTO_PS2,	MOUSE_MODEL_GENERIC },
#if notyet
    /* MS Kids Mouse */
    { "PNP0F14",	MOUSE_PROTO_???,	MOUSE_MODEL_GENERIC },
#endif
    /* Logitech bus */ 
    { "PNP0F15",	MOUSE_PROTO_BUS,	MOUSE_MODEL_GENERIC },
#if notyet
    /* Logitech SWIFT */
    { "PNP0F16",	MOUSE_PROTO_???,	MOUSE_MODEL_GENERIC },
#endif
    /* Logitech serial compat */
    { "PNP0F17",	MOUSE_PROTO_LOGIMOUSEMAN, MOUSE_MODEL_GENERIC },
    /* Logitech bus compatible */
    { "PNP0F18",	MOUSE_PROTO_BUS,	MOUSE_MODEL_GENERIC },
    /* Logitech PS/2 compatible */
    { "PNP0F19",	MOUSE_PROTO_PS2,	MOUSE_MODEL_GENERIC },
#if notyet
    /* Logitech SWIFT compatible */
    { "PNP0F1A",	MOUSE_PROTO_???,	MOUSE_MODEL_GENERIC },
    /* HP Omnibook */
    { "PNP0F1B",	MOUSE_PROTO_???,	MOUSE_MODEL_GENERIC },
    /* Compaq LTE TrackBall PS/2 */
    { "PNP0F1C",	MOUSE_PROTO_???,	MOUSE_MODEL_GENERIC },
    /* Compaq LTE TrackBall serial */
    { "PNP0F1D",	MOUSE_PROTO_???,	MOUSE_MODEL_GENERIC },
    /* MS Kidts Trackball */
    { "PNP0F1E",	MOUSE_PROTO_???,	MOUSE_MODEL_GENERIC },
#endif

    { NULL,		MOUSE_PROTO_UNKNOWN,	MOUSE_MODEL_GENERIC },
};

/* the table must be ordered by MOUSE_PROTO_XXX in mouse.h */
static unsigned short rodentcflags[] =
{
    (CS7	           | CREAD | CLOCAL | HUPCL ),	/* MicroSoft */
    (CS8 | CSTOPB	   | CREAD | CLOCAL | HUPCL ),	/* MouseSystems */
    (CS8 | CSTOPB	   | CREAD | CLOCAL | HUPCL ),	/* Logitech */
    (CS8 | PARENB | PARODD | CREAD | CLOCAL | HUPCL ),	/* MMSeries */
    (CS7		   | CREAD | CLOCAL | HUPCL ),	/* MouseMan */
    0,							/* Bus */
    0,							/* InPort */
    0,							/* PS/2 */
    (CS8		   | CREAD | CLOCAL | HUPCL ),	/* MM HitTablet */
    (CS7	           | CREAD | CLOCAL | HUPCL ),	/* GlidePoint */
    (CS7                   | CREAD | CLOCAL | HUPCL ),	/* IntelliMouse */
    (CS7                   | CREAD | CLOCAL | HUPCL ),	/* Thinking Mouse */
    (CS8 | CSTOPB	   | CREAD | CLOCAL | HUPCL ),	/* sysmouse */
#if notyet
    (CS8 | CSTOPB	   | CREAD | CLOCAL | HUPCL ),	/* Mariqua */
#endif
};

static struct rodentparam {
    int flags;
    char *portname;		/* /dev/XXX */
    int rtype;			/* MOUSE_PROTO_XXX */
    int level;			/* operation level: 0 or greater */
    int baudrate;
    int rate;			/* report rate */
    int resolution;		/* MOUSE_RES_XXX or a positive number */
    int zmap;			/* MOUSE_{X|Y}AXIS or a button number */
    int mfd;			/* mouse file descriptor */
    int cfd;			/* /dev/consolectl file descriptor */
    long clickthreshold;	/* double click speed in msec */
    mousehw_t hw;		/* mouse device hardware information */
    mousemode_t mode;		/* protocol information */
} rodent = { 
    flags : 0, 
    portname : NULL,
    rtype : MOUSE_PROTO_UNKNOWN,
    level : -1,
    baudrate : 1200, 
    rate : 0,
    resolution : MOUSE_RES_UNKNOWN, 
    zmap: 0,
    mfd : -1,
    cfd : -1,
    clickthreshold : 500,	/* 0.5 sec */
};

/* button status */
static struct {
    int count;		/* 0: up, 1: single click, 2: double click,... */
    struct timeval tv;	/* timestamp on the last `up' event */
} buttonstate[MOUSE_MAXBUTTON];

static jmp_buf env;

/* function prototypes */

static void	moused(void);
static void	hup(int sig);
static void	usage(void);

static int	r_identify(void);
static char	*r_if(int type);
static char	*r_name(int type);
static char	*r_model(int model);
static void	r_init(void);
static int	r_protocol(u_char b, mousestatus_t *act);
static int	r_installmap(char *arg);
static void	r_map(mousestatus_t *act1, mousestatus_t *act2);
static void	r_click(mousestatus_t *act);
static void	setmousespeed(int old, int new, unsigned cflag);

static int	pnpgets(char *buf);
static int	pnpparse(pnpid_t *id, char *buf, int len);
static symtab_t	*pnpproto(pnpid_t *id);

static symtab_t	*gettoken(symtab_t *tab, char *s, int len);
static char	*gettokenname(symtab_t *tab, int val);

void
main(int argc, char *argv[])
{
    int c;
    int	i;

    while((c = getopt(argc,argv,"3C:DF:PRS:cdfhi:l:m:p:r:st:z:")) != -1)
	switch(c) {

	case '3':
	    rodent.flags |= Emulate3Button;
	    break;

	case 'c':
	    rodent.flags |= ChordMiddle;
	    break;

	case 'd':
	    ++debug;
	    break;

	case 'f':
	    nodaemon = TRUE;
	    break;

	case 'i':
	    if (strcmp(optarg, "all") == 0)
	        identify = ID_ALL;
	    else if (strcmp(optarg, "port") == 0)
	        identify = ID_PORT;
	    else if (strcmp(optarg, "if") == 0)
	        identify = ID_IF;
	    else if (strcmp(optarg, "type") == 0)
	        identify = ID_TYPE;
	    else if (strcmp(optarg, "model") == 0)
	        identify = ID_MODEL;
	    else {
	        warnx("invalid argument `%s'", optarg);
	        usage();
	    }
	    nodaemon = TRUE;
	    break;

	case 'l':
	    rodent.level = atoi(optarg);
	    if ((rodent.level < 0) || (rodent.level > 4)) {
	        warnx("invalid argument `%s'", optarg);
	        usage();
	    }
	    break;

	case 'm':
	    if (!r_installmap(optarg)) {
	        warnx("invalid argument `%s'", optarg);
	        usage();
	    }
	    break;

	case 'p':
	    rodent.portname = optarg;
	    break;

	case 'r':
	    if (strcmp(optarg, "high") == 0)
	        rodent.resolution = MOUSE_RES_HIGH;
	    else if (strcmp(optarg, "medium-high") == 0)
	        rodent.resolution = MOUSE_RES_HIGH;
	    else if (strcmp(optarg, "medium-low") == 0)
	        rodent.resolution = MOUSE_RES_MEDIUMLOW;
	    else if (strcmp(optarg, "low") == 0)
	        rodent.resolution = MOUSE_RES_LOW;
	    else if (strcmp(optarg, "default") == 0)
	        rodent.resolution = MOUSE_RES_DEFAULT;
	    else {
	        rodent.resolution = atoi(optarg);
	        if (rodent.resolution <= 0) {
	            warnx("invalid argument `%s'", optarg);
	            usage();
	        }
	    }
	    break;

	case 's':
	    rodent.baudrate = 9600;
	    break;

	case 'z':
	    if (strcmp(optarg, "x") == 0)
		rodent.zmap = MOUSE_XAXIS;
	    else if (strcmp(optarg, "y") == 0)
		rodent.zmap = MOUSE_YAXIS;
            else {
		i = atoi(optarg);
		/* 
		 * Use button i for negative Z axis movement and 
		 * button (i + 1) for positive Z axis movement.
		 */
		if ((i <= 0) || (i > MOUSE_MAXBUTTON - 1)) {
	            warnx("invalid argument `%s'", optarg);
	            usage();
		}
		rodent.zmap = 1 << (i - 1);
	    }
	    break;

	case 'C':
	    rodent.clickthreshold = atoi(optarg);
	    if ((rodent.clickthreshold < 0) || 
	        (rodent.clickthreshold > MAX_CLICKTHRESHOLD)) {
	        warnx("invalid argument `%s'", optarg);
	        usage();
	    }
	    break;

	case 'D':
	    rodent.flags |= ClearDTR;
	    break;

	case 'F':
	    rodent.rate = atoi(optarg);
	    if (rodent.rate <= 0) {
	        warnx("invalid argument `%s'", optarg);
	        usage();
	    }
	    break;

	case 'P':
	    rodent.flags |= NoPnP;
	    break;

	case 'R':
	    rodent.flags |= ClearRTS;
	    break;

	case 'S':
	    rodent.baudrate = atoi(optarg);
	    if (rodent.baudrate <= 0) {
	        warnx("invalid argument `%s'", optarg);
	        usage();
	    }
	    debug("rodent baudrate %d", rodent.baudrate);
	    break;

	case 't':
	    if (strcmp(optarg, "auto") == 0) {
		rodent.rtype = MOUSE_PROTO_UNKNOWN;
		rodent.flags &= ~NoPnP;
		rodent.level = -1;
		break;
	    }
	    for (i = 0; rnames[i]; i++)
		if (strcmp(optarg, rnames[i]) == 0) {
		    rodent.rtype = i;
		    rodent.flags |= NoPnP;
		    rodent.level = (i == MOUSE_PROTO_SYSMOUSE) ? 1 : 0;
		    break;
		}
	    if (rnames[i])
		break;
	    warnx("no such mouse type `%s'", optarg);
	    usage();

	case 'h':
	case '?':
	default:
	    usage();
	}

    /* the default port name */
    switch(rodent.rtype) {

    case MOUSE_PROTO_INPORT:
        /* INPORT and BUS are the same... */
	rodent.rtype = MOUSE_PROTO_BUS;
	/* FALL THROUGH */
    case MOUSE_PROTO_BUS:
	if (!rodent.portname)
	    rodent.portname = "/dev/mse0";
	break;

    case MOUSE_PROTO_PS2:
	if (!rodent.portname)
	    rodent.portname = "/dev/psm0";
	break;

    default:
	if (rodent.portname)
	    break;
	warnx("no port name specified");
	usage();
    }

    for (;;) {
	if (setjmp(env) == 0) {
	    signal(SIGHUP, hup);
            if ((rodent.mfd = open(rodent.portname, O_RDWR | O_NONBLOCK, 0)) 
		== -1)
	        logerr(1, "unable to open %s", rodent.portname);
            if (r_identify() == MOUSE_PROTO_UNKNOWN) {
	        logwarnx("cannot determine mouse type on %s", rodent.portname);
	        close(rodent.mfd);
	        rodent.mfd = -1;
            }

	    /* print some information */
            if (identify != ID_NONE) {
		if (identify == ID_ALL)
                    printf("%s %s %s %s\n", 
		        rodent.portname, r_if(rodent.hw.iftype),
		        r_name(rodent.rtype), r_model(rodent.hw.model));
		else if (identify & ID_PORT)
		    printf("%s\n", rodent.portname);
		else if (identify & ID_IF)
		    printf("%s\n", r_if(rodent.hw.iftype));
		else if (identify & ID_TYPE)
		    printf("%s\n", r_name(rodent.rtype));
		else if (identify & ID_MODEL)
		    printf("%s\n", r_model(rodent.hw.model));
		exit(0);
	    } else {
                debug("port: %s  interface: %s  type: %s  model: %s", 
		    rodent.portname, r_if(rodent.hw.iftype),
		    r_name(rodent.rtype), r_model(rodent.hw.model));
	    }

	    if (rodent.mfd == -1) {
	        /*
	         * We cannot continue because of error.  Exit if the 
		 * program has not become a daemon.  Otherwise, block 
		 * until the the user corrects the problem and issues SIGHUP. 
	         */
	        if (!background)
		    exit(1);
	        sigpause(0);
	    }

            r_init();			/* call init function */
	    moused();
	}

	if (rodent.mfd != -1)
	    close(rodent.mfd);
	if (rodent.cfd != -1)
	    close(rodent.cfd);
	rodent.mfd = rodent.cfd = -1;
    }
    /* NOT REACHED */

    exit(0);
}

static void
moused(void)
{
    struct mouse_info mouse;
    mousestatus_t action;		/* original mouse action */
    mousestatus_t action2;		/* mapped action */
    fd_set fds;
    u_char b;

    if ((rodent.cfd = open("/dev/consolectl", O_RDWR, 0)) == -1)
	logerr(1, "cannot open /dev/consolectl", 0);

    if (!nodaemon && !background)
	if (daemon(0, 0)) {
	    logerr(1, "failed to become a daemon", 0);
	} else {
	    background = TRUE;
	}

    /* clear mouse data */
    bzero(&action, sizeof(action));
    bzero(&action2, sizeof(action2));
    bzero(&buttonstate, sizeof(buttonstate));
    bzero(&mouse, sizeof(mouse));

    /* choose which ioctl command to use */
    mouse.operation = MOUSE_MOTION_EVENT;
    extioctl = (ioctl(rodent.cfd, CONS_MOUSECTL, &mouse) == 0);

    /* process mouse data */
    for (;;) {

	FD_ZERO(&fds);
	FD_SET(rodent.mfd, &fds);
	if (select(FD_SETSIZE, &fds, NULL, NULL, NULL) <= 0)
	    logwarn("failed to read from mouse", 0);

	read(rodent.mfd, &b, 1);
	if (r_protocol(b, &action)) {	/* handler detected action */
	    r_map(&action, &action2);
	    debug("activity : buttons 0x%08x  dx %d  dy %d  dz %d",
		action2.button, action2.dx, action2.dy, action2.dz);

	    if (extioctl) {
	        r_click(&action2);
	        if (action2.flags & MOUSE_POSCHANGED) {
    		    mouse.operation = MOUSE_MOTION_EVENT;
	            mouse.u.data.buttons = action2.button;
	            mouse.u.data.x = action2.dx;
	            mouse.u.data.y = action2.dy;
	            mouse.u.data.z = action2.dz;
		    if (debug < 2)
	                ioctl(rodent.cfd, CONS_MOUSECTL, &mouse);
	        }
	    } else {
	        mouse.operation = MOUSE_ACTION;
	        mouse.u.data.buttons = action2.button;
	        mouse.u.data.x = action2.dx;
	        mouse.u.data.y = action2.dy;
	        mouse.u.data.z = action2.dz;
		if (debug < 2)
	            ioctl(rodent.cfd, CONS_MOUSECTL, &mouse);
	    }

            /*
	     * If the Z axis movement is mapped to a imaginary physical 
	     * button, we need to cook up a corresponding button `up' event
	     * after sending a button `down' event.
	     */
            if ((rodent.zmap > 0) && (action.dz != 0)) {
		action.obutton = action.button;
		action.dx = action.dy = action.dz = 0;
	        r_map(&action, &action2);
	        debug("activity : buttons 0x%08x  dx %d  dy %d  dz %d",
		    action2.button, action2.dx, action2.dy, action2.dz);

	        if (extioctl) {
	            r_click(&action2);
	        } else {
	            mouse.operation = MOUSE_ACTION;
	            mouse.u.data.buttons = action2.button;
		    mouse.u.data.x = mouse.u.data.y = mouse.u.data.z = 0;
		    if (debug < 2)
	                ioctl(rodent.cfd, CONS_MOUSECTL, &mouse);
	        }
	    }
	}
    }
    /* NOT REACHED */
}	    	

static void
hup(int sig)
{
    longjmp(env, 1);
}

/**
 ** usage
 **
 ** Complain, and free the CPU for more worthy tasks
 **/
static void
usage(void)
{
    fprintf(stderr, "%s\n%s\n%s\n",
        "usage: moused [-3DRcdfs] [-F rate] [-r resolution] [-S baudrate] [-C threshold]",
        "              [-m N=M] [-z N] [-t <mousetype>] -p <port>",
	"       moused [-d] -i -p <port>");
    exit(1);
}

/**
 ** Mouse interface code, courtesy of XFree86 3.1.2.
 **
 ** Note: Various bits have been trimmed, and in my shortsighted enthusiasm
 ** to clean, reformat and rationalise naming, it's quite possible that
 ** some things in here have been broken.
 **
 ** I hope not 8)
 **
 ** The following code is derived from a module marked :
 **/

/* $XConsortium: xf86_Mouse.c,v 1.2 94/10/12 20:33:21 kaleb Exp $ */
/* $XFree86: xc/programs/Xserver/hw/xfree86/common/xf86_Mouse.c,v 3.2 1995/01/28
 17:03:40 dawes Exp $ */
/*
 *
 * Copyright 1990,91 by Thomas Roell, Dinkelscherben, Germany.
 * Copyright 1993 by David Dawes <dawes@physics.su.oz.au>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the names of Thomas Roell and David Dawes not be
 * used in advertising or publicity pertaining to distribution of the
 * software without specific, written prior permission.  Thomas Roell
 * and David Dawes makes no representations about the suitability of this
 * software for any purpose.  It is provided "as is" without express or
 * implied warranty.
 *
 * THOMAS ROELL AND DAVID DAWES DISCLAIM ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS, IN NO EVENT SHALL THOMAS ROELL OR DAVID DAWES BE LIABLE FOR ANY
 * SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF
 * CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 */

/**
 ** GlidePoint support from XFree86 3.2.
 ** Derived from the module:
 **/

/* $XFree86: xc/programs/Xserver/hw/xfree86/common/xf86_Mouse.c,v 3.19 1996/10/16 14:40:51 dawes Exp $ */
/* $XConsortium: xf86_Mouse.c /main/10 1996/01/30 15:16:12 kaleb $ */

/* the following table must be ordered by MOUSE_PROTO_XXX in mouse.h */
static unsigned char proto[][7] = {
    /*  hd_mask hd_id   dp_mask dp_id   bytes b4_mask b4_id */
    { 	0x40,	0x40,	0x40,	0x00,	3,   ~0x23,  0x00 }, /* MicroSoft */
    {	0xf8,	0x80,	0x00,	0x00,	5,    0x00,  0xff }, /* MouseSystems */
    {	0xe0,	0x80,	0x80,	0x00,	3,    0x00,  0xff }, /* Logitech */
    {	0xe0,	0x80,	0x80,	0x00,	3,    0x00,  0xff }, /* MMSeries */
    { 	0x40,	0x40,	0x40,	0x00,	3,   ~0x33,  0x00 }, /* MouseMan */
    {	0xf8,	0x80,	0x00,	0x00,	5,    0x00,  0xff }, /* Bus */
    {	0xf8,	0x80,	0x00,	0x00,	5,    0x00,  0xff }, /* InPort */
    {	0xc0,	0x00,	0x00,	0x00,	3,    0x00,  0xff }, /* PS/2 mouse */
    {	0xe0,	0x80,	0x80,	0x00,	3,    0x00,  0xff }, /* MM HitTablet */
    { 	0x40,	0x40,	0x40,	0x00,	3,   ~0x33,  0x00 }, /* GlidePoint */
    { 	0x40,	0x40,	0x40,	0x00,	3,   ~0x3f,  0x00 }, /* IntelliMouse */
    { 	0x40,	0x40,	0x40,	0x00,	3,   ~0x33,  0x00 }, /* ThinkingMouse */
    {	0xf8,	0x80,	0x00,	0x00,	5,    0x00,  0xff }, /* sysmouse */
#if notyet
    {	0xf8,	0x80,	0x00,	0x00,	5,   ~0x2f,  0x10 }, /* Mariqua */
#endif
};
static unsigned char cur_proto[7];

static int
r_identify(void)
{
    char pnpbuf[256];	/* PnP identifier string may be up to 256 bytes long */
    pnpid_t pnpid;
    symtab_t *t;
    int level;
    int len;

    /* set the driver operation level, if applicable */
    if (rodent.level < 0)
	rodent.level = 1;
    ioctl(rodent.mfd, MOUSE_SETLEVEL, &rodent.level);
    rodent.level = (ioctl(rodent.mfd, MOUSE_GETLEVEL, &level) == 0) ? level : 0;

    /*
     * Interrogate the driver and get some intelligence on the device... 
     * The following ioctl functions are not always supported by device
     * drivers.  When the driver doesn't support them, we just trust the
     * user to supply valid information.
     */
    rodent.hw.iftype = MOUSE_IF_UNKNOWN;
    rodent.hw.model = MOUSE_MODEL_GENERIC;
    ioctl(rodent.mfd, MOUSE_GETHWINFO, &rodent.hw);

    if (rodent.rtype != MOUSE_PROTO_UNKNOWN)
        bcopy(proto[rodent.rtype], cur_proto, sizeof(cur_proto));
    rodent.mode.protocol = MOUSE_PROTO_UNKNOWN;
    rodent.mode.rate = -1;
    rodent.mode.resolution = MOUSE_RES_UNKNOWN;
    rodent.mode.accelfactor = 0;
    rodent.mode.level = 0;
    if (ioctl(rodent.mfd, MOUSE_GETMODE, &rodent.mode) == 0) {
        if ((rodent.mode.protocol == MOUSE_PROTO_UNKNOWN)
	    || (rodent.mode.protocol >= sizeof(proto)/sizeof(proto[0]))) {
	    logwarnx("unknown mouse protocol (%d)", rodent.mode.protocol);
	    return MOUSE_PROTO_UNKNOWN;
        } else {
	    /* INPORT and BUS are the same... */
	    if (rodent.mode.protocol == MOUSE_PROTO_INPORT)
	        rodent.mode.protocol = MOUSE_PROTO_BUS;
	    if (rodent.mode.protocol != rodent.rtype) {
		/* Hmm, the driver doesn't agree with the user... */
                if (rodent.rtype != MOUSE_PROTO_UNKNOWN)
	            logwarnx("mouse type mismatch (%s != %s), %s is assumed",
		        r_name(rodent.mode.protocol), r_name(rodent.rtype),
		        r_name(rodent.mode.protocol));
	        rodent.rtype = rodent.mode.protocol;
                bcopy(proto[rodent.rtype], cur_proto, sizeof(cur_proto));
	    }
        }
        cur_proto[4] = rodent.mode.packetsize;
        cur_proto[0] = rodent.mode.syncmask[0];	/* header byte bit mask */
        cur_proto[1] = rodent.mode.syncmask[1];	/* header bit pattern */
    }

    /* maybe this is an PnP mouse... */
    if (rodent.mode.protocol == MOUSE_PROTO_UNKNOWN) {

        if (rodent.flags & NoPnP)
            return rodent.rtype;
	if (((len = pnpgets(pnpbuf)) <= 0) || !pnpparse(&pnpid, pnpbuf, len))
            return rodent.rtype;

        debug("PnP serial mouse: '%*.*s' '%*.*s' '%*.*s'",
	    pnpid.neisaid, pnpid.neisaid, pnpid.eisaid, 
	    pnpid.ncompat, pnpid.ncompat, pnpid.compat, 
	    pnpid.ndescription, pnpid.ndescription, pnpid.description);

	/* we have a valid PnP serial device ID */
        rodent.hw.iftype = MOUSE_IF_SERIAL;
	t = pnpproto(&pnpid);
	if (t != NULL) {
            rodent.mode.protocol = t->val;
            rodent.hw.model = t->val2;
	} else {
            rodent.mode.protocol = MOUSE_PROTO_UNKNOWN;
	}
	if (rodent.mode.protocol == MOUSE_PROTO_INPORT)
	    rodent.mode.protocol = MOUSE_PROTO_BUS;

        /* make final adjustment */
	if (rodent.mode.protocol != MOUSE_PROTO_UNKNOWN) {
	    if (rodent.mode.protocol != rodent.rtype) {
		/* Hmm, the device doesn't agree with the user... */
                if (rodent.rtype != MOUSE_PROTO_UNKNOWN)
	            logwarnx("mouse type mismatch (%s != %s), %s is assumed",
		        r_name(rodent.mode.protocol), r_name(rodent.rtype),
		        r_name(rodent.mode.protocol));
	        rodent.rtype = rodent.mode.protocol;
                bcopy(proto[rodent.rtype], cur_proto, sizeof(cur_proto));
	    }
	}
    }

    debug("proto params: %02x %02x %02x %02x %d %02x %02x",
	cur_proto[0], cur_proto[1], cur_proto[2], cur_proto[3], 
	cur_proto[4], cur_proto[5], cur_proto[6]);

    return rodent.rtype;
}

static char *
r_if(int iftype)
{
    char *s;

    s = gettokenname(rifs, iftype);
    return (s == NULL) ? "unknown" : s;
}

static char *
r_name(int type)
{
    return ((type == MOUSE_PROTO_UNKNOWN) 
	|| (type > sizeof(rnames)/sizeof(rnames[0]) - 1))
	? "unknown" : rnames[type];
}

static char *
r_model(int model)
{
    char *s;

    s = gettokenname(rmodels, model);
    return (s == NULL) ? "unknown" : s;
}

static void
r_init(void)
{
    fd_set fds;
    char *s;
    char c;
    int i;

    /**
     ** This comment is a little out of context here, but it contains 
     ** some useful information...
     ********************************************************************
     **
     ** The following lines take care of the Logitech MouseMan protocols.
     **
     ** NOTE: There are different versions of both MouseMan and TrackMan!
     **       Hence I add another protocol P_LOGIMAN, which the user can
     **       specify as MouseMan in his XF86Config file. This entry was
     **       formerly handled as a special case of P_MS. However, people
     **       who don't have the middle button problem, can still specify
     **       Microsoft and use P_MS.
     **
     ** By default, these mice should use a 3 byte Microsoft protocol
     ** plus a 4th byte for the middle button. However, the mouse might
     ** have switched to a different protocol before we use it, so I send
     ** the proper sequence just in case.
     **
     ** NOTE: - all commands to (at least the European) MouseMan have to
     **         be sent at 1200 Baud.
     **       - each command starts with a '*'.
     **       - whenever the MouseMan receives a '*', it will switch back
     **	 to 1200 Baud. Hence I have to select the desired protocol
     **	 first, then select the baud rate.
     **
     ** The protocols supported by the (European) MouseMan are:
     **   -  5 byte packed binary protocol, as with the Mouse Systems
     **      mouse. Selected by sequence "*U".
     **   -  2 button 3 byte MicroSoft compatible protocol. Selected
     **      by sequence "*V".
     **   -  3 button 3+1 byte MicroSoft compatible protocol (default).
     **      Selected by sequence "*X".
     **
     ** The following baud rates are supported:
     **   -  1200 Baud (default). Selected by sequence "*n".
     **   -  9600 Baud. Selected by sequence "*q".
     **
     ** Selecting a sample rate is no longer supported with the MouseMan!
     ** Some additional lines in xf86Config.c take care of ill configured
     ** baud rates and sample rates. (The user will get an error.)
     */

    switch (rodent.rtype) {

    case MOUSE_PROTO_LOGI:
	/* 
	 * The baud rate selection command must be sent at the current
	 * baud rate; try all likely settings 
	 */
	setmousespeed(9600, rodent.baudrate, rodentcflags[rodent.rtype]);
	setmousespeed(4800, rodent.baudrate, rodentcflags[rodent.rtype]);
	setmousespeed(2400, rodent.baudrate, rodentcflags[rodent.rtype]);
	setmousespeed(1200, rodent.baudrate, rodentcflags[rodent.rtype]);
	/* select MM series data format */
	write(rodent.mfd, "S", 1);
	setmousespeed(rodent.baudrate, rodent.baudrate,
		      rodentcflags[MOUSE_PROTO_MM]);
	/* select report rate/frequency */
	if      (rodent.rate <= 0)   write(rodent.mfd, "O", 1);
	else if (rodent.rate <= 15)  write(rodent.mfd, "J", 1);
	else if (rodent.rate <= 27)  write(rodent.mfd, "K", 1);
	else if (rodent.rate <= 42)  write(rodent.mfd, "L", 1);
	else if (rodent.rate <= 60)  write(rodent.mfd, "R", 1);
	else if (rodent.rate <= 85)  write(rodent.mfd, "M", 1);
	else if (rodent.rate <= 125) write(rodent.mfd, "Q", 1);
	else			     write(rodent.mfd, "N", 1);
	break;

    case MOUSE_PROTO_LOGIMOUSEMAN:
	/* The command must always be sent at 1200 baud */
	setmousespeed(1200, 1200, rodentcflags[rodent.rtype]);
	write(rodent.mfd, "*X", 2);
	setmousespeed(1200, rodent.baudrate, rodentcflags[rodent.rtype]);
	break;

    case MOUSE_PROTO_HITTAB:
	setmousespeed(1200, rodent.baudrate, rodentcflags[rodent.rtype]);

	/*
	 * Initialize Hitachi PUMA Plus - Model 1212E to desired settings.
	 * The tablet must be configured to be in MM mode, NO parity,
	 * Binary Format.  xf86Info.sampleRate controls the sensativity
	 * of the tablet.  We only use this tablet for it's 4-button puck
	 * so we don't run in "Absolute Mode"
	 */
	write(rodent.mfd, "z8", 2);	/* Set Parity = "NONE" */
	usleep(50000);
	write(rodent.mfd, "zb", 2);	/* Set Format = "Binary" */
	usleep(50000);
	write(rodent.mfd, "@", 1);	/* Set Report Mode = "Stream" */
	usleep(50000);
	write(rodent.mfd, "R", 1);	/* Set Output Rate = "45 rps" */
	usleep(50000);
	write(rodent.mfd, "I\x20", 2);	/* Set Incrememtal Mode "20" */
	usleep(50000);
	write(rodent.mfd, "E", 1);	/* Set Data Type = "Relative */
	usleep(50000);

	/* Resolution is in 'lines per inch' on the Hitachi tablet */
	if      (rodent.resolution == MOUSE_RES_LOW) 		c = 'g';
	else if (rodent.resolution == MOUSE_RES_MEDIUMLOW)	c = 'e';
	else if (rodent.resolution == MOUSE_RES_MEDIUMHIGH)	c = 'h';
	else if (rodent.resolution == MOUSE_RES_HIGH)		c = 'd';
	else if (rodent.resolution <=   40) 			c = 'g';
	else if (rodent.resolution <=  100) 			c = 'd';
	else if (rodent.resolution <=  200) 			c = 'e';
	else if (rodent.resolution <=  500) 			c = 'h';
	else if (rodent.resolution <= 1000) 			c = 'j';
	else                                			c = 'd';
	write(rodent.mfd, &c, 1);
	usleep(50000);

	write(rodent.mfd, "\021", 1);	/* Resume DATA output */
	break;

    case MOUSE_PROTO_THINK:
	setmousespeed(1200, rodent.baudrate, rodentcflags[rodent.rtype]);
	/* the PnP ID string may be sent again, discard it */
	usleep(200000);
	i = FREAD;
	ioctl(rodent.mfd, TIOCFLUSH, &i);
	/* send the command to initialize the beast */
	for (s = "E5E5"; *s; ++s) {
	    write(rodent.mfd, s, 1);
	    FD_ZERO(&fds);
	    FD_SET(rodent.mfd, &fds);
	    if (select(FD_SETSIZE, &fds, NULL, NULL, NULL) <= 0)
		break;
	    read(rodent.mfd, &c, 1);
	    debug("%c", c);
	    if (c != *s)
	        break;
	}
	break;

    case MOUSE_PROTO_MSC:
	setmousespeed(1200, rodent.baudrate, rodentcflags[rodent.rtype]);
	if (rodent.flags & ClearDTR) {
	   i = TIOCM_DTR;
	   ioctl(rodent.mfd, TIOCMBIC, &i);
        }
        if (rodent.flags & ClearRTS) {
	   i = TIOCM_RTS;
	   ioctl(rodent.mfd, TIOCMBIC, &i);
        }
	break;

    case MOUSE_PROTO_SYSMOUSE:
	if (rodent.hw.iftype == MOUSE_IF_SYSMOUSE)
	    setmousespeed(1200, rodent.baudrate, rodentcflags[rodent.rtype]);
	/* fall through */

    case MOUSE_PROTO_BUS:
    case MOUSE_PROTO_INPORT:
    case MOUSE_PROTO_PS2:
	if (rodent.rate >= 0)
	    rodent.mode.rate = rodent.rate;
	if (rodent.resolution != MOUSE_RES_UNKNOWN)
	    rodent.mode.resolution = rodent.resolution;
	ioctl(rodent.mfd, MOUSE_SETMODE, &rodent.mode);
	break;

    default:
	setmousespeed(1200, rodent.baudrate, rodentcflags[rodent.rtype]);
	break;
    }
}

static int
r_protocol(u_char rBuf, mousestatus_t *act)
{
    /* MOUSE_MSS_BUTTON?DOWN -> MOUSE_BUTTON?DOWN */
    static int butmapmss[4] = {	/* Microsoft, MouseMan, GlidePoint, 
				   IntelliMouse, Thinking Mouse */
	0, 
	MOUSE_BUTTON3DOWN, 
	MOUSE_BUTTON1DOWN, 
	MOUSE_BUTTON1DOWN | MOUSE_BUTTON3DOWN, 
    };
    static int butmapmss2[4] = { /* Microsoft, MouseMan, GlidePoint, 
				    Thinking Mouse */
	0, 
	MOUSE_BUTTON4DOWN, 
	MOUSE_BUTTON2DOWN, 
	MOUSE_BUTTON2DOWN | MOUSE_BUTTON4DOWN, 
    };
    /* MOUSE_INTELLI_BUTTON?DOWN -> MOUSE_BUTTON?DOWN */
    static int butmapintelli[4] = { /* IntelliMouse, NetMouse, Mie Mouse,
				       MouseMan+ */
	0, 
	MOUSE_BUTTON2DOWN, 
	MOUSE_BUTTON4DOWN, 
	MOUSE_BUTTON2DOWN | MOUSE_BUTTON4DOWN, 
    };
    /* MOUSE_MSC_BUTTON?UP -> MOUSE_BUTTON?DOWN */
    static int butmapmsc[8] = {	/* MouseSystems, MMSeries, Logitech, 
				   Bus, sysmouse */
	0, 
	MOUSE_BUTTON3DOWN, 
	MOUSE_BUTTON2DOWN, 
	MOUSE_BUTTON2DOWN | MOUSE_BUTTON3DOWN, 
	MOUSE_BUTTON1DOWN, 
	MOUSE_BUTTON1DOWN | MOUSE_BUTTON3DOWN, 
	MOUSE_BUTTON1DOWN | MOUSE_BUTTON2DOWN,
	MOUSE_BUTTON1DOWN | MOUSE_BUTTON2DOWN | MOUSE_BUTTON3DOWN
    };
    /* MOUSE_PS2_BUTTON?DOWN -> MOUSE_BUTTON?DOWN */
    static int butmapps2[8] = {	/* PS/2 */
	0, 
	MOUSE_BUTTON1DOWN, 
	MOUSE_BUTTON3DOWN, 
	MOUSE_BUTTON1DOWN | MOUSE_BUTTON3DOWN, 
	MOUSE_BUTTON2DOWN, 
	MOUSE_BUTTON1DOWN | MOUSE_BUTTON2DOWN, 
	MOUSE_BUTTON2DOWN | MOUSE_BUTTON3DOWN,
	MOUSE_BUTTON1DOWN | MOUSE_BUTTON2DOWN | MOUSE_BUTTON3DOWN
    };
    /* for Hitachi tablet */
    static int butmaphit[8] = {	/* MM HitTablet */
	0, 
	MOUSE_BUTTON3DOWN, 
	MOUSE_BUTTON2DOWN, 
	MOUSE_BUTTON1DOWN, 
	MOUSE_BUTTON4DOWN, 
	MOUSE_BUTTON5DOWN, 
	MOUSE_BUTTON6DOWN, 
	MOUSE_BUTTON7DOWN, 
    };
    static int           pBufP = 0;
    static unsigned char pBuf[8];

    debug("received char 0x%x",(int)rBuf);

    /*
     * Hack for resyncing: We check here for a package that is:
     *  a) illegal (detected by wrong data-package header)
     *  b) invalid (0x80 == -128 and that might be wrong for MouseSystems)
     *  c) bad header-package
     *
     * NOTE: b) is a voilation of the MouseSystems-Protocol, since values of
     *       -128 are allowed, but since they are very seldom we can easily
     *       use them as package-header with no button pressed.
     * NOTE/2: On a PS/2 mouse any byte is valid as a data byte. Furthermore,
     *         0x80 is not valid as a header byte. For a PS/2 mouse we skip
     *         checking data bytes.
     *         For resyncing a PS/2 mouse we require the two most significant
     *         bits in the header byte to be 0. These are the overflow bits,
     *         and in case of an overflow we actually lose sync. Overflows
     *         are very rare, however, and we quickly gain sync again after
     *         an overflow condition. This is the best we can do. (Actually,
     *         we could use bit 0x08 in the header byte for resyncing, since
     *         that bit is supposed to be always on, but nobody told
     *         Microsoft...)
     */

    if (pBufP != 0 && rodent.rtype != MOUSE_PROTO_PS2 &&
	((rBuf & cur_proto[2]) != cur_proto[3] || rBuf == 0x80))
    {
	pBufP = 0;		/* skip package */
    }
    
    if (pBufP == 0 && (rBuf & cur_proto[0]) != cur_proto[1])
	return 0;

    /* is there an extra data byte? */
    if (pBufP >= cur_proto[4] && (rBuf & cur_proto[0]) != cur_proto[1])
    {
	/*
	 * Hack for Logitech MouseMan Mouse - Middle button
	 *
	 * Unfortunately this mouse has variable length packets: the standard
	 * Microsoft 3 byte packet plus an optional 4th byte whenever the
	 * middle button status changes.
	 *
	 * We have already processed the standard packet with the movement
	 * and button info.  Now post an event message with the old status
	 * of the left and right buttons and the updated middle button.
	 */

	/*
	 * Even worse, different MouseMen and TrackMen differ in the 4th
	 * byte: some will send 0x00/0x20, others 0x01/0x21, or even
	 * 0x02/0x22, so I have to strip off the lower bits.
         *
         * [JCH-96/01/21]
         * HACK for ALPS "fourth button". (It's bit 0x10 of the "fourth byte"
         * and it is activated by tapping the glidepad with the finger! 8^)
         * We map it to bit bit3, and the reverse map in xf86Events just has
         * to be extended so that it is identified as Button 4. The lower
         * half of the reverse-map may remain unchanged.
	 */

        /*
	 * [KY-97/08/03]
	 * Receive the fourth byte only when preceeding three bytes have
	 * been detected (pBufP >= cur_proto[4]).  In the previous
	 * versions, the test was pBufP == 0; thus, we may have mistakingly
	 * received a byte even if we didn't see anything preceeding 
	 * the byte.
	 */

	if ((rBuf & cur_proto[5]) != cur_proto[6]) {
            pBufP = 0;
	    return 0;
	}

	switch (rodent.rtype) {
#if notyet
	case MOUSE_PROTO_MARIQUA:
	    /* 
	     * This mouse has 16! buttons in addition to the standard
	     * three of them.  They return 0x10 though 0x1f in the
	     * so-called `ten key' mode and 0x30 though 0x3f in the
	     * `function key' mode.  As there are only 31 bits for 
	     * button state (including the standard three), we ignore
	     * the bit 0x20 and don't distinguish the two modes.
	     */
	    act->dx = act->dy = act->dz = 0;
	    act->obutton = act->button;
	    rBuf &= 0x1f;
	    act->button = (1 << (rBuf - 13))
                | (act->obutton & (MOUSE_BUTTON1DOWN | MOUSE_BUTTON3DOWN));
	    /* 
	     * FIXME: this is a button "down" event. There needs to be 
	     * a corresponding button "up" event... XXX
	     */
	    break;
#endif /* notyet */

	/*
	 * IntelliMouse, NetMouse (including NetMouse Pro) and Mie Mouse
	 * always send the fourth byte, whereas the fourth byte is
	 * optional for GlidePoint and ThinkingMouse. The fourth byte 
	 * is also optional for MouseMan+ and FirstMouse+ in their 
	 * native mode. It is always sent if they are in the IntelliMouse 
	 * compatible mode.
	 */ 
	case MOUSE_PROTO_INTELLI:	/* IntelliMouse, NetMouse, Mie Mouse,
					   MouseMan+ */
	    act->dx = act->dy = 0;
	    act->dz = (rBuf & 0x08) ? (rBuf & 0x0f) - 16 : (rBuf & 0x0f);
	    act->obutton = act->button;
	    act->button = butmapintelli[(rBuf & MOUSE_MSS_BUTTONS) >> 4]
		| (act->obutton & (MOUSE_BUTTON1DOWN | MOUSE_BUTTON3DOWN));
	    break;

	default:
	    act->dx = act->dy = act->dz = 0;
	    act->obutton = act->button;
	    act->button = butmapmss2[(rBuf & MOUSE_MSS_BUTTONS) >> 4]
		| (act->obutton & (MOUSE_BUTTON1DOWN | MOUSE_BUTTON3DOWN));
	    break;
	}

	act->flags = ((act->dx || act->dy || act->dz) ? MOUSE_POSCHANGED : 0)
	    | (act->obutton ^ act->button);
        pBufP = 0;
	return act->flags;
    }
        
    if (pBufP >= cur_proto[4])
	pBufP = 0;
    pBuf[pBufP++] = rBuf;
    if (pBufP != cur_proto[4])
	return 0;
    
    /*
     * assembly full package
     */

    debug("assembled full packet (len %d) %x,%x,%x,%x,%x,%x,%x,%x",
	cur_proto[4], 
	pBuf[0], pBuf[1], pBuf[2], pBuf[3], 
	pBuf[4], pBuf[5], pBuf[6], pBuf[7]);

    act->dz = 0;
    act->obutton = act->button;
    switch (rodent.rtype) 
    {
    case MOUSE_PROTO_MS:		/* Microsoft */
    case MOUSE_PROTO_LOGIMOUSEMAN:	/* MouseMan/TrackMan */
	act->button = act->obutton & MOUSE_BUTTON4DOWN;
	if (rodent.flags & ChordMiddle)
	    act->button |= ((pBuf[0] & MOUSE_MSS_BUTTONS) == MOUSE_MSS_BUTTONS)
		? MOUSE_BUTTON2DOWN 
		: butmapmss[(pBuf[0] & MOUSE_MSS_BUTTONS) >> 4];
	else
	    act->button |= (act->obutton & MOUSE_BUTTON2DOWN)
		| butmapmss[(pBuf[0] & MOUSE_MSS_BUTTONS) >> 4];
	act->dx = (char)(((pBuf[0] & 0x03) << 6) | (pBuf[1] & 0x3F));
	act->dy = (char)(((pBuf[0] & 0x0C) << 4) | (pBuf[2] & 0x3F));
	break;

    case MOUSE_PROTO_GLIDEPOINT:	/* GlidePoint */
    case MOUSE_PROTO_THINK:		/* ThinkingMouse */
    case MOUSE_PROTO_INTELLI:		/* IntelliMouse, NetMouse, Mie Mouse,
					   MouseMan+ */
	act->button = (act->obutton & (MOUSE_BUTTON2DOWN | MOUSE_BUTTON4DOWN))
            | butmapmss[(pBuf[0] & MOUSE_MSS_BUTTONS) >> 4];
	act->dx = (char)(((pBuf[0] & 0x03) << 6) | (pBuf[1] & 0x3F));
	act->dy = (char)(((pBuf[0] & 0x0C) << 4) | (pBuf[2] & 0x3F));
	break;
      
    case MOUSE_PROTO_MSC:		/* MouseSystems Corp */
#if notyet
    case MOUSE_PROTO_MARIQUA:		/* Mariqua */
#endif
	act->button = butmapmsc[(~pBuf[0]) & MOUSE_MSC_BUTTONS];
	act->dx =    (char)(pBuf[1]) + (char)(pBuf[3]);
	act->dy = - ((char)(pBuf[2]) + (char)(pBuf[4]));
	break;
      
    case MOUSE_PROTO_HITTAB:		/* MM HitTablet */
	act->button = butmaphit[pBuf[0] & 0x07];
	act->dx = (pBuf[0] & MOUSE_MM_XPOSITIVE) ?   pBuf[1] : - pBuf[1];
	act->dy = (pBuf[0] & MOUSE_MM_YPOSITIVE) ? - pBuf[2] :   pBuf[2];
	break;

    case MOUSE_PROTO_MM:		/* MM Series */
    case MOUSE_PROTO_LOGI:		/* Logitech Mice */
	act->button = butmapmsc[pBuf[0] & MOUSE_MSC_BUTTONS];
	act->dx = (pBuf[0] & MOUSE_MM_XPOSITIVE) ?   pBuf[1] : - pBuf[1];
	act->dy = (pBuf[0] & MOUSE_MM_YPOSITIVE) ? - pBuf[2] :   pBuf[2];
	break;
      
    case MOUSE_PROTO_BUS:		/* Bus */
    case MOUSE_PROTO_INPORT:		/* InPort */
	act->button = butmapmsc[(~pBuf[0]) & MOUSE_MSC_BUTTONS];
	act->dx =   (char)pBuf[1];
	act->dy = - (char)pBuf[2];
	break;

    case MOUSE_PROTO_PS2:		/* PS/2 */
	act->button = butmapps2[pBuf[0] & MOUSE_PS2_BUTTONS];
	act->dx = (pBuf[0] & MOUSE_PS2_XNEG) ?    pBuf[1] - 256  :  pBuf[1];
	act->dy = (pBuf[0] & MOUSE_PS2_YNEG) ?  -(pBuf[2] - 256) : -pBuf[2];
	/*
	 * Moused usually operates the psm driver at the operation level 1
	 * which sends mouse data in MOUSE_PROTO_SYSMOUSE protocol.
	 * The following code takes effect only when the user explicitly 
	 * requets the level 2 at which wheel movement and additional button 
	 * actions are encoded in model-dependent formats. At the level 0
	 * the following code is no-op because the psm driver says the model
	 * is MOUSE_MODEL_GENERIC.
	 */
	switch (rodent.hw.model) {
	case MOUSE_MODEL_INTELLI:
	case MOUSE_MODEL_NET:
	    /* wheel data is in the fourth byte */
	    act->dz = (char)pBuf[3];
	    break;
	case MOUSE_MODEL_MOUSEMANPLUS:
	    if ((pBuf[0] & ~MOUSE_PS2_BUTTONS) == 0xc8) {
		/* the extended data packet encodes button and wheel events */
		act->dx = act->dy = 0;
		act->dz = (pBuf[1] & MOUSE_PS2PLUS_ZNEG)
		    ? (pBuf[2] & 0x0f) - 16 : (pBuf[2] & 0x0f);
		act->button |= ((pBuf[2] & MOUSE_PS2PLUS_BUTTON4DOWN)
		    ? MOUSE_BUTTON4DOWN : 0);
	    } else {
		/* preserve button states */
		act->button |= act->obutton & MOUSE_EXTBUTTONS;
	    }
	    break;
	case MOUSE_MODEL_GLIDEPOINT:
	    /* `tapping' action */
	    act->button |= ((pBuf[0] & MOUSE_PS2_TAP)) ? 0 : MOUSE_BUTTON4DOWN;
	    break;
	case MOUSE_MODEL_NETSCROLL:
	    /* three addtional bytes encode button and wheel events */
	    act->button |= (pBuf[3] & MOUSE_PS2_BUTTON3DOWN) 
		? MOUSE_BUTTON4DOWN : 0;
	    act->dz = (pBuf[3] & MOUSE_PS2_XNEG) ? pBuf[4] - 256 : pBuf[4];
	    break;
	case MOUSE_MODEL_THINK:
	    /* the fourth button state in the first byte */
	    act->button |= (pBuf[0] & MOUSE_PS2_TAP) ? MOUSE_BUTTON4DOWN : 0;
	    break;
	case MOUSE_MODEL_GENERIC:
	default:
	    break;
	}
	break;

    case MOUSE_PROTO_SYSMOUSE:		/* sysmouse */
	act->button = butmapmsc[(~pBuf[0]) & MOUSE_SYS_STDBUTTONS];
	act->dx =    (char)(pBuf[1]) + (char)(pBuf[3]);
	act->dy = - ((char)(pBuf[2]) + (char)(pBuf[4]));
	if (rodent.level == 1) {
	    act->dz = ((char)(pBuf[5] << 1) + (char)(pBuf[6] << 1))/2;
	    act->button |= ((~pBuf[7] & MOUSE_SYS_EXTBUTTONS) << 3);
	}
	break;

    default:
	return 0;
    }
    /* 
     * We don't reset pBufP here yet, as there may be an additional data
     * byte in some protocols. See above.
     */

    /* has something changed? */
    act->flags = ((act->dx || act->dy || act->dz) ? MOUSE_POSCHANGED : 0)
	| (act->obutton ^ act->button);

    if (rodent.flags & Emulate3Button) {
	if (((act->flags & (MOUSE_BUTTON1DOWN | MOUSE_BUTTON3DOWN))
	        == (MOUSE_BUTTON1DOWN | MOUSE_BUTTON3DOWN))
	    && ((act->button & (MOUSE_BUTTON1DOWN | MOUSE_BUTTON3DOWN))
	        == (MOUSE_BUTTON1DOWN | MOUSE_BUTTON3DOWN))) {
	    act->button &= ~(MOUSE_BUTTON1DOWN | MOUSE_BUTTON3DOWN);
	    act->button |= MOUSE_BUTTON2DOWN;
	} else if ((act->obutton & MOUSE_BUTTON2DOWN)
	    && ((act->button & (MOUSE_BUTTON1DOWN | MOUSE_BUTTON3DOWN))
	        != (MOUSE_BUTTON1DOWN | MOUSE_BUTTON3DOWN))) {
	    act->button &= ~(MOUSE_BUTTON1DOWN | MOUSE_BUTTON2DOWN 
			       | MOUSE_BUTTON3DOWN);
	}
	act->flags &= MOUSE_POSCHANGED;
	act->flags |= act->obutton ^ act->button;
    }

    return act->flags;
}

/* phisical to logical button mapping */
static int p2l[MOUSE_MAXBUTTON] = {
    MOUSE_BUTTON1DOWN, MOUSE_BUTTON2DOWN, MOUSE_BUTTON3DOWN, MOUSE_BUTTON4DOWN, 
    MOUSE_BUTTON5DOWN, MOUSE_BUTTON6DOWN, MOUSE_BUTTON7DOWN, MOUSE_BUTTON8DOWN, 
    0x00000100,        0x00000200,        0x00000400,        0x00000800,
    0x00001000,        0x00002000,        0x00004000,        0x00008000,
    0x00010000,        0x00020000,        0x00040000,        0x00080000,
    0x00100000,        0x00200000,        0x00400000,        0x00800000,
    0x01000000,        0x02000000,        0x04000000,        0x08000000,
    0x10000000,        0x20000000,        0x40000000,
};

static char *
skipspace(char *s)
{
    while(isspace(*s))
	++s;
    return s;
}

static int
r_installmap(char *arg)
{
    int pbutton;
    int lbutton;
    char *s;

    while (*arg) {
	arg = skipspace(arg);
	s = arg;
	while (isdigit(*arg))
	    ++arg;
	arg = skipspace(arg);
	if ((arg <= s) || (*arg != '='))
	    return FALSE;
	lbutton = atoi(s);

	arg = skipspace(++arg);
	s = arg;
	while (isdigit(*arg))
	    ++arg;
	if ((arg <= s) || (!isspace(*arg) && (*arg != '\0')))
	    return FALSE;
	pbutton = atoi(s);

	if ((lbutton <= 0) || (lbutton > MOUSE_MAXBUTTON))
	    return FALSE;
	if ((pbutton <= 0) || (pbutton > MOUSE_MAXBUTTON))
	    return FALSE;
	p2l[pbutton - 1] = 1 << (lbutton - 1);
    }

    return TRUE;
}

static void
r_map(mousestatus_t *act1, mousestatus_t *act2)
{
    register int pb;
    register int pbuttons;
    int lbuttons;

    pbuttons = act1->button;
    lbuttons = 0;

    act2->obutton = act2->button;
    act2->dx = act1->dx;
    act2->dy = act1->dy;
    act2->dz = act1->dz;

    switch (rodent.zmap) {
    case 0:	/* do nothing */
	break;
    case MOUSE_XAXIS:
	if (act1->dz != 0) {
	    act2->dx = act1->dz;
	    act2->dz = 0;
	}
	break;
    case MOUSE_YAXIS:
	if (act1->dz != 0) {
	    act2->dy = act1->dz;
	    act2->dz = 0;
	}
	break;
    default:	/* buttons */
	pbuttons &= ~(rodent.zmap | (rodent.zmap << 1));
	if (act1->dz < 0)
	    pbuttons |= rodent.zmap;
	else if (act1->dz > 0)
	    pbuttons |= (rodent.zmap << 1);
	act2->dz = 0;
	break;
    }

    for (pb = 0; (pb < MOUSE_MAXBUTTON) && (pbuttons != 0); ++pb) {
	lbuttons |= (pbuttons & 1) ? p2l[pb] : 0;
	pbuttons >>= 1;
    }
    act2->button = lbuttons;

    act2->flags = ((act2->dx || act2->dy || act2->dz) ? MOUSE_POSCHANGED : 0)
	| (act2->obutton ^ act2->button);
}

static void
r_click(mousestatus_t *act)
{
    struct mouse_info mouse;
    struct timeval tv;
    struct timeval tv1;
    struct timeval tv2;
    struct timezone tz;
    int button;
    int mask;
    int i;

    mask = act->flags & MOUSE_BUTTONS;
    if (mask == 0)
	return;

    gettimeofday(&tv1, &tz);
    tv2.tv_sec = rodent.clickthreshold/1000;
    tv2.tv_usec = (rodent.clickthreshold%1000)*1000;
    timersub(&tv1, &tv2, &tv); 
    debug("tv:  %ld %ld", tv.tv_sec, tv.tv_usec);
    button = MOUSE_BUTTON1DOWN;
    for (i = 0; (i < MOUSE_MAXBUTTON) && (mask != 0); ++i) {
        if (mask & 1) {
            if (act->button & button) {
                /* the button is down */
    		debug("  :  %ld %ld", 
		    buttonstate[i].tv.tv_sec, buttonstate[i].tv.tv_usec);
		if (timercmp(&tv, &buttonstate[i].tv, >)) {
                    buttonstate[i].tv.tv_sec = 0;
                    buttonstate[i].tv.tv_usec = 0;
                    buttonstate[i].count = 1;
                } else {
                    ++buttonstate[i].count;
                }
	        mouse.u.event.value = buttonstate[i].count;
            } else {
                /* the button is up */
                buttonstate[i].tv = tv1;
	        mouse.u.event.value = 0;
            }
	    mouse.operation = MOUSE_BUTTON_EVENT;
	    mouse.u.event.id = button;
	    if (debug < 2)
	        ioctl(rodent.cfd, CONS_MOUSECTL, &mouse);
	    debug("button %d  count %d", i + 1, mouse.u.event.value);
        }
	button <<= 1;
	mask >>= 1;
    }
}

/* $XConsortium: posix_tty.c,v 1.3 95/01/05 20:42:55 kaleb Exp $ */
/* $XFree86: xc/programs/Xserver/hw/xfree86/os-support/shared/posix_tty.c,v 3.4 1995/01/28 17:05:03 dawes Exp $ */
/*
 * Copyright 1993 by David Dawes <dawes@physics.su.oz.au>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of David Dawes 
 * not be used in advertising or publicity pertaining to distribution of 
 * the software without specific, written prior permission.
 * David Dawes makes no representations about the suitability of this 
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 *
 * DAVID DAWES DISCLAIMS ALL WARRANTIES WITH REGARD TO 
 * THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND 
 * FITNESS, IN NO EVENT SHALL DAVID DAWES BE LIABLE FOR 
 * ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER 
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF 
 * CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN 
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 */


static void
setmousespeed(int old, int new, unsigned cflag)
{
	struct termios tty;
	char *c;

	if (tcgetattr(rodent.mfd, &tty) < 0)
	{
		logwarn("unable to get status of mouse fd", 0);
		return;
	}

	tty.c_iflag = IGNBRK | IGNPAR;
	tty.c_oflag = 0;
	tty.c_lflag = 0;
	tty.c_cflag = (tcflag_t)cflag;
	tty.c_cc[VTIME] = 0;
	tty.c_cc[VMIN] = 1;

	switch (old)
	{
	case 9600:
		cfsetispeed(&tty, B9600);
		cfsetospeed(&tty, B9600);
		break;
	case 4800:
		cfsetispeed(&tty, B4800);
		cfsetospeed(&tty, B4800);
		break;
	case 2400:
		cfsetispeed(&tty, B2400);
		cfsetospeed(&tty, B2400);
		break;
	case 1200:
	default:
		cfsetispeed(&tty, B1200);
		cfsetospeed(&tty, B1200);
	}

	if (tcsetattr(rodent.mfd, TCSADRAIN, &tty) < 0)
	{
		logwarn("unable to set status of mouse fd", 0);
		return;
	}

	switch (new)
	{
	case 9600:
		c = "*q";
		cfsetispeed(&tty, B9600);
		cfsetospeed(&tty, B9600);
		break;
	case 4800:
		c = "*p";
		cfsetispeed(&tty, B4800);
		cfsetospeed(&tty, B4800);
		break;
	case 2400:
		c = "*o";
		cfsetispeed(&tty, B2400);
		cfsetospeed(&tty, B2400);
		break;
	case 1200:
	default:
		c = "*n";
		cfsetispeed(&tty, B1200);
		cfsetospeed(&tty, B1200);
	}

	if (rodent.rtype == MOUSE_PROTO_LOGIMOUSEMAN 
	    || rodent.rtype == MOUSE_PROTO_LOGI)
	{
		if (write(rodent.mfd, c, 2) != 2)
		{
			logwarn("unable to write to mouse fd", 0);
			return;
		}
	}
	usleep(100000);

	if (tcsetattr(rodent.mfd, TCSADRAIN, &tty) < 0)
		logwarn("unable to set status of mouse fd", 0);
}

/* 
 * PnP COM device support 
 * 
 * It's a simplistic implementation, but it works :-)
 * KY, 31/7/97.
 */

/*
 * Try to elicit a PnP ID as described in 
 * Microsoft, Hayes: "Plug and Play External COM Device Specification, 
 * rev 1.00", 1995.
 *
 * The routine does not fully implement the COM Enumerator as par Section
 * 2.1 of the document.  In particular, we don't have idle state in which
 * the driver software monitors the com port for dynamic connection or 
 * removal of a device at the port, because `moused' simply quits if no 
 * device is found.
 *
 * In addition, as PnP COM device enumeration procedure slightly has 
 * changed since its first publication, devices which follow earlier
 * revisions of the above spec. may fail to respond if the rev 1.0 
 * procedure is used. XXX
 */
static int
pnpgets(char *buf)
{
    struct timeval timeout;
    fd_set fds;
    int i;
    char c;

#if 0
    /* 
     * This is the procedure described in rev 1.0 of PnP COM device spec.
     * Unfortunately, some devices which comform to earlier revisions of
     * the spec gets confused and do not return the ID string...
     */

    /* port initialization (2.1.2) */
    ioctl(rodent.mfd, TIOCMGET, &i);
    i |= TIOCM_DTR;		/* DTR = 1 */
    i &= ~TIOCM_RTS;		/* RTS = 0 */
    ioctl(rodent.mfd, TIOCMSET, &i);
    usleep(200000);
    if ((ioctl(rodent.mfd, TIOCMGET, &i) == -1) || ((i & TIOCM_DSR) == 0))
	goto disconnect_idle;

    /* port setup, 1st phase (2.1.3) */
    setmousespeed(1200, 1200, (CS7 | CREAD | CLOCAL | HUPCL));
    i = TIOCM_DTR | TIOCM_RTS;	/* DTR = 0, RTS = 0 */
    ioctl(rodent.mfd, TIOCMBIC, &i);
    usleep(200000);
    i = TIOCM_DTR;		/* DTR = 1, RTS = 0 */
    ioctl(rodent.mfd, TIOCMBIS, &i);
    usleep(200000);

    /* wait for response, 1st phase (2.1.4) */
    i = FREAD;
    ioctl(rodent.mfd, TIOCFLUSH, &i);
    i = TIOCM_RTS;		/* DTR = 1, RTS = 1 */
    ioctl(rodent.mfd, TIOCMBIS, &i);

    /* try to read something */
    FD_ZERO(&fds);
    FD_SET(rodent.mfd, &fds);
    timeout.tv_sec = 0;
    timeout.tv_usec = 200000;
    if (select(FD_SETSIZE, &fds, NULL, NULL, &timeout) <= 0) {

	/* port setup, 2nd phase (2.1.5) */
        i = TIOCM_DTR | TIOCM_RTS;	/* DTR = 0, RTS = 0 */
        ioctl(rodent.mfd, TIOCMBIC, &i);
        usleep(200000);

	/* wait for respose, 2nd phase (2.1.6) */
        i = FREAD;
        ioctl(rodent.mfd, TIOCFLUSH, &i);
        i = TIOCM_DTR | TIOCM_RTS;	/* DTR = 1, RTS = 1 */
        ioctl(rodent.mfd, TIOCMBIS, &i);

        /* try to read something */
        FD_ZERO(&fds);
        FD_SET(rodent.mfd, &fds);
        timeout.tv_sec = 0;
        timeout.tv_usec = 200000;
        if (select(FD_SETSIZE, &fds, NULL, NULL, &timeout) <= 0)
	    goto connect_idle;
    }
#else
    /*
     * This is a simplified procedure; it simply toggles RTS.
     */

    ioctl(rodent.mfd, TIOCMGET, &i);
    i |= TIOCM_DTR;		/* DTR = 1 */
    i &= ~TIOCM_RTS;		/* RTS = 0 */
    ioctl(rodent.mfd, TIOCMSET, &i);
    usleep(200000);

    setmousespeed(1200, 1200, (CS7 | CREAD | CLOCAL | HUPCL));

    /* wait for respose */
    i = FREAD;
    ioctl(rodent.mfd, TIOCFLUSH, &i);
    i = TIOCM_DTR | TIOCM_RTS;	/* DTR = 1, RTS = 1 */
    ioctl(rodent.mfd, TIOCMBIS, &i);

    /* try to read something */
    FD_ZERO(&fds);
    FD_SET(rodent.mfd, &fds);
    timeout.tv_sec = 0;
    timeout.tv_usec = 200000;
    if (select(FD_SETSIZE, &fds, NULL, NULL, &timeout) <= 0)
        goto connect_idle;
#endif

    /* collect PnP COM device ID (2.1.7) */
    i = 0;
    usleep(200000);	/* the mouse must send `Begin ID' within 200msec */
    while (read(rodent.mfd, &c, 1) == 1) {
	/* we may see "M", or "M3..." before `Begin ID' */
        if ((c == 0x08) || (c == 0x28)) {	/* Begin ID */
	    buf[i++] = c;
	    break;
        }
        debug("%c %02x", c, c);
    }
    if (i <= 0) {
	/* we haven't seen `Begin ID' in time... */
	goto connect_idle;
    }

    ++c;			/* make it `End ID' */
    for (;;) {
        FD_ZERO(&fds);
        FD_SET(rodent.mfd, &fds);
        timeout.tv_sec = 0;
        timeout.tv_usec = 200000;
        if (select(FD_SETSIZE, &fds, NULL, NULL, &timeout) <= 0)
	    break;

	read(rodent.mfd, &buf[i], 1);
        if (buf[i++] == c)	/* End ID */
	    break;
	if (i >= 256)
	    break;
    }
    /* string may not be human readable... */
    debug("'%-*.*s'", i, i, buf);
    if (buf[i - 1] != c)
	goto connect_idle;
    return i;

    /*
     * According to PnP spec, we should set DTR = 1 and RTS = 0 while 
     * in idle state.  But, `moused' shall set DTR = RTS = 1 and proceed, 
     * assuming there is something at the port even if it didn't 
     * respond to the PnP enumeration procedure.
     */
disconnect_idle:
    i = TIOCM_DTR | TIOCM_RTS;		/* DTR = 1, RTS = 1 */
    ioctl(rodent.mfd, TIOCMBIS, &i);
connect_idle:
    return 0;
}

static int
pnpparse(pnpid_t *id, char *buf, int len)
{
    char s[3];
    int offset;
    int sum = 0;
    int i, j;

    id->revision = 0;
    id->eisaid = NULL;
    id->serial = NULL;
    id->class = NULL;
    id->compat = NULL;
    id->description = NULL;
    id->neisaid = 0;
    id->nserial = 0;
    id->nclass = 0;
    id->ncompat = 0;
    id->ndescription = 0;

    offset = 0x28 - buf[0];

    /* calculate checksum */
    for (i = 0; i < len - 3; ++i) {
	sum += buf[i];
	buf[i] += offset;
    }
    sum += buf[len - 1];
    for (; i < len; ++i)
	buf[i] += offset;
    debug("PnP ID string: '%*.*s'", len, len, buf);

    /* revision */
    buf[1] -= offset;
    buf[2] -= offset;
    id->revision = ((buf[1] & 0x3f) << 6) | (buf[2] & 0x3f);
    debug("PnP rev %d.%02d", id->revision / 100, id->revision % 100);

    /* EISA vender and product ID */
    id->eisaid = &buf[3];
    id->neisaid = 7;

    /* option strings */
    i = 10;
    if (buf[i] == '\\') {
        /* device serial # */
        for (j = ++i; i < len; ++i) {
            if (buf[i] == '\\')
		break;
        }
	if (i >= len)
	    i -= 3;
	if (i - j == 8) {
            id->serial = &buf[j];
            id->nserial = 8;
	}
    }
    if (buf[i] == '\\') {
        /* PnP class */
        for (j = ++i; i < len; ++i) {
            if (buf[i] == '\\')
		break;
        }
	if (i >= len)
	    i -= 3;
	if (i > j + 1) {
            id->class = &buf[j];
            id->nclass = i - j;
        }
    }
    if (buf[i] == '\\') {
	/* compatible driver */
        for (j = ++i; i < len; ++i) {
            if (buf[i] == '\\')
		break;
        }
	/*
	 * PnP COM spec prior to v0.96 allowed '*' in this field, 
	 * it's not allowed now; just igore it.
	 */
	if (buf[j] == '*')
	    ++j;
	if (i >= len)
	    i -= 3;
	if (i > j + 1) {
            id->compat = &buf[j];
            id->ncompat = i - j;
        }
    }
    if (buf[i] == '\\') {
	/* product description */
        for (j = ++i; i < len; ++i) {
            if (buf[i] == ';')
		break;
        }
	if (i >= len)
	    i -= 3;
	if (i > j + 1) {
            id->description = &buf[j];
            id->ndescription = i - j;
        }
    }

    /* checksum exists if there are any optional fields */
    if ((id->nserial > 0) || (id->nclass > 0)
	|| (id->ncompat > 0) || (id->ndescription > 0)) {
        debug("PnP checksum: 0x%X", sum); 
        sprintf(s, "%02X", sum & 0x0ff);
        if (strncmp(s, &buf[len - 3], 2) != 0) {
#if 0
            /*
	     * I found some mice do not comply with the PnP COM device 
	     * spec regarding checksum... XXX
	     */
            logwarnx("PnP checksum error", 0);
	    return FALSE;
#endif
        }
    }

    return TRUE;
}

static symtab_t *
pnpproto(pnpid_t *id)
{
    symtab_t *t;
    int i, j;

    if (id->nclass > 0)
	if (strncmp(id->class, "MOUSE", id->nclass) != 0)
	    /* this is not a mouse! */
	    return NULL;

    if (id->neisaid > 0) {
        t = gettoken(pnpprod, id->eisaid, id->neisaid);
	if (t->val != MOUSE_PROTO_UNKNOWN)
            return t;
    }

    /*
     * The 'Compatible drivers' field may contain more than one
     * ID separated by ','.
     */
    if (id->ncompat <= 0)
	return NULL;
    for (i = 0; i < id->ncompat; ++i) {
        for (j = i; id->compat[i] != ','; ++i)
            if (i >= id->ncompat)
		break;
        if (i > j) {
            t = gettoken(pnpprod, id->compat + j, i - j);
	    if (t->val != MOUSE_PROTO_UNKNOWN)
                return t;
	}
    }

    return NULL;
}

/* name/val mapping */

static symtab_t *
gettoken(symtab_t *tab, char *s, int len)
{
    int i;

    for (i = 0; tab[i].name != NULL; ++i) {
	if (strncmp(tab[i].name, s, len) == 0)
	    break;
    }
    return &tab[i];
}

static char *
gettokenname(symtab_t *tab, int val)
{
    int i;

    for (i = 0; tab[i].name != NULL; ++i) {
	if (tab[i].val == val)
	    return tab[i].name;
    }
    return NULL;
}
