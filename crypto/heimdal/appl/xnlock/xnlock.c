/*
 * xnlock -- Dan Heller, 1990
 * "nlock" is a "new lockscreen" type program... something that prevents
 * screen burnout by making most of it "black" while providing something
 * of interest to be displayed in case anyone is watching.
 * "xnlock" is the X11 version of the program.
 * Original sunview version written by Dan Heller 1985 (not included here).
 */
#ifdef HAVE_CONFIG_H
#include <config.h>
RCSID("$Id: xnlock.c,v 1.85 2001/03/15 17:13:13 joda Exp $");
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <X11/StringDefs.h>
#include <X11/Intrinsic.h>
#include <X11/keysym.h>
#include <X11/Shell.h>
#include <X11/Xos.h>
#ifdef strerror
#undef strerror
#endif
#include <ctype.h>
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_PWD_H
#include <pwd.h>
#endif

#ifdef KRB5
#include <krb5.h>
#endif
#ifdef KRB4
#include <krb.h>
#include <kafs.h>
#endif

#include <roken.h>
#include <err.h>

static char login[16];
static char userprompt[128];
#ifdef KRB4
static char name[ANAME_SZ];
static char inst[INST_SZ];
static char realm[REALM_SZ];
#endif
#ifdef KRB5
static krb5_context context;
static krb5_principal client;
#endif

#define font_height(font)	  	(font->ascent + font->descent)

static char *SPACE_STRING = "                                                      ";
static char STRING[] = "****************";

#define STRING_LENGTH (sizeof(STRING))
#define MAX_PASSWD_LENGTH 256
/* (sizeof(STRING)) */

#define PROMPT	    "Password: "
#define FAIL_MSG    "Sorry, try again"
#define LEFT 	001
#define RIGHT 	002
#define DOWN 	004
#define UP 	010
#define FRONT	020
#define X_INCR 3
#define Y_INCR 2
#define XNLOCK_CTRL 1
#define XNLOCK_NOCTRL 0

static XtAppContext	app;
static Display        *dpy;
static unsigned short	Width, Height;
static Widget		widget;
static GC		gc;
static XtIntervalId	timeout_id;
static char	       *words;
static int		x, y;
static Pixel		Black, White;
static XFontStruct    *font;
static char		root_cpass[128];
static char		user_cpass[128];
static int		time_left, prompt_x, prompt_y, time_x, time_y;
static unsigned long	interval;
static Pixmap		left0, left1, right0, right1, left_front,
			right_front, front, down;

#define MAXLINES 40

#define IS_MOVING  1
#define GET_PASSWD 2
static int state; /* indicates states: walking or getting passwd */

static int ALLOW_LOGOUT = (60*10);	/* Allow logout after nn seconds */
#define LOGOUT_PASSWD "enuHDmTo5Lq4g" /* when given password "LOGOUT" */
static time_t locked_at;

struct appres_t {
    Pixel bg;
    Pixel fg;
    XFontStruct *font;
    Boolean ignore_passwd;
    Boolean do_reverse;
    Boolean accept_root;
    char *text, *text_prog, *file, *logoutPasswd;
    Boolean no_screensaver;
    Boolean destroytickets;
} appres;

static XtResource resources[] = {
    { XtNbackground, XtCBackground, XtRPixel, sizeof(Pixel), 
      XtOffsetOf(struct appres_t, bg), XtRString, "black" },

    { XtNforeground, XtCForeground, XtRPixel, sizeof(Pixel), 
      XtOffsetOf(struct appres_t, fg), XtRString, "white" },

    { XtNfont, XtCFont, XtRFontStruct, sizeof (XFontStruct *),
      XtOffsetOf(struct appres_t, font), 
      XtRString, "-*-new century schoolbook-*-*-*-18-*" },

    { "ignorePasswd", "IgnorePasswd", XtRBoolean, sizeof(Boolean),
      XtOffsetOf(struct appres_t,ignore_passwd),XtRImmediate,(XtPointer)False },

    { "acceptRootPasswd", "AcceptRootPasswd", XtRBoolean, sizeof(Boolean),
      XtOffsetOf(struct appres_t, accept_root), XtRImmediate, (XtPointer)True },

    { "text", "Text", XtRString, sizeof(String),
      XtOffsetOf(struct appres_t, text), XtRString, "I'm out running around." },
    
    { "program", "Program", XtRString, sizeof(String),
      XtOffsetOf(struct appres_t, text_prog), XtRImmediate, NULL },

    { "file", "File", XtRString, sizeof(String),
      XtOffsetOf(struct appres_t,file), XtRImmediate, NULL },

    { "logoutPasswd", "logoutPasswd", XtRString, sizeof(String),
      XtOffsetOf(struct appres_t, logoutPasswd), XtRString, LOGOUT_PASSWD },
    
    { "noScreenSaver", "NoScreenSaver", XtRBoolean, sizeof(Boolean),
      XtOffsetOf(struct appres_t,no_screensaver), XtRImmediate, (XtPointer)True },

    { "destroyTickets", "DestroyTickets", XtRBoolean, sizeof(Boolean),
      XtOffsetOf(struct appres_t,destroytickets), XtRImmediate, (XtPointer)True },
};

