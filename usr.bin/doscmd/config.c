/*
 * Copyright (c) 1992, 1993, 1996
 *	Berkeley Software Design, Inc.  All rights reserved.
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
 *	This product includes software developed by Berkeley Software
 *	Design, Inc.
 *
 * THIS SOFTWARE IS PROVIDED BY Berkeley Software Design, Inc. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Berkeley Software Design, Inc. BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	BSDI config.c,v 2.2 1996/04/08 19:32:22 bostic Exp
 *
 * $Id: config.c,v 1.2 1996/09/18 16:12:24 miff Exp $
 */

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <sys/types.h>

#include "doscmd.h"

/*
** doscmdrc parser
*/
int
read_config(FILE *fp)
{
    char *buffer;
    char _buffer[1024];
    char *_av[16];
    char **av;
    int ac;
    int bootdrive = -1;

    while (buffer = fgets(_buffer, sizeof(_buffer), fp)) {
	char *comment = strchr(buffer, '#');
	char *equal;

    	if (comment)
	    *comment = 0;

    	while (isspace(*buffer))
	    ++buffer;
    	if (!*buffer)
	    continue;

	/*
	 * Strip <CR><LF>
	 */
    	comment = buffer;
    	while (*comment && *comment != '\n' && *comment != '\r')
		++comment;
    	*comment = 0;

	/*
	 * Check to see if this is to go in the environment
	 */
    	equal = buffer;
    	while (*equal && *equal != '=' && !isspace(*equal))
	    ++equal;

    	if (*equal == '=') {
	    if (strncmp(buffer, "MS_VERSION=", 11) == 0)
	    	setver(0, strtol(equal + 1, 0, 0));
	    else if (strncmp(buffer, "X11_FONT=", 9) == 0)
		xfont = strdup(equal + 1);
	    else
		put_dosenv(buffer);
	    continue;
    	}

    	ac = ParseBuffer(buffer, av = _av, 16);

    	if (ac == 0)
	    continue;
    	if (!strcasecmp(av[0], "assign")) {
	    int drive = -1;
    	    int printer;
    	    int ro = 0;

    	    if (ac < 2) {
		fprintf(stderr, "Usage: assign device ...\n");
		quit(1);
	    }
	    if (av[2] && !strcasecmp(av[2], "-ro")) {
		av[2] = av[1];
		av[1] = av[0];
		++av;
		--ac;
		ro = 1;
	    }
	    if (!strncasecmp(av[1], "lpt", 3)) {
		if (av[1][3] < '1' || av[1][3] > '4'
				   || av[1][4] != ':' || ac < 3) {
		    fprintf(stderr, "Usage: assign lptn: [direct] lpr-name [ time-out]\n");
		    quit(1);
		}
		printer = av[1][3] - '1';
		if (strchr(av[2], '/')) {
		    printer_direct(printer);
		    printer_spool(printer, av[2]);
		} else if (!strcasecmp(av[2], "direct")) {
		    printer_direct(printer);
		    printer_spool(printer, 0);
		} else {
		    printer_spool(printer, av[2]);
		    if (ac == 4)
			printer_timeout(printer, av[3]);
		}
	    } else if (!strncasecmp(av[1], "flop", 4)) {
		if (ac != 4) {
		    fprintf(stderr, "Usage: assign flop [-ro] file type\n");
		    quit(1);
		}

    	    	if (isdigit(av[1][4])) {
		    drive = atoi(&av[1][4]) - 1;
		} else if (islower(av[1][4]) && av[1][5] == ':' && !av[1][6]) {
		    drive = av[1][4] - 'a';
		} else if (isupper(av[1][4]) && av[1][5] == ':' && !av[1][6]) {
		    drive = av[1][4] - 'A';
		}
init_soft:
		drive = init_floppy(drive, atoi(av[3]), av[2]);
    	    	if (ro)
    	    	    make_readonly(drive);
    	    } else if (!strncasecmp(av[1], "hard", 4)) {
		int cyl, head, sec;

    	    	if (isdigit(av[1][4])) {
		    drive = atoi(&av[1][4]) + 1;
		} else if (islower(av[1][4]) && av[1][5] == ':' && !av[1][6]) {
		    drive = av[1][4] - 'a';
		} else if (isupper(av[1][4]) && av[1][5] == ':' && !av[1][6]) {
		    drive = av[1][4] - 'A';
		}

init_hard:
		switch (ac) {
		default:
		    fprintf(stderr, "Usage: assign [A-Z]: [-ro] directory\n"
			            "       assign hard [-ro] file type [boot_sector]\n"
		    		    "       assign hard [-ro] file cylinders heads sectors/track [boot_sector]\n");
		    quit(1);
    	    	case 5:
    	    	case 4:
		    if (!map_type(atoi(av[3]), &cyl, &head, &sec)) {
			fprintf(stderr, "%s: invalid type\n", av[3]);
		    	quit(1);
		    }
    	    	    drive = init_hdisk(drive, cyl, head, sec, av[2], av[4]);
		    if (ro)
			make_readonly(drive);
		    break;
    	    	case 7:
    	    	case 6:
    	    	    drive = init_hdisk(drive, atoi(av[3]), atoi(av[4]), atoi(av[5]),
			       av[2], av[6]);
		    if (ro)
			make_readonly(drive);
		    break;
		}
	    } else if (av[1][1] == ':') {
		if (av[1][2] || !isalpha(av[1][0])) {
		    fprintf(stderr, "Usage: assign [A-Z]: ...\n");
		    quit(1);
		}
    	    	if (isupper(av[1][0]))
		    drive = av[1][0] - 'A';
		else
		    drive = av[1][0] - 'a';

    	    	if (ac == 3) {
		    init_path(drive, (u_char *)av[2], 0);
		    if (ro)
		    	dos_makereadonly(drive);
    	    	} else if (drive < 2)
		    goto init_soft;
    	    	else
		    goto init_hard;
            } else if (!strncasecmp(av[1], "com", 3)) {
                int port;
                int addr;
                unsigned char irq;
                int i;
 
                if ((ac != 5) || (!isdigit(av[1][3]))) {
                    fprintf(stderr, "Usage: assign com[1-4] path addr irq\n");
                    quit(1);
                }
                port = atoi(&av[1][3]) - 1;
                if ((port < 0) || (port > (N_COMS_MAX - 1))) {
                    fprintf(stderr, "Usage: assign com[1-4] path addr irq\n");
                    quit(1);
                }
                errno = 0;
                addr = (int)strtol(av[3], '\0', 0);
                /* XXX DEBUG ISA-specific */
                if ((errno != 0) || (addr > MAXPORT)) {
                    fprintf(stderr, "Usage: assign com[1-4] path addr irq\n");
                    quit(1);
                }
                errno = 0;
                irq = (unsigned char)strtol(av[4], '\0', 0);
                /* XXX DEBUG ISA-specific */
                if ((errno != 0) || (irq < 1) || (irq > 15)) {
                    fprintf(stderr, "Usage: assign com[1-4] path addr irq\n");
                    quit(1);
                }
                init_com(port, av[2], addr, irq);
	    } else {
		fprintf(stderr, "Usage: assign flop ...\n");
		fprintf(stderr, "       assign hard ...\n");
		fprintf(stderr, "       assign [A-Z]: ...\n");
		fprintf(stderr, "       assign comX ...\n");
		quit(1);
	    }
    	} else if (!strcasecmp(av[0], "boot")) {
	    if (ac != 2 || av[1][2] || !isalpha(av[1][0])) {
		fprintf(stderr, "Usage: boot [A: | C:]\n");
		quit(1);
	    }
	    if (isupper(av[1][0]))
		bootdrive = av[1][0] - 'A';
	    else
		bootdrive = av[1][0] - 'a';
	    if (bootdrive != 0 && bootdrive != 2) {
	    	fprintf(stderr, "Boot drive must be either A: or C:\n");
		quit(1);
	    }
    	} else if (!strcasecmp(av[0], "setver")) {
	    int v;
	    if (ac != 3 || !(v = strtol(av[2], 0, 0))) {
		fprintf(stderr, "Usage: setver command version\n");
		quit(1);
	    }
    	    setver(av[1], v);
	} else {
	    fprintf(stderr, "%s: invalid command\n", av[0]);
	    quit(1);
    	}
    }
    fclose(fp);
    return(bootdrive);
}
