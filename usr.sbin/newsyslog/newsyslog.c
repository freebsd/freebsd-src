/*
 * This file contains changes from the Open Software Foundation.
 */

/*

Copyright 1988, 1989 by the Massachusetts Institute of Technology

Permission to use, copy, modify, and distribute this software
and its documentation for any purpose and without fee is
hereby granted, provided that the above copyright notice
appear in all copies and that both that copyright notice and
this permission notice appear in supporting documentation,
and that the names of M.I.T. and the M.I.T. S.I.P.B. not be
used in advertising or publicity pertaining to distribution
of the software without specific, written prior permission.
M.I.T. and the M.I.T. S.I.P.B. make no representations about
the suitability of this software for any purpose.  It is
provided "as is" without express or implied warranty.

*/

/*
 *      newsyslog - roll over selected logs at the appropriate time,
 *              keeping the a specified number of backup files around.
 *
 *      $Source: /home/ncvs/src/usr.sbin/newsyslog/newsyslog.c,v $
 *      $Author: graichen $
 */

#ifndef lint
static char rcsid[] = "$Id: newsyslog.c,v 1.1.1.1 1996/01/05 09:28:10 graichen Exp $";
#endif /* not lint */

#ifndef CONF
#define CONF "/etc/athena/newsyslog.conf" /* Configuration file */
#endif
#ifndef PIDFILE
#define PIDFILE "/etc/syslog.pid"
#endif
#ifndef COMPRESS
#define COMPRESS "/usr/ucb/compress" /* File compression program */
#endif
#ifndef COMPRESS_POSTFIX
#define COMPRESS_POSTFIX ".Z"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <pwd.h>
#include <grp.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/wait.h>

#define kbytes(size)  (((size) + 1023) >> 10)
#ifdef _IBMR2
/* Calculates (db * DEV_BSIZE) */
#define dbtob(db)  ((unsigned)(db) << UBSHIFT) 
#endif

#define CE_COMPACT 1            /* Compact the achived log files */
#define CE_BINARY 2             /* Logfile is in binary, don't add */
                                /* status messages */
#define NONE -1
        
struct conf_entry {
        char    *log;           /* Name of the log */
        int     uid;            /* Owner of log */
        int     gid;            /* Group of log */
        int     numlogs;        /* Number of logs to keep */
        int     size;           /* Size cutoff to trigger trimming the log */
        int     hours;          /* Hours between log trimming */
        int     permissions;    /* File permissions on the log */
        int     flags;          /* Flags (CE_COMPACT & CE_BINARY)  */
        struct conf_entry       *next; /* Linked list pointer */
};

extern int      optind;
extern char     *optarg;
extern char *malloc();
extern uid_t getuid(),geteuid();
extern time_t time();

char    *progname;              /* contains argv[0] */
int     verbose = 0;            /* Print out what's going on */
int     needroot = 1;           /* Root privs are necessary */
int     noaction = 0;           /* Don't do anything, just show it */
char    *conf = CONF;           /* Configuration file to use */
time_t  timenow;
int     syslog_pid;             /* read in from /etc/syslog.pid */
#define MIN_PID		3
#define MAX_PID		65534
char    hostname[64];           /* hostname */
char    *daytime;               /* timenow in human readable form */


struct conf_entry *parse_file();
char *sob(), *son(), *strdup(), *missing_field();

main(argc,argv)
        int argc;
        char **argv;
{
        struct conf_entry *p, *q;
        
        PRS(argc,argv);
        if (needroot && getuid() && geteuid()) {
                fprintf(stderr,"%s: must have root privs\n",progname);
                exit(1);
        }
        p = q = parse_file();
        while (p) {
                do_entry(p);
                p=p->next;
                free((char *) q);
                q=p;
        }
        exit(0);
}

do_entry(ent)
        struct conf_entry       *ent;
        
{
        int     size, modtime;
        