static XrmOptionDescRec options[] = {
    { "-fg", ".foreground", XrmoptionSepArg, NULL }, 
    { "-foreground", ".foreground", XrmoptionSepArg, NULL }, 
    { "-fn", ".font", XrmoptionSepArg, NULL }, 
    { "-font", ".font", XrmoptionSepArg, NULL }, 
    { "-ip", ".ignorePasswd", XrmoptionNoArg, "True" },
    { "-noip", ".ignorePasswd", XrmoptionNoArg, "False" },
    { "-ar",  ".acceptRootPasswd", XrmoptionNoArg, "True" },
    { "-noar", ".acceptRootPasswd", XrmoptionNoArg, "False" },
    { "-nonoscreensaver", ".noScreenSaver", XrmoptionNoArg, "False" },
    { "-nodestroytickets", ".destroyTickets", XrmoptionNoArg, "False" },
};

static char*
get_words(void)
{
    FILE *pp = NULL;
    static char buf[512];
    long n;

    if (appres.text_prog) {
	pp = popen(appres.text_prog, "r");
	if (!pp) {
	    warn("popen %s", appres.text_prog);
	    return appres.text;
	}
	n = fread(buf, 1, sizeof(buf) - 1, pp);
	buf[n] = 0;
	pclose(pp);
	return buf;
    }
    if (appres.file) {
	pp = fopen(appres.file, "r");
	if (!pp) {
	    warn("fopen %s", appres.file);
	    return appres.text;
	}
	n = fread(buf, 1, sizeof(buf) - 1, pp);
	buf[n] = 0;
	fclose(pp);
	return buf;
    }

    return appres.text;
}

static void
usage(void)
{
    fprintf(stderr, "usage: %s [options] [message]\n", getprogname());
    fprintf(stderr, "-fg color     foreground color\n");
    fprintf(stderr, "-bg color     background color\n");
    fprintf(stderr, "-rv           reverse foreground/background colors\n");
    fprintf(stderr, "-nrv          no reverse video\n");
    fprintf(stderr, "-ip           ignore passwd\n");
    fprintf(stderr, "-nip          don't ignore passwd\n");
    fprintf(stderr, "-ar           accept root's passwd to unlock\n");
    fprintf(stderr, "-nar          don't accept root's passwd\n");
    fprintf(stderr, "-f [file]     message is read from file or ~/.msgfile\n");
    fprintf(stderr, "-prog program  text is gotten from executing `program'\n");
    fprintf(stderr, "-nodestroytickets keep kerberos tickets\n");
    exit(1);
}

static void
init_words (int argc, char **argv)
{
    int i = 0;

    while(argv[i]) {
	if(strcmp(argv[i], "-p") == 0
	   || strcmp(argv[i], "-prog") == 0) {
	    i++;
	    if(argv[i]) {
		appres.text_prog = argv[i];
		i++;
	    } else {
		warnx ("-p requires an argument");
		usage();
	    }
	} else if(strcmp(argv[i], "-f") == 0) {
	    i++;
	    if(argv[i]) {
		appres.file = argv[i];
		i++;
	    } else {
		asprintf (&appres.file,
			  "%s/.msgfile", getenv("HOME"));
		if (appres.file == NULL)
		    errx (1, "cannot allocate memory for message");
	    }
	} else {
	    int j;
	    int len = 1;
	    for(j = i; argv[j]; j++)
		len += strlen(argv[j]) + 1;
	    appres.text = malloc(len);
	    if (appres.text == NULL)
		errx (1, "cannot allocate memory for message");
	    appres.text[0] = 0;
	    for(; i < j; i++){
		strlcat(appres.text, argv[i], len);
		strlcat(appres.text, " ", len);
	    }
	}
    }
}

static void
ScreenSaver(int save)
{
    static int timeout, interval, prefer_blank, allow_exp;
    if(!appres.no_screensaver){
	if (save) {
	    XGetScreenSaver(dpy, &timeout, &interval, 
			    &prefer_blank, &allow_exp);
	    XSetScreenSaver(dpy, 0, interval, prefer_blank, allow_exp);
	} else
	    /* restore state */
	    XSetScreenSaver(dpy, timeout, interval, prefer_blank, allow_exp);
    }
}

