/*-
 * Copyright (c) 2012 Simon W. Moore
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract (FA8750-10-C-0237)
 * ("CTSRD"), as part of the DARPA CRASH research programme.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/endian.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <libutil.h>
#include <login_cap.h>
#include <poll.h>
#include <pwd.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include <terasic_mtl.h>
#include <imagebox.h>

static void start_keyboard_shell(void);

static pid_t browser_pid;
static pid_t kbd_pid;

// send keyboard output to stdout by default
static int kbdfd = 0;

u_int32_t *fb_buf;

static void
handle_sigchld(int sig __unused)
{
  pid_t pid;

  if ((pid = wait4(-1, NULL, 0, NULL)) < 1)
    err(1, "wait4");
  else {
    if (pid == browser_pid)
      browser_pid = 0;
    else if (pid == kbd_pid) {
      kbd_pid = 0;
      kbdfd = -1;
    } else
      warnx("unexpected pid from wait4(): %d", pid);
  }
}

void
pen_drawing_clear_screen(void)
{
  int x0,y0;
  unsigned int blue  = fb_colour(0x00,0x00,0xaa);
  unsigned int white = fb_colour(0xff,0xff,0xff);
  unsigned int grey  = fb_colour(0xcc,0xcc,0xcc);
  fb_fade2off();
  fb_fill(white);
  for(x0=5; x0<fb_width; x0+=10)
    plot_line(x0,5, x0,fb_height-6, grey);
  for(y0=5; y0<fb_height; y0+=10)
    plot_line(5,y0, fb_width-6,y0, grey);

  plot_line( 5, 5,   5,35, blue);
  plot_line( 5,35,  35,35, blue);
  plot_line(35,35,  35, 5, blue);
  plot_line(35, 5,   5, 5, blue);

  plot_line( 5, 5,  35,35, blue);
  plot_line( 5,35,  35, 5, blue);

  fb_fade2on();
}


void
pen_drawing(void)
{
  unsigned int inkcol = fb_colour(0x00,0x00,0xaa);
  int px0=-1;
  int py0=-1;
  int j;
  pen_drawing_clear_screen();

  // wait for touch to finish from image selector
  multitouch_release_event();

  // while no pinch gesture, draw...
  while(!((touch_count==2) && (touch_gesture==0x49))) {
    // try to filter out short lived touch releases
    multitouch_filter();

    if((touch_count<1) || (touch_count>2))
      px0=py0=-1;
    else if(touch_count==1) {
      // if the top left corner is touched, clear the screen
      if((touch_x0>=0) && (touch_x0<=40) &&
	 (touch_y0>=0) && (touch_y0<=40)) {
	pen_drawing_clear_screen();
	multitouch_release_event();
	px0=py0=-1;
      } else if((touch_x0>=0) && (touch_y0>=0)) {
	if((px0<0) || (py0<0))
	  fb_putpixel(touch_x0,touch_y0,inkcol);
	else
	  plot_line(px0,py0,touch_x0,touch_y0,inkcol);
	px0=touch_x0;
	py0=touch_y0;
      }
    }
  }
}




/*****************************************************************************
 * On screen keyboard
 *****************************************************************************/

static const int keyboard_numimg = 4;
static const int keyboard_height = 240;
static const int keyboard_width  = 800;
static u_int32_t* keyboard_imgs[4];

void
keyboard_init(void)
{
  int j;
  // allocate memory for images
  for(j=0; j<keyboard_numimg; j++)
    keyboard_imgs[j] = (u_int32_t*) malloc(sizeof(u_int32_t) * keyboard_width * keyboard_height);

  read_png_file("/usr/share/images/keyboardA.png", keyboard_imgs[0], keyboard_width, keyboard_height);
  read_png_file("/usr/share/images/keyboardS.png", keyboard_imgs[1], keyboard_width, keyboard_height);
  read_png_file("/usr/share/images/keyboardN.png", keyboard_imgs[2], keyboard_width, keyboard_height);
  read_png_file("/usr/share/images/keyboardC.png", keyboard_imgs[3], keyboard_width, keyboard_height);

  start_keyboard_shell();
}

