/*-
 * Copyright (c) 1997, 1998
 *	Nan Yang Computer Services Limited.  All rights reserved.
 *
 *  This software is distributed under the so-called ``Berkeley
 *  License'':
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
 *	This product includes software developed by Nan Yang Computer
 *      Services Limited.
 * 4. Neither the name of the Company nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * This software is provided ``as is'', and any express or implied
 * warranties, including, but not limited to, the implied warranties of
 * merchantability and fitness for a particular purpose are disclaimed.
 * In no event shall the company or contributors be liable for any
 * direct, indirect, incidental, special, exemplary, or consequential
 * damages (including, but not limited to, procurement of substitute
 * goods or services; loss of use, data, or profits; or business
 * interruption) however caused and on any theory of liability, whether
 * in contract, strict liability, or tort (including negligence or
 * otherwise) arising in any way out of the use of this software, even if
 * advised of the possibility of such damage.
 */

/*
 * $Id: vext.h,v 1.22 2003/04/28 06:19:06 grog Exp $
 * $FreeBSD$
 */

#define MAXARGS 64					    /* maximum number of args on a line */
#define PLEXINITSIZE 65536				    /* init in this size chunks */
#define MAXPLEXINITSIZE 65536				    /* max chunk size to use for init */
#define MAXDATETEXT 128					    /* date text in history (far too much) */
#define VINUMDEBUG					    /* for including kernel headers */

enum {
    KILOBYTE = 1024,
    MEGABYTE = 1048576,
    GIGABYTE = 1073741824
};

#define VINUMMOD "vinum"

#define DEFAULT_HISTORYFILE "/var/log/vinum_history"	    /* default name for history stuff */

#include <ctype.h>
#include <errno.h>
#include <sys/param.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <grp.h>
#include <netdb.h>
#include <paths.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/linker.h>
#include <sys/module.h>
#include <sys/resource.h>
#include <readline/history.h>
#include <readline/readline.h>
#include <sys/sysctl.h>

#include <sys/time.h>
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/malloc.h>
#include <sys/uio.h>
#include <sys/namei.h>
#include <sys/stat.h>
#include <sys/disklabel.h>
#include <sys/syslog.h>
#include <sys/fcntl.h>
#include <sys/queue.h>
#include <stdarg.h>
#include <vm/vm.h>
#include <dev/vinum/vinumvar.h>
#include <dev/vinum/vinumio.h>
#include <dev/vinum/vinumkw.h>
#include <dev/vinum/vinumutil.h>
#include <machine/cpu.h>