/* Forward decls necessary */
static void talk(int force_erase);
static unsigned long look(void);

static int
zrefresh(void)
{
  switch (fork()) {
  case -1:
      warn ("zrefresh: fork");
      return -1;
  case 0:
      /* Child */
      execlp("zrefresh", "zrefresh", 0);
      execl(BINDIR "/zrefresh", "zrefresh", 0);
      return -1;
  default:
      /* Parent */
      break;
  }
  return 0;
}

static void
leave(void)
{
    XUngrabPointer(dpy, CurrentTime);
    XUngrabKeyboard(dpy, CurrentTime);
    ScreenSaver(0);
    XCloseDisplay(dpy);
    zrefresh();
    exit(0);
}

static void
walk(int dir)
{
    int incr = 0;
    static int lastdir;
    static int up = 1;
    static Pixmap frame;

    XSetForeground(dpy, gc, White);
    XSetBackground(dpy, gc, Black);
    if (dir & (LEFT|RIGHT)) { /* left/right movement (mabye up/down too) */
	up = -up; /* bouncing effect (even if hit a wall) */
	if (dir & LEFT) {
	    incr = X_INCR;
	    frame = (up < 0) ? left0 : left1;
	} else {
	    incr = -X_INCR;
	    frame = (up < 0) ? right0 : right1;
	}
	if ((lastdir == FRONT || lastdir == DOWN) && dir & UP) {
	    /* workaround silly bug that leaves screen dust when
	     * guy is facing forward or down and moves up-left/right.
	     */
	    XCopyPlane(dpy, frame, XtWindow(widget), gc, 0, 0, 64,64, x, y, 1L);
	    XFlush(dpy);
	}
	/* note that maybe neither UP nor DOWN is set! */
	if (dir & UP && y > Y_INCR)
	    y -= Y_INCR;
	else if (dir & DOWN && y < (int)Height - 64)
	    y += Y_INCR;
    }
    /* Explicit up/down movement only (no left/right) */
    else if (dir == UP)
	XCopyPlane(dpy, front, XtWindow(widget), gc,
	    0,0, 64,64, x, y -= Y_INCR, 1L);
    else if (dir == DOWN)
	XCopyPlane(dpy, down, XtWindow(widget), gc,
	    0,0, 64,64, x, y += Y_INCR, 1L);
    else if (dir == FRONT && frame != front) {
	if (up > 0)
	    up = -up;
	if (lastdir & LEFT)
	    frame = left_front;
	else if (lastdir & RIGHT)
	    frame = right_front;
	else
	    frame = front;
	XCopyPlane(dpy, frame, XtWindow(widget), gc, 0, 0, 64,64, x, y, 1L);
    }
    if (dir & LEFT)
	while(--incr >= 0) {
	    XCopyPlane(dpy, frame, XtWindow(widget), gc,
		0,0, 64,64, --x, y+up, 1L);
	    XFlush(dpy);
	}
    else if (dir & RIGHT)
	while(++incr <= 0) {
	    XCopyPlane(dpy, frame, XtWindow(widget), gc,
		0,0, 64,64, ++x, y+up, 1L);
	    XFlush(dpy);
	}
    lastdir = dir;
}

static int
think(void)
{
    if (rand() & 1)
	walk(FRONT);
    if (rand() & 1) {
	words = get_words();
	return 1;
    }
    return 0;
}

static void
move(XtPointer _p, XtIntervalId *_id)
{
    static int length, dir;

    if (!length) {
	int tries = 0;
	dir = 0;
	if ((rand() & 1) && think()) {
	    talk(0); /* sets timeout to itself */
	    return;
	}
	if (!(rand() % 3) && (interval = look())) {
	    timeout_id = XtAppAddTimeOut(app, interval, move, NULL);
	    return;
	}
	interval = 20 + rand() % 100;
	do  {
	    if (!tries)
		length = Width/100 + rand() % 90, tries = 8;
	    else
		tries--;
	    switch (rand() % 8) {
		case 0:
		    if (x - X_INCR*length >= 5)
			dir = LEFT;
		case 1:
		    if (x + X_INCR*length <= (int)Width - 70)
			dir = RIGHT;
		case 2:
		    if (y - (Y_INCR*length) >= 5)
			dir = UP, interval = 40;
		case 3:
		    if (y + Y_INCR*length <= (int)Height - 70)
			dir = DOWN, interval = 20;
		case 4:
		    if (x - X_INCR*length >= 5 && y - (Y_INCR*length) >= 5)
			dir = (LEFT|UP);
		case 5:
		    if (x + X_INCR * length <= (int)Width - 70 &&
			y-Y_INCR * length >= 5)
			dir = (RIGHT|UP);
		case 6:
		    if (x - X_INCR * length >= 5 &&
			y + Y_INCR * length <= (int)Height - 70)
			dir = (LEFT|DOWN);
		case 7:
		    if (x + X_INCR*length <= (int)Width - 70 &&
			y + Y_INCR*length <= (int)Height - 70)
			dir = (RIGHT|DOWN);
	    }
	} while (!dir);
    }
    walk(dir);
    --length;
    timeout_id = XtAppAddTimeOut(app, interval, move, NULL);
}