static void
writeall(int fd, const char *buf, size_t len)
{
  int wlen = 0, n;

  while (wlen != len) {
    n = write(fd, buf + wlen, len - wlen);
    if (n < 0) {
      syslog(LOG_ALERT, "write failed: %s", strerror(errno));
      err(1, "write");
    }
    wlen += n;
  }
}

static void
start_keyboard_shell(void)
{
  int pmaster, pslave;
  login_cap_t *lcap = NULL;
  struct passwd *pwd = NULL;

  if (openpty(&pmaster, &pslave, NULL, NULL, NULL) == -1)
    err(1, "openpty");
  kbd_pid = fork();
  if (kbd_pid < 0)
    err(1, "fork()");
  else if (kbd_pid > 0) {
    close(pslave);
    kbdfd = pmaster;
  } else {
    close(pmaster);
    if (login_tty(pslave) < 0) {
      syslog(LOG_ALERT, "login_tty failed in child: %s", strerror(errno));
      err(1, "tty_login");
    }

    if ((pwd = getpwuid(getuid())) == NULL)
      err(1, "getpwuid: %s", getuid());
    if ((lcap = login_getpwclass(pwd)) == NULL)
      err(1, "login_getpwclass");
    if (setusercontext(lcap, pwd, pwd->pw_uid,
	LOGIN_SETALL & ~LOGIN_SETGROUP & ~LOGIN_SETLOGIN) != 0)
      err(1, "setusercontext");

    execl("/bin/sh", "sh", NULL);
    syslog(LOG_ALERT, "exec of /bin/sh failed: %s", strerror(errno));
    err(1, "execl()");
  }
}