        if (verbose) {
                if (ent->flags & CE_COMPACT)
                        printf("%s <%dZ>: ",ent->log,ent->numlogs);
                else
                        printf("%s <%d>: ",ent->log,ent->numlogs);
        }
        size = sizefile(ent->log);
        modtime = age_old_log(ent->log);
        if (size < 0) {
                if (verbose)
                        printf("does not exist.\n");
        } else {
                if (verbose && (ent->size > 0))
                        printf("size (Kb): %d [%d] ", size, ent->size);
                if (verbose && (ent->hours > 0))
                        printf(" age (hr): %d [%d] ", modtime, ent->hours);
                if (((ent->size > 0) && (size >= ent->size)) ||
                    ((ent->hours > 0) && ((modtime >= ent->hours)
                                        || (modtime < 0)))) {
                        if (verbose)
                                printf("--> trimming log....\n");
                        if (noaction && !verbose) {
                                if (ent->flags & CE_COMPACT)
                                        printf("%s <%dZ>: trimming",
                                               ent->log,ent->numlogs);
                                else
                                        printf("%s <%d>: trimming",
                                               ent->log,ent->numlogs);
                        }
                        dotrim(ent->log, ent->numlogs, ent->flags,
                               ent->permissions, ent->uid, ent->gid);
                } else {
                        if (verbose)
                                printf("--> skipping\n");
                }
        }
}

PRS(argc,argv)
        int argc;
        char **argv;
{
        int     c;
        FILE    *f;
        char    line[BUFSIZ];
	char	*p;

        progname = argv[0];
        timenow = time((time_t *) 0);
        daytime = ctime(&timenow) + 4;
        daytime[15] = '\0';

        /* Let's find the pid of syslogd */
        syslog_pid = 0;
        f = fopen(PIDFILE,"r");
        if (f && fgets(line,BUFSIZ,f))
                syslog_pid = atoi(line);
	if (f)
		(void)fclose(f);

        /* Let's get our hostname */
        (void) gethostname(hostname, sizeof(hostname));

	/* Truncate domain */
	if (p = strchr(hostname, '.')) {
		*p = '\0';
	}

        optind = 1;             /* Start options parsing */
        while ((c=getopt(argc,argv,"nrvf:t:")) != EOF)
                switch (c) {
                case 'n':
                        noaction++; /* This implies needroot as off */
                        /* fall through */
                case 'r':
                        needroot = 0;
                        break;
                case 'v':
                        verbose++;
                        break;
                case 'f':
                        conf = optarg;
                        break;
                default:
                        usage();
                }
        }

usage()
{
        fprintf(stderr,
                "Usage: %s <-nrv> <-f config-file>\n", progname);
        exit(1);
}

/* Parse a configuration file and return a linked list of all the logs
 * to process
 */
struct conf_entry *parse_file()
{
        FILE    *f;
        char    line[BUFSIZ], *parse, *q;
        char    *errline, *group;
        struct conf_entry *first = NULL;
        struct conf_entry *working;
        struct passwd *pass;
        struct group *grp;