static void
post_prompt_box(Window window)
{
    int width = (Width / 3);
    int height = font_height(font) * 6;
    int box_x, box_y;

    /* make sure the entire nose icon fits in the box */
    if (height < 100)
	height = 100;

    if(width < 105 + font->max_bounds.width*STRING_LENGTH)
	width = 105 + font->max_bounds.width*STRING_LENGTH;
    box_x = (Width - width) / 2;
    time_x = prompt_x = box_x + 105;

    time_y = prompt_y = Height / 2;
    box_y = prompt_y - 3 * font_height(font);

    /* erase current guy -- text message may still exist */
    XSetForeground(dpy, gc, Black);
    XFillRectangle(dpy, window, gc, x, y, 64, 64);
    talk(1); /* forcefully erase message if one is being displayed */
    /* Clear area in middle of screen for prompt box */
    XSetForeground(dpy, gc, White);
    XFillRectangle(dpy, window, gc, box_x, box_y, width, height);

    /* make a box that's 5 pixels thick. Then add a thin box inside it */
    XSetForeground(dpy, gc, Black);
    XSetLineAttributes(dpy, gc, 5, 0, 0, 0);
    XDrawRectangle(dpy, window, gc, box_x+5, box_y+5, width-10, height-10);
    XSetLineAttributes(dpy, gc, 0, 0, 0, 0);
    XDrawRectangle(dpy, window, gc, box_x+12, box_y+12, width-23, height-23);

    XDrawString(dpy, window, gc,
		prompt_x, prompt_y-font_height(font), 
		userprompt, strlen(userprompt));
    XDrawString(dpy, window, gc, prompt_x, prompt_y, PROMPT, strlen(PROMPT));
    /* set background for copyplane and DrawImageString; need reverse video */
    XSetBackground(dpy, gc, White);
    XCopyPlane(dpy, right0, window, gc, 0,0, 64,64,
	       box_x + 20, box_y + (height - 64)/2, 1L);
    prompt_x += XTextWidth(font, PROMPT, strlen(PROMPT));
    time_y += 2*font_height(font);
}

static void
RaiseWindow(Widget w, XEvent *ev, String *s, Cardinal *n)
{
  Widget x;
  if(!XtIsRealized(w))
    return;
  x = XtParent(w);
  XRaiseWindow(dpy, XtWindow(x));
}


static void
ClearWindow(Widget w, XEvent *_event, String *_s, Cardinal *_n)
{
    XExposeEvent *event = (XExposeEvent *)_event;
    if (!XtIsRealized(w))
	return;
    XClearArea(dpy, XtWindow(w), event->x, event->y, 
	       event->width, event->height, False);
    if (state == GET_PASSWD)
	post_prompt_box(XtWindow(w));
    if (timeout_id == 0 && event->count == 0) {
	timeout_id = XtAppAddTimeOut(app, 1000L, move, NULL);
	/* first grab the input focus */
	XSetInputFocus(dpy, XtWindow(w), RevertToPointerRoot, CurrentTime);
	/* now grab the pointer and keyboard and contrain to this window */
	XGrabPointer(dpy, XtWindow(w), TRUE, 0, GrabModeAsync,
	     GrabModeAsync, XtWindow(w), None, CurrentTime);
    }
}

static void
countdown(XtPointer _t, XtIntervalId *_d)
{
    int *timeout = (int *)_t;
    char buf[128];
    time_t seconds;

    if (--(*timeout) < 0) {
	XExposeEvent event;
	XtRemoveTimeOut(timeout_id);
	state = IS_MOVING;
	event.x = event.y = 0;
	event.width = Width, event.height = Height;
	ClearWindow(widget, (XEvent *)&event, 0, 0);
	timeout_id = XtAppAddTimeOut(app, 200L, move, NULL);
	return;
    }
    seconds = time(0) - locked_at;
    if (seconds >= 3600)
      snprintf(buf, sizeof(buf),
	       "Locked for %d:%02d:%02d    ",
	       (int)seconds/3600, (int)seconds/60%60, (int)seconds%60);
    else
      snprintf(buf, sizeof(buf),
	       "Locked for %2d:%02d    ",
	       (int)seconds/60, (int)seconds%60);
      
    XDrawImageString(dpy, XtWindow(widget), gc,
	time_x, time_y, buf, strlen(buf));
    XtAppAddTimeOut(app, 1000L, countdown, timeout);
    return;
}