void
keyboard_on(void)
{
  const int touch_timeout = 100000;
  int wait_touch_release = true;
  int touch_release_timer = touch_timeout;
  char* keymap[4][4];
  int keymode = 0;
  int prev_keymode = -1;
  int keyYpos = fb_height-keyboard_height;

  const int poll_timeout = touch_timeout;
  int wait_poll_timeout = poll_timeout;

  int n;
  char *devpath, buf[1024];
  ssize_t rlen;
  struct pollfd pfd[1];

  // extra mapping codes:
  // ctrl key  = \xff -> keymode=3
  // num key   = \xfe -> keymode=2
  // shift key = \xfd -> keymode=1

  // keyboard map in 1/2 key spacing on X axis and whole key spacing on Y
  keymap[0][0] = "\t\tqqwweerrttyyuuiioopp--\x08\x08";
  keymap[0][1] = "\xff\xff aassddffgghhjjkkll;;\n\n\n";
  keymap[0][2] = "\xfd\xfd``zzxxccvvbbnnmm,,..//\\\\";
  keymap[0][3] = "\xfe\xfe\x1b\x1b              <<^^vv>>";

  // shift map:
  keymap[1][0] = "\t\tQQWWEERRTTYYUUIIOOPP++\x08\x08";
  keymap[1][1] = "\xff\xff AASSDDFFGGHHJJKKLL::\n\n\n";
  keymap[1][2] = "\xfd\xfd~~ZZXXCCVVBBNNMM<<>>??||";
  keymap[1][3] = "\xfe\xfe\x1b\x1b              <<^^vv>>";

  // numeric map:
  keymap[2][0] = "\t\t11223344556677889900--\x08\x08";
  keymap[2][1] = "\xff\xff !!@@##$$%%^^&&**(())\n\n\n";
  keymap[2][2] = "\xfd\xfd++--==__[[]]{{}}\"\"''``~~";
  keymap[2][3] = "\xfe\xfe\x1b\x1b              <<^^vv>>";

  // TODO: must be a better way to add \xff to the above without it being interpreted as \xffa
  keymap[0][1][3] = '\xff';
  keymap[1][1][3] = '\xff';
  keymap[2][1][3] = '\xff';

  if (kbdfd < 0)
    start_keyboard_shell();

  fb_fade2off();
  fb_fade2text(127);
  do {
    if((keymode<0) || (keymode>3)) {
      printf("ERROR - assertion failed - keymode out of range = %1d",keymode);
      keymode = 0;
    }
    if(keymode!=prev_keymode) {
      int j, k;
      prev_keymode = keymode;
      // display keyboard
      for(j=0; (j<(fb_width*fb_height)) && (j<fb_width*keyYpos); j++)
	fb_buf[j] = 0;
      for(k=0; (j<(fb_width*fb_height)) && (k<(fb_width*keyboard_height)); k++) {
	fb_buf[j] = keyboard_imgs[keymode][k];
	j++;
      }
      for(;(j<fb_width*fb_height); j++)
	fb_buf[j] = 0;
      fb_post(fb_buf);
    }

    multitouch_pole();
    if(touch_release_timer>0) { // wait for touch release stability
      if(touch_count==0)
	touch_release_timer--;
      else
	touch_release_timer = touch_timeout;
    } else // not waiting for a touch release
      if(touch_count==1) {
	int cx,cy;
	touch_release_timer = touch_timeout;
	cx = (touch_x0-10)/30;
	cy = (touch_y0-keyYpos)/60;
	//printf("cx=%1d, cy=%1d\n",cx,cy);
	if((cx>=0) && (cx<26) && (cy>=0) && (cy<4)) {
	  char c;
	  int ic;
	  if(keymode<3)
	    c = keymap[keymode][cy][cx];
	  else { // control key
	    c = keymap[0][cy][cx];
	    if((c>='a') && (c<='z')) // convert to control code
	      c = c - 'a' + '\x01';
	  }
	  ic = (unsigned int) c;
	  // TODO: fix sign problem
	  if(ic < 0)
	    ic += 256;
	  if(ic > 0xfc) {
	    keymode = ic - 0xfc;
	    if(keymode == prev_keymode) // allow keymode to be cancelled
	      keymode = 0;
	  }
	  if(ic < 0x80) {
	    // printf("key = \"%c\" = 0x%02x\n", c, ic);
	    writeall(kbdfd, &c, 1);
	    // cancel shift and ctrl modes after character sent
	    if((keymode==1) || (keymode==3))
	      keymode=0;
	  }
	}
      } else
	if(touch_count==2) {
	  if(touch_gesture==0x30) // swipe north - keyboard to top
	    keyYpos = 0;
	  if(touch_gesture==0x38) // swipe south - keyboard to bottom
	    keyYpos = fb_height-keyboard_height;
	  if((touch_gesture==0x3c) && (keyYpos<fb_height)) // swipe west  - keyboard off
	    keyYpos += fb_height; // put off screen
	  if((touch_gesture==0x34) && (keyYpos>=fb_height)) // swipe east  - keyboard on
	    keyYpos -= fb_height; // put on screen
	  prev_keymode = -1; // redraw keyboard
	}
    
    if (kbdfd != 0) {
      if (kbd_pid == 0)
	break;

      if (wait_poll_timeout > 0) {
	wait_poll_timeout--;
	continue;
      }
      wait_poll_timeout = poll_timeout;

      /* Check for output from the child and post it if needed */
      pfd[0].fd = kbdfd;
      pfd[0].events = POLLIN;
      n = poll(pfd, 1, 0);
      if (n == 0)
        continue;
      else if (n < 0) {
        if (errno == EINTR)
          continue;
        err(1, "poll");
      }
      if (n < 0) {
        syslog(LOG_ALERT, "poll failed with %s", strerror(errno));
	err(1, "poll");
      }
      if (pfd[0].revents & POLLIN) {
        rlen = read(pfd[0].fd, buf, sizeof(buf));
        if (rlen < 0) {
          syslog(LOG_ALERT, "read failed: %s", strerror(errno));
	  err(1, "read");
        } else if (rlen > 0)
	  writeall(0, buf, rlen);
      }
    }
  } while(!((touch_count==2) && (touch_gesture==0x49)));
  multitouch_release_event();
}


/*****************************************************************************
 * Picture viewer including PNG image loader
 *****************************************************************************/

static const int selector_nimages=5;
static u_int32_t *selector_images[5];
static char *slide_dir = NULL;
static char *slide_dir2 = NULL;
static u_int32_t *coverimage, *coverimage2;
static u_int32_t *bgimage;

#define	SEL_SLIDE_IMG 0
#define SEL_QUILL_IMG 1
#define SEL_TERM_IMG 2
#define SEL_BROWSER_IMG 3
#define SEL_SLIDE_IMG2 4

