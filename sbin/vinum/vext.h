/*
 * Copyright (c) 1997 Nan Yang Computer Services Limited
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
 *      This product includes software developed for the NetBSD Project
 *      by Jason R. Thorpe.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* $Id: vext.h,v 1.9 1998/08/11 03:06:02 grog Exp grog $ */

#define MAXARGS 64					    /* maximum number of args on a line */
#define PLEXINITSIZE MAXPHYS				    /* block size to write when initializing */

enum {
    KILOBYTE = 1024,
    MEGABYTE = 1048576,
    GIGABYTE = 1073741824
};

/* Prototype declarations */
void parseline(int c, char *args[]);			    /* parse a line with c parameters at args */
void checkentry(int index);
int haveargs(int);					    /* check arg, error message if not valid */
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
void vinum_init(int argc, char *argv[], char *arg0[]);
void vinum_resetconfig(int argc, char *argv[], char *arg0[]);
void vinum_start(int argc, char *argv[], char *arg0[]);
void continue_revive(int plexno);
void vinum_stop(int argc, char *argv[], char *arg0[]);
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
void vinum_label(int argc, char *argv[], char *arg0[]);
void vinum_ld(int argc, char *argv[], char *arg0[]);
void vinum_ls(int argc, char *argv[], char *arg0[]);
void vinum_lp(int argc, char *argv[], char *arg0[]);
void vinum_lv(int argc, char *argv[], char *arg0[]);
#ifdef DEBUG
void vinum_debug(int argc, char *argv[], char *arg0[]);
#endif
void make_devices(void);
void get_drive_info(struct drive *drive, int index);
void get_sd_info(struct sd *sd, int index);
void get_plex_sd_info(struct sd *sd, int plexno, int sdno);
void get_plex_info(struct plex *plex, int index);
void get_volume_info(struct volume *volume, int index);
int find_object(const char *name, enum objecttype *type);
char *lltoa(long long l, char *s);
void vinum_ldi(int, int);
void vinum_lvi(int, int);
void vinum_lpi(int, int);
void vinum_lsi(int, int);
int vinum_li(int object, enum objecttype type);
char *roughlength(long long bytes, int);
u_int64_t sizespec(char *spec);

extern int force;					    /* set to 1 to force some dangerous ops */
extern int verbose;					    /* set verbose operation */
extern int Verbose;					    /* very verbose operation */
extern int recurse;					    /* set recursion */
extern int stats;					    /* show statistics */

/* Structures to read kernel data into */
extern struct _vinum_conf vinum_conf;			    /* configuration information */

extern struct volume vol;
extern struct plex plex;
extern struct sd sd;
extern struct drive drive;

extern jmp_buf command_fail;				    /* return on a failed command */
extern int superdev;					    /* vinum super device */

extern int line;					    /* stdin line number for error messages */
extern int file_line;					    /* and line in input file (yes, this is tacky) */

extern char buffer[];					    /* buffer to read in to */