#ifdef KRB5
static int
verify_krb5(const char *password)
{
    krb5_error_code ret;
    krb5_ccache id;
    
    krb5_cc_default(context, &id);
    ret = krb5_verify_user(context,
			   client, 
			   id,
			   password, 
			   0,
			   NULL);
    if (ret == 0){
#ifdef KRB4
	if (krb5_config_get_bool(context, NULL,
				 "libdefaults",
				 "krb4_get_tickets",
				 NULL)) {
	    CREDENTIALS c;
	    krb5_creds mcred, cred;
	    char krb4tkfile[MAXPATHLEN];

	    krb5_make_principal(context, &mcred.server,
				client->realm,
				"krbtgt",
				client->realm,
				NULL);
	    ret = krb5_cc_retrieve_cred(context, id, 0, &mcred, &cred);
	    if(ret == 0) {
		ret = krb524_convert_creds_kdc(context, id, &cred, &c);
		if(ret == 0) 
		    tf_setup(&c, c.pname, c.pinst);
		memset(&c, 0, sizeof(c));
		krb5_free_creds_contents(context, &cred);
	    }
	    krb5_free_principal(context, mcred.server);
	}
	if (k_hasafs())
	    krb5_afslog(context, id, NULL, NULL);
#endif
	return 0;
    }
    if (ret != KRB5KRB_AP_ERR_MODIFIED)
	krb5_warn(context, ret, "verify_krb5");
    
    return -1;
}
#endif

static int
verify(char *password)
{
    int ret;

    /*
     * First try with root password, if allowed.
     */
    if (   appres.accept_root
	&& strcmp(crypt(password, root_cpass), root_cpass) == 0)
      return 0;

    /*
     * Password that log out user
     */
    if (getuid() != 0 &&
	geteuid() != 0 &&
	(time(0) - locked_at) > ALLOW_LOGOUT &&
	strcmp(crypt(password, appres.logoutPasswd), appres.logoutPasswd) == 0)
	    {
		signal(SIGHUP, SIG_IGN);
		kill(-1, SIGHUP);
		sleep(5);
		/* If the X-server shut down then so will we, else
		 * continue */
		signal(SIGHUP, SIG_DFL);
	    }

    /*
     * Try copy of users password.
     */
    if (strcmp(crypt(password, user_cpass), user_cpass) == 0)
      return 0;

    /*
     * Try to verify as user in case password change.
     */
    if (unix_verify_user(login, password) == 0)
	return 0;

#ifdef KRB5
    /*
     * Try to verify as user with kerberos 5.
     */
    if(verify_krb5(password) == 0)
	return 0;
#endif

#ifdef KRB4
    /*
     * Try to verify as user with kerberos 4.
     */
    ret = krb_verify_user(name, inst, realm, password,
			  KRB_VERIFY_NOT_SECURE, NULL);
    if (ret == KSUCCESS){
	if (k_hasafs())
	    krb_afslog(NULL, NULL);
	return 0;
    }
    if (ret != INTK_BADPW)
	warnx ("warning: %s",
	       (ret < 0) ? strerror(ret) : krb_get_err_text(ret));
#endif
    
    return -1;
}