static uint32_t *
loadcover(const char *dir)
{
  DIR *dirp;
  struct dirent *entry;
  char *covername;
  uint32_t *image;
  int fd;
  uint32_t c, fcol, r;
  struct iboxstate *is;

  covername = NULL;
  if ((dirp = opendir(dir)) == NULL)
    err(1, "opendir");
  while((entry = readdir(dirp)) != NULL) {
    /* XXX: doesn't support symlinks */
    if (entry->d_type != DT_REG)
      continue;
    /* Ignore all files other than covers. */
    if (fnmatch("*-cover-*.png", entry->d_name, 0) != 0)
      continue;
    covername = entry->d_name;
    break;
  }
  if (covername == NULL)
    errx(1, "No cover found in %s", dir);

  if ((fd = openat(dirfd(dirp), covername, O_RDONLY)) == -1)
    err(1, "openat(dir, %s)", covername);
  if ((is = png_read_start(fd, fb_width, fb_height, SB_NONE)) == NULL)
    errx(1, "Failed to start PNG decode for %s", covername);
  if (png_read_finish(is) != 0)
    errx(1, "png_read_finish() failed for icons.png");

  image = malloc(sizeof(u_int32_t) * fb_width * fb_height);
  if (image == NULL)
    err(1, "malloc image]");
  fcol = (fb_width - is->width) / 2;
  fb_composite(image, fb_width, fb_height, fcol, 0,
    is->buffer, is->width, is->height);
  if (is->width != fb_width) {
    for (r = 0; r < is->height; r++) {
      for (c = 0; c < fcol; c++)
	image[c + (r * fb_width)] = image[fcol + (r * fb_width)];
      for (c = fcol + is->width; c < fb_width; c++)
	image[c + (r * fb_width)] = image[fcol + is->width - 1 + (r * fb_width)];
    }
  }
  iboxstate_free(is);
  close(fd);
  closedir(dirp);

  return (image);
}

// initialisation - load the images to view
void
pictview_init(void)
{
  int j;

  bgimage = malloc(sizeof(u_int32_t) * fb_width * fb_height);
  busy_indicator();
  read_png_file("/usr/share/images/CatSword.png",        bgimage, fb_width, fb_height);

  coverimage = loadcover(slide_dir);

  if (slide_dir2 != NULL)
    coverimage2 = loadcover(slide_dir2);
  else
    coverimage2 = NULL;

  for(j=0; j<selector_nimages; j++) {
    if (j == SEL_SLIDE_IMG)
      selector_images[j] = coverimage;
    else if (j == SEL_SLIDE_IMG2)
      selector_images[j] = coverimage2;
    else
      if ((selector_images[j] = malloc(sizeof(u_int32_t) * fb_width * fb_height)) == NULL)
        err(1, "malloc selector_images[%d]", j);
  }

  busy_indicator();
  read_png_file("/usr/share/images/Quill.png",
      selector_images[SEL_QUILL_IMG], fb_width, fb_height);
  busy_indicator();
  read_png_file("/usr/share/images/Terminal.png",
      selector_images[SEL_TERM_IMG], fb_width, fb_height);
  busy_indicator();
  read_png_file("/usr/share/images/browser-thumb.png",
     selector_images[SEL_BROWSER_IMG], fb_width, fb_height);
  busy_indicator();
}