        if (strcmp(conf,"-"))
                f = fopen(conf,"r");
        else
                f = stdin;
        if (!f) {
                (void) fprintf(stderr,"%s: ",progname);
                perror(conf);
                exit(1);
        }
        while (fgets(line,BUFSIZ,f)) {
                if ((line[0]== '\n') || (line[0] == '#'))
                        continue;
                errline = strdup(line);
                if (!first) {
                        working = (struct conf_entry *) malloc(sizeof(struct conf_entry));
                        first = working;
                } else {
                        working->next = (struct conf_entry *) malloc(sizeof(struct conf_entry));
                        working = working->next;
                }

                q = parse = missing_field(sob(line),errline);
                *(parse = son(line)) = '\0';
                working->log = strdup(q);

                q = parse = missing_field(sob(++parse),errline);
                *(parse = son(parse)) = '\0';
                if ((group = strchr(q, '.')) != NULL) {
                    *group++ = '\0';
                    if (*q) {
                        if (!(isnumber(*q))) {
                            if ((pass = getpwnam(q)) == NULL) {
                                fprintf(stderr,
                                    "Error in config file; unknown user:\n");
                                fputs(errline,stderr);
                                exit(1);
                            }
                            working->uid = pass->pw_uid;
                        } else
                            working->uid = atoi(q);
                    } else
                        working->uid = NONE;
                    
                    q = group;
                    if (*q) {
                        if (!(isnumber(*q))) {
                            if ((grp = getgrnam(q)) == NULL) {
                                fprintf(stderr,
                                    "Error in config file; unknown group:\n");
                                fputs(errline,stderr);
                                exit(1);
                            }
                            working->gid = grp->gr_gid;
                        } else
                            working->gid = atoi(q);
                    } else
                        working->gid = NONE;
                    
                    q = parse = missing_field(sob(++parse),errline);
                    *(parse = son(parse)) = '\0';
                }
                else 
                    working->uid = working->gid = NONE;

                if (!sscanf(q,"%o",&working->permissions)) {
                        fprintf(stderr,
                                "Error in config file; bad permissions:\n");
                        fputs(errline,stderr);
                        exit(1);
                }

                q = parse = missing_field(sob(++parse),errline);
                *(parse = son(parse)) = '\0';
                if (!sscanf(q,"%d",&working->numlogs)) {
                        fprintf(stderr,
                                "Error in config file; bad number:\n");
                        fputs(errline,stderr);
                        exit(1);
                }

                q = parse = missing_field(sob(++parse),errline);
                *(parse = son(parse)) = '\0';
                if (isdigit(*q))
                        working->size = atoi(q);
                else
                        working->size = -1;
                
                q = parse = missing_field(sob(++parse),errline);
                *(parse = son(parse)) = '\0';
                if (isdigit(*q))
                        working->hours = atoi(q);
                else
                        working->hours = -1;

                q = parse = sob(++parse); /* Optional field */
                *(parse = son(parse)) = '\0';
                working->flags = 0;
                while (q && *q && !isspace(*q)) {
                        if ((*q == 'Z') || (*q == 'z'))
                                working->flags |= CE_COMPACT;
                        else if ((*q == 'B') || (*q == 'b'))
                                working->flags |= CE_BINARY;
                        else {
                                fprintf(stderr,
                                        "Illegal flag in config file -- %c\n",
                                        *q);
                                exit(1);
                        }
                        q++;
                }
                
                free(errline);
        }
        if (working)
                working->next = (struct conf_entry *) NULL;
        (void) fclose(f);
        return(first);
}

char *missing_field(p,errline)
        char    *p,*errline;
{
        if (!p || !*p) {
                fprintf(stderr,"Missing field in config file:\n");
                fputs(errline,stderr);
                exit(1);
        }
        return(p);
}