static void
GetPasswd(Widget w, XEvent *_event, String *_s, Cardinal *_n)
{
    XKeyEvent *event = (XKeyEvent *)_event;
    static char passwd[MAX_PASSWD_LENGTH];
    static int cnt;
    static int is_ctrl = XNLOCK_NOCTRL;
    char c;
    KeySym keysym;
    int echolen;
    int old_state = state;

    if (event->type == ButtonPress) {
	x = event->x, y = event->y;
	return;
    }
    if (state == IS_MOVING) {
	/* guy is running around--change to post prompt box. */
	XtRemoveTimeOut(timeout_id);
	state = GET_PASSWD;
	if (appres.ignore_passwd || !strlen(user_cpass))
	    leave();
	post_prompt_box(XtWindow(w));
	cnt = 0;
	time_left = 30;
	countdown((XtPointer)&time_left, 0);
    }
    if (event->type == KeyRelease) {
      keysym = XLookupKeysym(event, 0);
      if (keysym == XK_Control_L || keysym == XK_Control_R) {
	is_ctrl = XNLOCK_NOCTRL;
      }
    }
    if (event->type != KeyPress)
	return;

    time_left = 30;
    
    keysym = XLookupKeysym(event, 0);
    if (keysym == XK_Control_L || keysym == XK_Control_R) {
      is_ctrl = XNLOCK_CTRL;
      return;
    }
    if (!XLookupString(event, &c, 1, &keysym, 0))
	return;
    if (keysym == XK_Return || keysym == XK_Linefeed) {
	passwd[cnt] = 0;
	if(old_state == IS_MOVING)
	    return;
	XtRemoveTimeOut(timeout_id);

	if(verify(passwd) == 0)
	    leave();

	cnt = 0;

	XDrawImageString(dpy, XtWindow(widget), gc,
	    time_x, time_y, FAIL_MSG, strlen(FAIL_MSG));
	time_left = 0;
	timeout_id = XtAppAddTimeOut(app, 2000L, countdown, &time_left);
	return;
    }
    if (keysym == XK_BackSpace || keysym == XK_Delete || keysym == XK_Left) {
	if (cnt)
	    passwd[cnt--] = ' ';
    } else if (keysym == XK_u && is_ctrl == XNLOCK_CTRL) {
      while (cnt) {
	passwd[cnt--] = ' ';
	echolen = min(cnt, STRING_LENGTH);
	XDrawImageString(dpy, XtWindow(w), gc,
		    prompt_x, prompt_y, STRING, echolen);
	XDrawImageString(dpy, XtWindow(w), gc,
			 prompt_x + XTextWidth(font, STRING, echolen),
			 prompt_y, SPACE_STRING, STRING_LENGTH - echolen + 1);
      }
    } else if (isprint(c)) {
	if ((cnt + 1) >= MAX_PASSWD_LENGTH)
	    XBell(dpy, 50);
	else
	    passwd[cnt++] = c;
    } else
	return;
    echolen = min(cnt, STRING_LENGTH);
    XDrawImageString(dpy, XtWindow(w), gc,
	prompt_x, prompt_y, STRING, echolen);
    XDrawImageString(dpy, XtWindow(w), gc,
	prompt_x + XTextWidth(font, STRING, echolen),
	prompt_y, SPACE_STRING, STRING_LENGTH - echolen +1);
}

#include "nose.0.left"
#include "nose.1.left"
#include "nose.0.right"
#include "nose.1.right"
#include "nose.left.front"
#include "nose.right.front"
#include "nose.front"
#include "nose.down"

static void
init_images(void)
{
    static Pixmap *images[] = {
	&left0, &left1, &right0, &right1,
	&left_front, &right_front, &front, &down 
    };
    static unsigned char *bits[] = {
	nose_0_left_bits, nose_1_left_bits, nose_0_right_bits,
	nose_1_right_bits, nose_left_front_bits, nose_right_front_bits,
	nose_front_bits, nose_down_bits
    };
    int i;

    for (i = 0; i < XtNumber(images); i++)
	if (!(*images[i] =
		XCreatePixmapFromBitmapData(dpy, DefaultRootWindow(dpy),
		    (char*)(bits[i]), 64, 64, 1, 0, 1)))
	    XtError("Can't load nose images");
}