/* Prototype declarations */
void parseline(int c, char *args[]);			    /* parse a line with c parameters at args */
void checkentry(int index);
int haveargs(int);					    /* check arg, error message if not valid */
void setsigs(void);
void catchsig(int ignore);
void vinum_create(int argc, char *argv[], char *arg0[]);
void vinum_read(int argc, char *argv[], char *arg0[]);
void vinum_modify(int argc, char *argv[], char *arg0[]);
void vinum_volume(int argc, char *argv[], char *arg0[]);
void vinum_plex(int argc, char *argv[], char *arg0[]);
void vinum_sd(int argc, char *argv[], char *arg0[]);
void vinum_drive(int argc, char *argv[], char *arg0[]);
void vinum_list(int argc, char *argv[], char *arg0[]);
void vinum_info(int argc, char *argv[], char *arg0[]);
void vinum_set(int argc, char *argv[], char *arg0[]);
void vinum_rm(int argc, char *argv[], char *arg0[]);
void vinum_mv(int argc, char *argv[], char *arg0[]);
void vinum_init(int argc, char *argv[], char *arg0[]);
void listconfig(void);
void resetstats(struct vinum_ioctl_msg *msg);
void initvol(int volno);
void initplex(int plexno, char *name);
void initsd(int sdno, int dowait);
void vinum_resetconfig(int argc, char *argv[], char *arg0[]);
void vinum_start(int argc, char *argv[], char *arg0[]);
void continue_revive(int plexno);
void vinum_stop(int argc, char *argv[], char *arg0[]);
void vinum_makedev(int argc, char *argv[], char *arg0[]);
void vinum_help(int argc, char *argv[], char *arg0[]);
void vinum_quit(int argc, char *argv[], char *arg0[]);
void vinum_setdaemon(int argc, char *argv[], char *arg0[]);
void vinum_replace(int argc, char *argv[], char *arg0[]);
void vinum_readpol(int argc, char *argv[], char *arg0[]);
void reset_volume_stats(int volno, int recurse);
void reset_plex_stats(int plexno, int recurse);
void reset_sd_stats(int sdno, int recurse);
void reset_drive_stats(int driveno);
void vinum_resetstats(int argc, char *argv[], char *arg0[]);
void vinum_attach(int argc, char *argv[], char *argv0[]);
void vinum_detach(int argc, char *argv[], char *argv0[]);
void vinum_rename(int argc, char *argv[], char *argv0[]);
void vinum_rename_2(char *, char *);
void vinum_replace(int argc, char *argv[], char *argv0[]);
void vinum_printconfig(int argc, char *argv[], char *argv0[]);
void printconfig(FILE * of, char *comment);
void vinum_saveconfig(int argc, char *argv[], char *argv0[]);
int checkupdates(void);
void genvolname(void);
struct _drive *create_drive(char *devicename);
void vinum_concat(int argc, char *argv[], char *argv0[]);
void vinum_stripe(int argc, char *argv[], char *argv0[]);
void vinum_raid4(int argc, char *argv[], char *argv0[]);
void vinum_raid5(int argc, char *argv[], char *argv0[]);
void vinum_mirror(int argc, char *argv[], char *argv0[]);
void vinum_label(int argc, char *argv[], char *arg0[]);
void vinum_ld(int argc, char *argv[], char *arg0[]);
void vinum_ls(int argc, char *argv[], char *arg0[]);
void vinum_lp(int argc, char *argv[], char *arg0[]);
void vinum_lv(int argc, char *argv[], char *arg0[]);
void vinum_setstate(int argc, char *argv[], char *argv0[]);
void vinum_checkparity(int argc, char *argv[], char *argv0[]);
void vinum_rebuildparity(int argc, char *argv[], char *argv0[]);
void parityops(int argc, char *argv[], enum parityop op);
void start_daemon(void);
void vinum_debug(int argc, char *argv[], char *arg0[]);
void list_defective_objects(void);
void vinum_dumpconfig(int argc, char *argv[], char *argv0[]);
void dumpconfig(char *part);
int check_drive(char *devicename);
void get_drive_info(struct _drive *drive, int index);
void get_sd_info(struct _sd *sd, int index);
void get_plex_sd_info(struct _sd *sd, int plexno, int sdno);
void get_plex_info(struct _plex *plex, int index);
void get_volume_info(struct _volume *volume, int index);
struct _drive *find_drive_by_devname(char *name);
int find_object(const char *name, enum objecttype *type);
char *lltoa(int64_t l, char *s);
void vinum_ldi(int, int);
void vinum_lvi(int, int);
void vinum_lpi(int, int);
void vinum_lsi(int, int);
int vinum_li(int object, enum objecttype type);
char *roughlength(int64_t bytes, int);
u_int64_t sizespec(char *spec);
char *sd_state(enum sdstate);
void timestamp(void);

extern int force;					    /* set to 1 to force some dangerous ops */
extern int interval;					    /* interval in ms between init/revive */
extern int vflag;					    /* set verbose operation or verify */
extern int Verbose;					    /* very verbose operation */
extern int recurse;					    /* set recursion */
extern int sflag;					    /* show statistics */
extern int SSize;					    /* sector size for revive */
extern int dowait;					    /* wait for children to exit */
extern char *objectname;				    /* name for some functions */

extern FILE *History;					    /* history file */

/* Structures to read kernel data into */
extern struct __vinum_conf vinum_conf;			    /* configuration information */

extern struct _volume vol;
extern struct _plex plex;
extern struct _sd sd;
extern struct _drive drive;

extern jmp_buf command_fail;				    /* return on a failed command */
extern int superdev;					    /* vinum super device */

extern int line;					    /* stdin line number for error messages */
extern int file_line;					    /* and line in input file (yes, this is tacky) */

extern char buffer[];					    /* buffer to read in to */

#define min(a, b) a < b? a: b