dotrim(log,numdays,flags,perm,owner_uid,group_gid)
        char    *log;
        int     numdays;
        int     flags;
        int     perm;
        int     owner_uid;
        int     group_gid;
{
        char    file1[128], file2[128];
        char    zfile1[128], zfile2[128];
        int     fd;
        struct  stat st;

#ifdef _IBMR2
/* AIX 3.1 has a broken fchown- if the owner_uid is -1, it will actually */
/* change it to be owned by uid -1, instead of leaving it as is, as it is */
/* supposed to. */
                if (owner_uid == -1)
                  owner_uid = geteuid();
#endif

        /* Remove oldest log */
        (void) sprintf(file1,"%s.%d",log,numdays);
        (void) strcpy(zfile1, file1);
        (void) strcat(zfile1, COMPRESS_POSTFIX);

        if (noaction) {
                printf("rm -f %s\n", file1);
                printf("rm -f %s\n", zfile1);
        } else {
                (void) unlink(file1);
                (void) unlink(zfile1);
        }

        /* Move down log files */
        while (numdays--) {
                (void) strcpy(file2,file1);
                (void) sprintf(file1,"%s.%d",log,numdays);
                (void) strcpy(zfile1, file1);
                (void) strcpy(zfile2, file2);
                if (lstat(file1, &st)) {
                        (void) strcat(zfile1, COMPRESS_POSTFIX);
                        (void) strcat(zfile2, COMPRESS_POSTFIX);
                        if (lstat(zfile1, &st)) continue;
                }
                if (noaction) {
                        printf("mv %s %s\n",zfile1,zfile2);
                        printf("chmod %o %s\n", perm, zfile2);
                        printf("chown %d.%d %s\n",
                               owner_uid, group_gid, zfile2);
                } else {
                        (void) rename(zfile1, zfile2);
                        (void) chmod(zfile2, perm);
                        (void) chown(zfile2, owner_uid, group_gid);
                }
        }
        if (!noaction && !(flags & CE_BINARY))
                (void) log_trim(log);  /* Report the trimming to the old log */

        if (noaction) 
                printf("mv %s to %s\n",log,file1);
        else
                (void) rename(log,file1);
        if (noaction) 
                printf("Start new log...");
        else {
                fd = creat(log,perm);
                if (fd < 0) {
                        perror("can't start new log");
                        exit(1);
                }               
                if (fchown(fd, owner_uid, group_gid)) {
                        perror("can't chmod new log file");
                        exit(1);
                }
                (void) close(fd);
                if (!(flags & CE_BINARY))
                        if (log_trim(log)) {    /* Add status message */
                                perror("can't add status message to log");
                                exit(1);
                        }
        }
        if (noaction)
                printf("chmod %o %s...",perm,log);
        else
                (void) chmod(log,perm);
        if (noaction)
                printf("kill -HUP %d (syslogd)\n",syslog_pid);
        else
	if (syslog_pid < MIN_PID || syslog_pid > MAX_PID) {
		fprintf(stderr,"%s: preposterous process number: %d\n",
				progname, syslog_pid);
        } else if (kill(syslog_pid,SIGHUP)) {
                        fprintf(stderr,"%s: ",progname);
                        perror("warning - could not restart syslogd");
                }
        if (flags & CE_COMPACT) {
                if (noaction)
                        printf("Compress %s.0\n",log);
                else
                        compress_log(log);
        }
}

/* Log the fact that the logs were turned over */
log_trim(log)
        char    *log;
{
        FILE    *f;
        if ((f = fopen(log,"a")) == NULL)
                return(-1);
        fprintf(f,"%s %s newsyslog[%d]: logfile turned over\n",
                daytime, hostname, getpid());
        if (fclose(f) == EOF) {
                perror("log_trim: fclose:");
                exit(1);
        }
        return(0);
}

/* Fork of /usr/ucb/compress to compress the old log file */
compress_log(log)
        char    *log;
{
        int     pid;
        char    tmp[128];
        
        pid = fork();
        (void) sprintf(tmp,"%s.0",log);
        if (pid < 0) {
                fprintf(stderr,"%s: ",progname);
                perror("fork");
                exit(1);
        } else if (!pid) {
                (void) execl(COMPRESS,"compress","-f",tmp,0);
                fprintf(stderr,"%s: ",progname);
                perror(COMPRESS);
                exit(1);
        }
}

/* Return size in kilobytes of a file */
int sizefile(file)
        char    *file;
{
        struct stat sb;

        if (stat(file,&sb) < 0)
                return(-1);
        return(kbytes(dbtob(sb.st_blocks)));
}

/* Return the age of old log file (file.0) */
int age_old_log(file)
        char    *file;
{
        struct stat sb;
        char tmp[MAXPATHLEN+3];

        (void) strcpy(tmp,file);
        if (stat(strcat(tmp,".0"),&sb) < 0)
            if (stat(strcat(tmp,COMPRESS_POSTFIX), &sb) < 0)
                return(-1);
        return( (int) (timenow - sb.st_mtime + 1800) / 3600);
}


#ifndef OSF
/* Duplicate a string using malloc */

char *strdup(strp)
register char   *strp;
{
        register char *cp;

        if ((cp = malloc((unsigned) strlen(strp) + 1)) == NULL)
                abort();
        return(strcpy (cp, strp));
}
#endif

/* Skip Over Blanks */
char *sob(p)
        register char   *p;
{
        while (p && *p && isspace(*p))
                p++;
        return(p);
}

/* Skip Over Non-Blanks */
char *son(p)
        register char   *p;
{
        while (p && *p && !isspace(*p))
                p++;
        return(p);
}