static void
talk(int force_erase)
{
    int width = 0, height, Z, total = 0;
    static int X, Y, talking;
    static struct { int x, y, width, height; } s_rect;
    char *p, *p2;
    char buf[BUFSIZ], args[MAXLINES][256];

    /* clear what we've written */
    if (talking || force_erase) {
	if (!talking)
	    return;
	if (talking == 2) {
	    XSetForeground(dpy, gc, Black);
	    XDrawString(dpy, XtWindow(widget), gc, X, Y, words, strlen(words));
	} else if (talking == 1) {
	    XSetForeground(dpy, gc, Black);
	    XFillRectangle(dpy, XtWindow(widget), gc, s_rect.x-5, s_rect.y-5,
			   s_rect.width+10, s_rect.height+10);
	}
	talking = 0;
	if (!force_erase)
	    timeout_id = XtAppAddTimeOut(app, 40L,
					 (XtTimerCallbackProc)move,
					 NULL);
	return;
    }
    XSetForeground(dpy, gc, White);
    talking = 1;
    walk(FRONT);
    strlcpy (buf, words, sizeof(buf));
    p = buf;

    /* possibly avoid a lot of work here
     * if no CR or only one, then just print the line
     */
    if (!(p2 = strchr(p, '\n')) || !p2[1]) {
	int w;

	if (p2)
	    *p2 = 0;
	w = XTextWidth(font, words, strlen(words));
	X = x + 32 - w/2;
	Y = y - 5 - font_height(font);
	/* give us a nice 5 pixel margin */
	if (X < 5)
	    X = 5;
	else if (X + w + 15 > (int)Width + 5)
	    X = Width - w - 5;
	if (Y < 5)
	    Y = y + 64 + 5 + font_height(font);
	XDrawString(dpy, XtWindow(widget), gc, X, Y, words, strlen(words));
	timeout_id = XtAppAddTimeOut(app, 5000L, (XtTimerCallbackProc)talk, 
				     NULL);
	talking++;
	return;
    }

    /* p2 now points to the first '\n' */
    for (height = 0; p; height++) {
	int w;
	*p2 = 0;
	if ((w = XTextWidth(font, p, p2 - p)) > width)
	    width = w;
	total += p2 - p; /* total chars; count to determine reading time */
	strlcpy(args[height], p, sizeof(args[height]));
	if (height == MAXLINES - 1) {
	    puts("Message too long!");
	    break;
	}
	p = p2+1;
	if (!(p2 = strchr(p, '\n')))
	    break;
    }
    height++;

    /* Figure out the height and width in pixels (height, width) extend
     * the new box by 15 pixels on the sides (30 total) top and bottom.
     */
    s_rect.width = width + 30;
    s_rect.height = height * font_height(font) + 30;
    if (x - s_rect.width - 10 < 5)
	s_rect.x = 5;
    else
	if ((s_rect.x = x+32-(s_rect.width+15)/2)
					 + s_rect.width+15 > (int)Width-5)
	    s_rect.x = Width - 15 - s_rect.width;
    if (y - s_rect.height - 10 < 5)
	s_rect.y = y + 64 + 5;
    else
	s_rect.y = y - 5 - s_rect.height;

    XSetForeground(dpy, gc, White);
    XFillRectangle(dpy, XtWindow(widget), gc,
       s_rect.x-5, s_rect.y-5, s_rect.width+10, s_rect.height+10);

    /* make a box that's 5 pixels thick. Then add a thin box inside it */
    XSetForeground(dpy, gc, Black);
    XSetLineAttributes(dpy, gc, 5, 0, 0, 0);
    XDrawRectangle(dpy, XtWindow(widget), gc,
		   s_rect.x, s_rect.y, s_rect.width-1, s_rect.height-1);
    XSetLineAttributes(dpy, gc, 0, 0, 0, 0);
    XDrawRectangle(dpy, XtWindow(widget), gc,
		   s_rect.x + 7, s_rect.y + 7, s_rect.width - 15, 
		   s_rect.height - 15);

    X = 15;
    Y = 15 + font_height(font);

    /* now print each string in reverse order (start at bottom of box) */
    for (Z = 0; Z < height; Z++) {
	XDrawString(dpy, XtWindow(widget), gc, s_rect.x+X, s_rect.y+Y,
	    args[Z], strlen(args[Z]));
	Y += font_height(font);
    }
    timeout_id = XtAppAddTimeOut(app, (total/15) * 1000, 
				 (XtTimerCallbackProc)talk, NULL);
}

static unsigned long
look(void)
{
    XSetForeground(dpy, gc, White);
    XSetBackground(dpy, gc, Black);
    if (rand() % 3) {
	XCopyPlane(dpy, (rand() & 1)? down : front, XtWindow(widget), gc,
	    0, 0, 64,64, x, y, 1L);
	return 1000L;
    }
    if (!(rand() % 5))
	return 0;
    if (rand() % 3) {
	XCopyPlane(dpy, (rand() & 1)? left_front : right_front,
	    XtWindow(widget), gc, 0, 0, 64,64, x, y, 1L);
	return 1000L;
    }
    if (!(rand() % 5))
	return 0;
    XCopyPlane(dpy, (rand() & 1)? left0 : right0, XtWindow(widget), gc,
	0, 0, 64,64, x, y, 1L);
    return 1000L;
}