static void
show_about(void)
{
	int fd;
	u_int32_t rombuf[5];
	u_int32_t bdate, btime, svnrev_bcd, svnrev;
	u_int32_t *image;
	int year, month, day, hour, minute, second;
	char kvbuf[256];
	char *kbuilder, *kconfig, *kdate, *kversion, *kvp;
	char *version_string;
	size_t len;
	ssize_t rlen;

	if ((fd = open("/dev/berirom", O_RDONLY)) == -1) {
		warn("Unable to open /dev/berirom");
		return;
	}
	rlen = read(fd, rombuf, sizeof(rombuf));
	close(fd);
	if (rlen != sizeof(rombuf)) {
		warnx("Unable to read from /dev/berirom");
		return;
	}
	/*
	 * bdate is the build date in BCD MMDDYYYY
	 *   i.e. 0x08302012 is August 30th, 2012
	 */
	bdate = le32toh(rombuf[0]);
	month = ((bdate >> 28) & 0xF) * 10 + ((bdate >> 24) & 0xF);
	day = ((bdate >> 20) & 0xF) * 10 + ((bdate >> 16) & 0xF);
	year = ((bdate >> 12) & 0xF) * 1000 + ((bdate >> 8) & 0xF) * 100 +
	    ((bdate >> 4) & 0xF) * 10 + (bdate & 0xF);

	/*
	 * btime is the build time in BCD 00HHMMSS
	 *   i.e. 0x00173700 is 5:37pm
	 */
	btime = le32toh(rombuf[1]);
	hour = ((btime >> 20) & 0xF) * 10 + ((btime >> 16) & 0xF);
	minute = ((btime >> 12) & 0xF) * 10 + ((btime >> 8) & 0xF);
	second = ((btime >> 4) & 0xF) * 10 + (btime & 0xF);

	svnrev_bcd = le32toh(rombuf[2]);
	svnrev =
	    10000000 * ((svnrev_bcd >> 28) & 0xF) +
	    1000000 * ((svnrev_bcd >> 24) & 0xF) +
	    100000 * ((svnrev_bcd >> 20) & 0xF) +
	    10000 * ((svnrev_bcd >> 16) & 0xF) +
	    1000 * ((svnrev_bcd >> 12) & 0xF) +
	    100 * ((svnrev_bcd >> 8) & 0xF) +
	    10 * ((svnrev_bcd >> 4) & 0xF) +
	    (svnrev_bcd & 0xF);
	
	len = sizeof(kvbuf);
	if (sysctlbyname("kern.version", kvbuf, &len, NULL, 0) == -1) {
		warnx("sysctlbyname(kern.version)");
		return;
	}
	if (len == sizeof(kvbuf))
	kvbuf[len - 1] = '\0';
	kvp = kvbuf;
	kversion = kvp;
	while (*kvp != ':' && *kvp != '\0')
		kvp++;
	if (kvp == '\0') {
		warnx("malformed kernel version string: '%s'", kversion);
		return;
	}
	kvp[0] = '\0';
	kvp++;
	while (*kvp == ' ')
		kvp++;

	kdate = kvp;
	while (*kvp != '\n' && *kvp != '\0')
		kvp++;
	if (kvp == '\0') {
		warnx("malformed kernel version string");
		return;
	}
	kvp[0] = '\0';
	kvp++;
	while (*kvp == ' ')
		kvp++;

	kbuilder = kvp;
	while (*kvp != ':' && *kvp != '\0')
		kvp++;
	if (kvp == '\0') {
		warnx("malformed kernel version string");
		return;
	}
	kvp[0] = '\0';
	kvp++;

	if (*kvp != '/') {
		warnx("malformed kernel version string: config is not a path");
		return;
	}
	while (*kvp != '\n' && *kvp != '\0')
		kvp++;
	if (kvp == '\0') {
		warnx("malformed kernel version string");
		return;
	}
	kvp[0] = '\0';
	while (*(kvp - 1) != '/')
		kvp --;
	kconfig = kvp;

	asprintf(&version_string,
	    "CPU Info:\n"
	    "  Date: %4d-%02d-%02d %02d:%02d:%02d\n"
	    "  SVN Rev: r%d\n"
	    "\n"
	    "Kernel Info:\n"
	    "  Version: %s\n"
	    "  Date: %s\n"
	    "  Builder: %s\n"
	    "  Config: %s",	/* No terminating new-line */
	    year, month, day, hour, minute, second, svnrev,
	    kversion, kdate, kbuilder, kconfig);
	if (version_string == NULL) {
		warnx("asprintf");
		return;
	}

	image = malloc(sizeof(u_int32_t) * fb_width * fb_height);
	if (image == NULL)
		err(1, "show_about() malloc");
	fb_save(image);

	fb_dialog_gestures(TSGF_WEST | TSGF_2WEST | TSGF_ZOOM_OUT,
	    fb_colour(0, 0, 0), fb_colour(0, 0, 255), fb_colour(0, 0, 0),
	    "About the CHERI Demo", version_string);
	ts_drain();
	free(version_string);

	fb_post(image);
        free(image);
}


