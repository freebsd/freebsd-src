/*
 * Copyright (c) 1994 Joerg Wunsch
 *
 * All rights reserved.
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
 *	This product includes software developed by Joerg Wunsch
 * 4. The name authors may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *
 * @(#)mcon.c, 3.20, Last Edit-Date: [Tue Dec 20 14:53:15 1994]
 */

/*---------------------------------------------------------------------------*
 *
 *	history:
 *
 *	-jw	initial version; includes a basic mapping between PeeCee
 *		scan codes and key names
 *	-hm	changed sys/pcvt_ioctl.h -> machine/pcvt_ioctl.h
 *
 *---------------------------------------------------------------------------*/

/*
 * Utility program to wire the mouse emulator control ioctl to the
 * user level. Allows setting of any configurable parameter, or
 * display the current configuration.
 */

#include <machine/pcvt_ioctl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/fcntl.h>

static const char *keynames[] = {
	"", "esc", "1", "2", "3", "4", "5", "6", "7", "8", "9", "0",
	"-", "+", "bksp", "tab", "q", "w", "e", "r", "t", "y", "u",
	"i", "o", "p", "[", "]", "enter", "ctrl", "a", "s", "d", "f",
	"g", "h", "j", "k", "l", ";", "\"", "`", "lshift", "\\",
	"z", "x", "c", "v", "b", "n", "m", ",", ".", "/", "rshift",
	"prtscr", "alt", "space", "caps", "f1", "f2", "f3", "f4",
	"f5", "f6", "f7", "f8", "f9", "f10", "numlock", "scrolllock",
	"kp7", "kp8", "kp9", "kp-", "kp4", "kp5", "kp6", "kp+",
	"kp1", "kp2", "kp3", "kp0", "kp."
};


const char *scantoname(int scan) {
	if(scan >= sizeof keynames / sizeof(const char *))
		return "???";
	else
		return keynames[scan];
}

int nametoscan(const char *name) {
	int i;
	for(i = 0; i < sizeof keynames / sizeof(const char *); i++)
		if(strcmp(keynames[i], name) == 0)
			return i;
	return -1;
}


int main(int argc, char **argv) {
	int c, errs = 0, fd, do_set = 0;
	int left = 0, mid = 0, right = 0, accel = 0, sticky = -1;
	struct mousedefs mdef;

	while((c = getopt(argc, argv, "l:m:r:a:s:")) != -1)
		switch(c) {
		case 'l':
			left = nametoscan(optarg);
			do_set = 1;
			if(left == -1) goto keynameerr;
			break;

		case 'm':
			mid = nametoscan(optarg);
			do_set = 1;
			if(mid == -1) goto keynameerr;
			break;

		case 'r':
			right = nametoscan(optarg);
			do_set = 1;
			if(right == -1) goto keynameerr;
			break;

		keynameerr:
		{
			fprintf(stderr, "unknown key name: %s\n",
				optarg);
			errs++;
		}
			break;

		case 'a':
			accel = 1000 * strtol(optarg, 0, 10);
			do_set = 1;
			break;

		case 's':
			if(strcmp(optarg, "0") == 0
			   || strcmp(optarg, "false") == 0
			   || strcmp(optarg, "no") == 0)
				sticky = 0;
			else if(strcmp(optarg, "1") == 0
				|| strcmp(optarg, "true") == 0
				|| strcmp(optarg, "yes") == 0)
				sticky = 1;
			else {
				fprintf(stderr, "invalid argument to -s: %s\n",
					optarg);
				errs++;
			}
			do_set = 1;
			break;

		default:
			errs++;
		}

	argc -= optind;
	argv += optind;

	if(errs || argc != 1) {
		fprintf(stderr, "usage: "
			"mouse [-l key][-m key][-r key][-a acctime][-s 0|1] "
			"mousedev\n");
		return 2;
	}

	if((fd = open(argv[0], O_RDONLY)) < 0) {
		perror("open(mousedev)");
		return 2;
	}
	if(ioctl(fd, KBDMOUSEGET, &mdef) < 0) {
		perror("ioctl(KBDMOUSEGET)");
		return 1;
	}

	if(!do_set) {
		printf("Current mouse emulator definitions:\n"
		       "left button: %s\n"
		       "middle button: %s\n"
		       "right button: %s\n"
		       "acceleration limit: %d msec\n"
		       "sticky buttons: %s\n",
		       scantoname(mdef.leftbutton),
		       scantoname(mdef.middlebutton),
		       scantoname(mdef.rightbutton),
		       mdef.acceltime / 1000,
		       mdef.stickybuttons? "yes": "no");
		return 0;
	}

	if(left) mdef.leftbutton = left & 0x7f;
	if(mid) mdef.middlebutton = mid & 0x7f;
	if(right) mdef.rightbutton = right & 0x7f;

	if(accel) mdef.acceltime = accel;
	if(sticky != -1) mdef.stickybuttons = sticky;

	if(ioctl(fd, KBDMOUSESET, &mdef) < 0) {
		perror("ioctl(KBDMOUSESET)");
		return 1;
	}

	return 0;
}