int
main (int argc, char **argv)
{
    int i;
    Widget override;
    XGCValues gcvalues;

    setprogname (argv[0]);

    /*
     * Must be setuid root to read /etc/shadow, copy encrypted
     * passwords here and then switch to sane uid.
     */
    {
      struct passwd *pw;
      uid_t uid = getuid();
      if (!(pw = k_getpwuid(0)))
	errx (1, "can't get root's passwd!");
      strlcpy(root_cpass, pw->pw_passwd, sizeof(root_cpass));

      if (!(pw = k_getpwuid(uid)))
	errx (1, "Can't get your password entry!");
      strlcpy(user_cpass, pw->pw_passwd, sizeof(user_cpass));
      setuid(uid);
      if (uid != 0 && setuid(0) != -1) {
	fprintf(stderr, "Failed to drop privileges!\n");
	exit(1);
      }
      /* Now we're no longer running setuid root. */
      strlcpy(login, pw->pw_name, sizeof(login));
    }

    srand(getpid());
    for (i = 0; i < STRING_LENGTH; i++)
	STRING[i] = ((unsigned long)rand() % ('~' - ' ')) + ' ';

    locked_at = time(0);

    snprintf(userprompt, sizeof(userprompt), "User: %s", login);
#ifdef KRB4
    krb_get_default_principal(name, inst, realm);
    snprintf(userprompt, sizeof(userprompt), "User: %s", 
	     krb_unparse_name_long(name, inst, realm));
#endif
#ifdef KRB5
    {
	krb5_error_code ret;
	char *str;

	ret = krb5_init_context(&context);
	if (ret)
	    errx (1, "krb5_init_context failed: %d", ret);
	krb5_get_default_principal(context, &client);
	krb5_unparse_name(context, client, &str);
	snprintf(userprompt, sizeof(userprompt), "User: %s", str);
	free(str);
    }
#endif

    override = XtVaAppInitialize(&app, "XNlock", options, XtNumber(options),
				 (Cardinal*)&argc, argv, NULL, 
				 XtNoverrideRedirect, True, 
				 NULL);
    
    XtVaGetApplicationResources(override,(XtPointer)&appres,
				resources,XtNumber(resources),
				NULL);
    /* the background is black and the little guy is white */
    Black = appres.bg;
    White = appres.fg;

    if (appres.destroytickets) {
#ifdef KRB4
	int fd;

        dest_tkt();		/* Nuke old ticket file */
				/* but keep a place holder */
	fd = open (TKT_FILE, O_WRONLY | O_CREAT | O_EXCL, 0600);
	if (fd >= 0)
	    close (fd);
#endif
    }

    dpy = XtDisplay(override);
    
    if (dpy == 0)
      errx (1, "Error: Can't open display");

    Width = DisplayWidth(dpy, DefaultScreen(dpy)) + 2;
    Height = DisplayHeight(dpy, DefaultScreen(dpy)) + 2;
    
    for(i = 0; i < ScreenCount(dpy); i++){
	Widget shell, core;

	struct xxx{
	    Pixel bg;
	}res;
	
	XtResource Res[] = {
	    { XtNbackground, XtCBackground, XtRPixel, sizeof(Pixel),
	      XtOffsetOf(struct xxx, bg), XtRString, "black" }
	};

	if(i == DefaultScreen(dpy))
	    continue;
      
	shell = XtVaAppCreateShell(NULL,NULL, applicationShellWidgetClass, dpy, 
				   XtNscreen, ScreenOfDisplay(dpy, i), 
				   XtNoverrideRedirect, True, 
				   XtNx, -1, 
				   XtNy, -1,
				   NULL);
      
	XtVaGetApplicationResources(shell, (XtPointer)&res, 
				    Res, XtNumber(Res),
				    NULL);

	core = XtVaCreateManagedWidget("_foo", widgetClass, shell,
				       XtNwidth, DisplayWidth(dpy, i),
				       XtNheight, DisplayHeight(dpy, i),
				       XtNbackground, res.bg, 
				       NULL);
	XtRealizeWidget(shell);
    }

    widget = XtVaCreateManagedWidget("_foo", widgetClass, override,
				     XtNwidth,	Width,
				     XtNheight,	Height,
				     XtNbackground, Black, 
				     NULL);

    init_words(--argc, ++argv);
    init_images();

    gcvalues.foreground = Black;
    gcvalues.background = White;


    font = appres.font;
    gcvalues.font = font->fid;
    gcvalues.graphics_exposures = False;
    gc = XCreateGC(dpy, DefaultRootWindow(dpy),
	GCForeground | GCBackground | GCGraphicsExposures | GCFont,
	&gcvalues);

    x = Width / 2;
    y = Height / 2;
    srand (time(0));
    state = IS_MOVING;

    {
	static XtActionsRec actions[] = {
	    { "ClearWindow",	ClearWindow  },
	    { "GetPasswd",	GetPasswd    },
	    { "RaiseWindow", 	RaiseWindow  },
	};
	XtAppAddActions(app, actions, XtNumber(actions));
	XtOverrideTranslations(widget,
	       XtParseTranslationTable(
				       "<Expose>:	ClearWindow()	\n"
				       "<BtnDown>:	GetPasswd()	\n"
				       "<Visible>: RaiseWindow() \n"
				       "<KeyRelease>:  GetPasswd()     \n"
				       "<KeyPress>:	GetPasswd()"));
    }

    XtRealizeWidget(override);
    if((i = XGrabPointer(dpy, XtWindow(widget), True, 0, GrabModeAsync,
			 GrabModeAsync, XtWindow(widget), 
			 None, CurrentTime)) != 0) 
	errx(1, "Failed to grab pointer (%d)", i);
	
    if((i = XGrabKeyboard(dpy, XtWindow(widget), True, GrabModeAsync,
			  GrabModeAsync, CurrentTime)) != 0)
	errx(1, "Failed to grab keyboard (%d)", i);
    ScreenSaver(1);
    XtAppMainLoop(app);
    exit(0);
}