// display thumb nails
int
pictview_selector(void)
{
  const int scale=4; // images 1/4 thumbnails
  const int tile=3;  // 3 x 3 image tiles
  int display_image;
  int j,xi,yi;
  int x,y;
  int imgmap[tile][tile];
  struct tsstate *ts;

  // map images to tile locations
  imgmap[0][0] = SEL_QUILL_IMG;
  imgmap[1][0] = -1;
  imgmap[2][0] = SEL_TERM_IMG;

  imgmap[0][1] = -1;
  imgmap[1][1] = -1;
  imgmap[2][1] = -1;

  imgmap[0][2] = SEL_SLIDE_IMG;
  imgmap[1][2] = -1;
  imgmap[2][2] = SEL_BROWSER_IMG;

  if (slide_dir2 != NULL)
    imgmap[1][2] = SEL_SLIDE_IMG2;

  // display off
  fb_fade2off();

  // display background
  for(j=0; j<fb_width*fb_height; j++)
    fb_buf[j] = bgimage[j];
  fb_post(fb_buf);

  // display photos as tiles
  for(yi=0; yi<tile; yi++)
    for(xi=0; xi<tile; xi++)
      if(imgmap[xi][yi]>=0) {
	int x,y;
	int x0 = (fb_width/tile)*xi + ((fb_width/tile)-(fb_width/scale))/2;
	int y0 = (fb_height/tile)*yi + ((fb_height/tile)-(fb_height/scale))/2;
	int img = imgmap[xi][yi];
	for(y=0; y<fb_height/scale; y++)
	  for(x=0; x<fb_width/scale; x++)
	    fb_buf[x+x0+(y+y0)*fb_width] = selector_images[img][(x+y*fb_width)*scale];
      }
  fb_post(fb_buf);

  // display on
  fb_fade2on();

  ts_drain();

  // wait for image selection
  for(display_image=-1; (display_image<0); ) {
    ts = ts_poll();
    switch (ts->ts_gesture) {
    case TSG_CLICK:
      x = ts->ts_x1 / (fb_width / tile);
      y = ts->ts_y1 / (fb_height / tile);
      display_image = imgmap[x][y];
      ts_drain();
      break;
    case TSG2_EAST:
      show_about();
      break;
    }
  }

  fb_fade2off();
  return display_image;
}

static void
run_cheripoint(const char *dir)
{
  static int pmaster;
  int pslave, n;
  char buf[1024];
  ssize_t rlen;
  struct pollfd pfd[1];
  
  if (openpty(&pmaster, &pslave, NULL, NULL, NULL) == -1)
    err(1, "openpty");
  browser_pid = fork();
  if (browser_pid < 0)
    err(1, "fork()");
  else if (browser_pid > 0)
    close(pslave);
  else {
    close(pmaster);
    if (login_tty(pslave) < 0) {
      syslog(LOG_ALERT, "login_tty failed in child: %s", strerror(errno));
      err(1, "tty_login");
    }
    execl("/usr/bin/cheripoint", "cheripoint", "-f", dir, NULL);
    syslog(LOG_ALERT, "exec of /usr/bin/cheripoint failed: %s", strerror(errno));
    err(1, "execl()");
  }

  for(;;) {
    /*
     * If the child has exited, reset the state and return to the
     * main screen.
     */
    if (browser_pid == 0) {
      close(pmaster);
      break;
    }

    /* Check for output from the child and post it if needed */
    pfd[0].fd = pmaster;
    pfd[0].events = POLLIN;
    n = poll(pfd, 1, INFTIM);
    if (n == 0)
      continue;
    else if (n < 0) {
      if (errno == EINTR)
        continue;
      err(1, "poll");
    }
    if (n < 0) {
      syslog(LOG_ALERT, "poll failed with %s", strerror(errno));
	err(1, "poll");
    }
    if (pfd[0].revents & POLLIN) {
      rlen = read(pfd[0].fd, buf, sizeof(buf));
      if (rlen < 0) {
        syslog(LOG_ALERT, "read failed: %s", strerror(errno));
          err(1, "read");
      } else if (rlen > 0)
	  writeall(0, buf, rlen);
    }
  }
}


void
run_browser(void)
{
  static int pmaster;
  int pslave, n;
  char buf[1024];
  ssize_t rlen;
  struct pollfd pfd[1];
  
  if (openpty(&pmaster, &pslave, NULL, NULL, NULL) == -1)
    err(1, "openpty");
  browser_pid = fork();
  if (browser_pid < 0)
    err(1, "fork()");
  else if (browser_pid > 0)
    close(pslave);
  else {
    close(pmaster);
    if (login_tty(pslave) < 0) {
      syslog(LOG_ALERT, "login_tty failed in child: %s", strerror(errno));
      err(1, "tty_login");
    }
    execl("/usr/bin/browser", "browser", "-f", "-T", "/demo", NULL);
    syslog(LOG_ALERT, "exec of /usr/bin/browser failed: %s", strerror(errno));
    err(1, "execl()");
  }

  for(;;) {
    /*
     * If the child has exited, reset the state and return to the
     * main screen.
     */
    if (browser_pid == 0) {
      close(pmaster);
      break;
    }

    /* Check for output from the child and post it if needed */
    pfd[0].fd = pmaster;
    pfd[0].events = POLLIN;
    n = poll(pfd, 1, INFTIM);
    if (n == 0)
      continue;
    else if (n < 0) {
      if (errno == EINTR)
        continue;
      err(1, "poll");
    }
    if (n < 0) {
      syslog(LOG_ALERT, "poll failed with %s", strerror(errno));
	err(1, "poll");
    }
    if (pfd[0].revents & POLLIN) {
      rlen = read(pfd[0].fd, buf, sizeof(buf));
      if (rlen < 0) {
        syslog(LOG_ALERT, "read failed: %s", strerror(errno));
          err(1, "read");
      } else if (rlen > 0)
	  writeall(0, buf, rlen);
    }
  }
}


void
pictview(void)
{
  int display_image;
  int pan_direction = -1;

  fb_fade2off();
  busy_indicator();
  fb_fade2on();
  pictview_init();
  keyboard_init();

  while(1) {
    display_image = pictview_selector();
    if(display_image == SEL_TERM_IMG)
      keyboard_on();
      // show_text_buffer();
    else if(display_image == SEL_QUILL_IMG)
      pen_drawing();
    else if(display_image == SEL_BROWSER_IMG)
      run_browser();
    else if(display_image == SEL_SLIDE_IMG)
      run_cheripoint(slide_dir);
    else if(display_image == SEL_SLIDE_IMG2)
      run_cheripoint(slide_dir2);
  }
}


static void
usage(void)
{
	fprintf(stderr, "usage: pictview [-s <slide dir>] [-s <slide dir>]\n");
	exit(1);
}


int
main(int argc, char *argv[])
{
  int ch, tty;
  char *devpath;
  struct sigaction act;

  fb_buf = malloc(sizeof(*fb_buf) * fb_width * fb_height);

  // initialise framebuffers and mtl control for mmap access
  fb_init();
  fb_load_syscons_font(NULL, "/usr/share/syscons/fonts/iso-8x16.fnt");
  fb_text_cursor(255, 255);

   // various test routines...
  // flash_colours();
  // stripy_pixels_fast(dispfd);
  // fb_fill(fb_colour(0x00,0x00,0xff));
  // pen_drawing();
  // line_pattern();

  memset (&act, 0, sizeof(act));
  act.sa_handler = handle_sigchld;
  if (sigaction(SIGCHLD, &act, 0))
    err(1, "sigacation");

  while ((ch = getopt(argc, argv, "s:")) != -1) {
    switch (ch) {
    case 's':
      if (slide_dir == NULL)
        slide_dir = optarg;
      else if (slide_dir2 == NULL)
	slide_dir2 = optarg;
      else
	usage();
      break;
    default:
      usage();
    }
  }
  argc -= optind;
  argv += optind;

  if (argc > 1)
    errx(1, "usage: pictview [-s <slide dir>] [tty]");
  if (argc == 1) {
    kbdfd = -1;
    if (argv[0][0] != '/')
      asprintf(&devpath, "/dev/%s", argv[0]);
    else
      devpath = argv[0];
    if ((tty = open(devpath, O_RDWR)) < 0) {
      syslog(LOG_ALERT, "open failed with %s", strerror(errno));
      err(1, "open(%s)", devpath);
    }

    if (login_tty(tty) < 0) {
      syslog(LOG_ALERT, "login_tty failed: %s", strerror(errno));
      err(1, "login_tty()");
    }
  }

  if (slide_dir == NULL) {
    warnx("usage: must pass in -s <dir>");
    usage();
  }

  pictview();

  fb_fini();
  printf("The End\n");
  return 0;
}
