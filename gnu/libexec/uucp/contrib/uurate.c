/*
 * @(#)uurate.c 1.2 - Thu Sep  3 18:32:46 1992
 *
 * This program digests log and stats files in the "Taylor" format
 * and outputs various statistical data to standard out.
 *
 * Author:
 *      Bob Denny (denny@alisa.com)
 *      Fri Feb  7 13:38:36 1992
 *
 * Original author:
 *      Mark Pizzolato   mark@infopiz.UUCP
 *
 * Edits:
 *      Bob Denny - Fri Feb  7 15:04:54 1992
 *      Heavy rework for Taylor UUCP. This was the (very old) uurate from
 *      DECUS UUCP, which had a single logfile for activity and stats.
 *      Personally, I would have done things differently, with tables
 *      and case statements, but in the interest of time, I preserved
 *      Mark Pizzolato's techniques and style.
 *
 *      Bob Denny - Sun Aug 30 14:18:50 1992
 *      Changes to report format suggested by Francois Pinard and others.
 *      Add summary report, format from uutraf.pl (perl script), again
 *      thanks to Francois. Integrate and checkout with 1.03 of Taylor UUCP.
 *
 *      Stephan Niemz <stephan@sunlab.ka.sub.org> - Fri Apr 9 1993
 *      - Print totals in summary report,
 *      - show all commands in execution report,
 *      - count incoming calls correctly,
 *      - suppress empty tables,
 *      - don't divide by zero in efficiency report,
 *      - limit the efficiency to 100% (could be more with the i-protocol),
 *      - suppress some zeros to improve readability,
 *      - check for failure of calloc,
 *      - -h option changed to -s for consistency with all other uucp commands
 *        (but -h was left in for comptibility).
 *
 *      Scott Boyd <scott@futures.com> - Thu Aug 26 13:21:34 PDT 1993
 *      - Changed hosts linked-list insertion routine so that hosts
 *        are always listed in alphabetical order on reports.
 *
 *      Klaus Dahlenburg <kdburg@incoahe.hanse.de> - Fri Jun 18 1993 (1.2.2)
 *      - redesigned the printed layout (sticked to those 80 column tubes).
 *      - 'Retry time not ...' and ' ERROR: All matching ports ...' will now be
 *        counted as calls and will raise the failed-call counter.
 *      - times now shown as hh:mm:ss; the fields may hold up to 999 hrs  
 *        (a month equals 744 hrs at max). Printing will be as follows:
 *
 *         hrs > 0  hh:mm:ss
 *         min > 0     mm:ss
 *         sec > 0        ss
 *         leading zeroes are suppressed.
 *
 *      - the bytes xfered will be given in thousands only (we're counting 
 *        so 1K is 1000 bytes!). Sums up to 9,999,999.9 thousand can be shown.
 *      - dropped the fractions of a byte in columns: bytes/second (avg cps).
 *      - File statistic changed to display in/out in one row instead of 2
 *        separate reports.
 *      - eliminated the goto in command report and tightened the code; also
 *        the 'goto usage' has been replaced by a call to void usage() with no
 *        return (exit 1).
 *      - a totaling is done for all reports now; the total values are held 
 *        within the structure; after finishing read there will be an alloc
 *        for a site named 'Total' so that the totals line will be printed
 *        more or less automatically.
 *      - option -t implemented: that is every possible report will be given.
 *      - the start and end date/time of the input files are printed; can be
 *        dropped by the -q option at run time.
 *      - it is now possible to use Log/Stats files from V2_LOGGING configs.
 *        They must however not be mixed together (with each other).
 *      - the Log/Stats files are taken from config which is passed via
 *        Makefile at compile time. Variable to set is: newconfigdir. If the
 *        default config can't be read the default values are used
 *        (the config is optional!).
 *        Note: keyword/filename must be on the same line (no continuation).
 *      - -I option implemented to run with a different config file. In case
 *        the file can't be opened the run is aborted!
 *      - -q option implemented to run without environment report (default is
 *        FALSE: print the report).
 *      - -p option added to print protocol statistics: one for the packets
 *        and one for the errors encountered
 *      - reapplied patch by Scott Boyd <scott@futures.com> that I did not
 *        get knowledge of
 */
/* $Log: uurate.c,v $
 * Revision 1.4  1995/08/19 21:24:38  ache
 * Commit delta: current -> 1.06 + FreeBSD configuration
 *
 * Revision 1.15  1994/04/07  21:47:11  kdburg
 * printed 'no data avail' while there was data; layout chnaged
 * (cosmetic only)
 *
 * Revision 1.14  1994/04/07  21:16:32  kdburg
 * the layout of the protocol-used line within the LOGFILE changed
 * from 1.04 to 1.05; both formats may be used together; output
 * changed for packet report (columns adjusted)
 *
 * Revision 1.13  1994/04/04  10:04:35  kdburg
 * cosmetic change to the packet-report (separator lines)
 *
 * Revision 1.12  1994/03/30  19:52:04  kdburg
 * incorporated patch by Scott Boyd which was missing from this version
 * of uurate.c. Left the comment in cronological order.
 *
 * Revision 1.11  1994/03/28  18:53:22  kdburg
 * config not checked properly for 'logfile/statsfile' overwrites, bail-out
 * possible; wrong file name written to log for statsfile when found
 *
 * Revision 1.10  1993/09/28  16:46:51  kdburg
 * transmission failures denoted by: failed after ... in stats file
 * have not been counted at all.
 *
 * Revision 1.9  1993/08/17  23:38:36  kdburg
 * sometimes a line(site) was missing from the protocol stats due
 * to a missing +; added option -d and -v reassing option -h to print
 * the help; a zero was returned instead of a null-pointer by
 * prot_sum
 *
 * Revision 1.8  1993/07/03  06:58:55  kdburg
 * empty input not handled properly; assigned some buffer to input; msg
 * not displayed when no protocol data was available
 *
 * Revision 1.7  1993/06/27  10:31:53  kdburg
 * rindex was replaced by strchr must be strrchr
 *
 * Revision 1.6  1993/06/26  06:59:18  kdburg
 * switch hdr_done not reset at beginning of protocol report
 *
 * Revision 1.5  1993/06/25  22:22:30  kdburg
 * changed rindex to strchr; if there is no NEWCONFIG defined take
 * appropriate action
 *
 * Revision 1.4  1993/06/25  20:04:07  kdburg
 * added comment about -p option; inserted proto for rindex
 *
 * Revision 1.3  1993/06/25  19:31:14  kdburg
 * major rework done; added protocol reports (normal/errors)
 *
 * Revision 1.2  1993/06/21  19:53:54  kdburg
 * init
 * */

char version[] = "@(#) Taylor UUCP Log File Summary Filter, Version 1.2.2";
static char rcsid[] = "$FreeBSD$";
#include <ctype.h>            /* Character Classification      */
#include <math.h>
#include "uucp.h"
/* uucp.h includes string.h or strings.h, no include here. */

#if HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif

#define _DEBUG_ 0

/*
 * Direction of Calling and Data Transmission
 */

#define IN      0            /* Inbound            */
#define OUT     1            /* Outbound            */

/*
 *  define some limits
 */
#define MAXCOLS    8        /* report has this # of columns incl. 'name' */
#define MAXREP     6        /* number of reports available */
#define MAXFNAME  64        /* max input file name length incl. path*/
#define MAXDNAME   8        /* max display (Hostname) name length  */

/*
 * Data structures used to collect information
 */
struct File_Stats
    {
    int files;                      /* Files Transferred      */
    unsigned long bytes;      /* Data Size Transferred*/
    double time;                /* Transmission Time      */
    };

struct Phone_Call
    {
    int calls;                           /* Call Count            */
    int succs;                           /* Successful calls     */
    double connect_time;           /* Connect Time Spent      */
    struct File_Stats flow[2];       /* Rcvd & Sent Data      */
    };

struct Execution_Command
    {
    struct Execution_Command *next;
    char Commandname[64];
    int count;
    };

struct Protocol_Summary
    {
    struct Protocol_Summary *next;
    char type[3];
    long int  pr_cnt;
    long int  pr_psent;
    long int  pr_present;
    long int  pr_preceived;
    long int  pr_eheader;
    long int  pr_echksum;
    long int  pr_eorder;
    long int  pr_ereject;
    long int  pr_pwinmin;
    long int  pr_pwinmax;
    long int  pr_psizemin;
    long int  pr_psizemax;
    };

struct Host_entry
    {
    struct Host_entry *next;
    char Hostname[32];
    struct Execution_Command *cmds;      /* Local Activities */
    struct Phone_Call call[2];            /* In & Out Activities */
    struct Protocol_Summary *proto;
    };

  struct Host_entry *hosts = NULL;
  struct Host_entry *tot = NULL;
  struct Host_entry *cur = NULL;
  struct Execution_Command *cmd, *t_cmds = NULL;
  struct Protocol_Summary *prot, *t_prot, *s_prot, *ss_prot = NULL;
/*
 * Stuff for getopt()
 */

extern int optind;                /* GETOPT : Option Index */
extern char *optarg;            /* GETOPT : Option Value */
#if ! HAVE_STDLIB_H
   extern pointer *calloc();
#endif  /* HAVE_STDLIB_H */
/*
 * Default files to read. Taken from Taylor compile-time configuration.
 * def_logs must look like an argvec, hence the dummy argv[0].
 * Maybe later modified by scanning the config
 */

static char *def_logs[3] = { NULL, NULL, NULL};
char *I_conf = NULL;            /* points to config lib given by -I option */
char *D_conf = NULL;            /* points to config lib from makefile */
char *Tlog = NULL;              /* points to Log-file */
char *Tstat = NULL;             /* points to Stats-file */
char Pgm_name[64];              /* our pgm-name */
char logline[BUFSIZ+1];         /* input area */
char noConf[] = "- not defined -";
char buff[16*BUFSIZ];
char sbuff[2*BUFSIZ];

/*
 * Boolean switches for various decisions
 */

  int p_done = FALSE;           /* TRUE: start date/time of file printed */
  int hdr_done = FALSE;         /* TRUE: report header printed */
  int show_files = FALSE;       /* TRUE: -f option given */
  int show_calls = FALSE;       /* TRUE: -c option given */
  int show_commands = FALSE;    /* TRUE: -x option given */
  int show_efficiency = FALSE;  /* TRUE: -e option given */
  int show_all = FALSE;         /* TRUE: -t option given */
  int show_proto = FALSE;       /* TRUE: -p option given */
  int use_stdin = FALSE;        /* TRUE: -i option given */
  int be_quiet = FALSE;         /* TRUE: -q option given */
  int have_files[2];            /* TRUE: [IN] or [OUT] files found */
  int have_calls = FALSE;       /* TRUE: in/out calls found */
  int have_commands = FALSE;    /* TRUE: found uuxqt records */
  int have_proto = FALSE;       /* TRUE: protocol data found */
  int no_records = TRUE;        /* FALSE: got one record from file */

/*
 * protos
 */

static pointer *getmem(unsigned n);
static void inc_cmd(struct Execution_Command **, char *name);
static void fmtime(double sec, char *buf);
static void fmbytes(unsigned long n, char *buf);
static void usage();
static int  chk_config(char *conf, int n, int type);
static void hdrprt(char c, int bot);
struct Protocol_Summary *prot_sum(struct Protocol_Summary **, char *, int);

/*
 * BEGIN EXECUTION
 */

int main(argc, argv)
         int argc;
         char *argv[];
{
  FILE *Log = NULL;
  int c; 
  char *p, *s, *stt, *flq = NULL;
  char Hostname[MAXHOSTNAMELEN]; /* def taken from <sys/param.h> */
  char Filename[15];             /* filename to be printed */
  char in_date[14];              /* holds the date info of record read*/
  char in_time[14];              /* holds the time info of record read */
  char dt_info[31];  /* holds the date info from the last record read */
  char *logmsg;
  int sent, called = IN;
  int report = 0;            /* if <= 0 give msg that no report was avail. */
  int junk;

  /* --------------------------------------------------------------------
   *           P r o l o g
   * --------------------------------------------------------------------
   */

   Hostname[0] = '\0';
   have_files[IN]= have_files[OUT]= FALSE;
   setvbuf(stdout,sbuff,_IOFBF,sizeof(sbuff));

  /*
   * get how we've been called isolate the name from the path
   */

   if ((stt = strrchr(argv[0],'/')) != NULL)
      strcpy(Pgm_name,++stt);
   else
      strcpy(Pgm_name,argv[0]);
   def_logs[0] = Pgm_name;
   
  /*
   * I wish the compiler had the #error directive!
   */

#if !HAVE_TAYLOR_LOGGING && !HAVE_V2_LOGGING
  fprintf(stderr,"\a%s: (E) %s\n",Pgm_name,"Your config of Taylor UUCP is not yet supported.");
  fprintf(stderr,"%s: (E) %s\n",Pgm_name,"Current support is for V2 or TAYLOR logging only.");
  puts("   Run aborted due to errors\n")
  exit(1);
#endif

  /*
   *  get some mem to store the default config name (def's are in
   *  policy.h )
   */

  if (sizeof(NEWCONFIGLIB) > 1)       /* defined at compile time */
  {
     D_conf = (char *)getmem((sizeof(NEWCONFIGLIB) + sizeof("/config")));
     strcpy(D_conf,NEWCONFIGLIB);       /* passed by makefile */
     strcat(D_conf,"/config");
  }
  Tlog   = (char *)getmem(sizeof(LOGFILE));
  Tstat  = (char *)getmem(sizeof(STATFILE));
  Tlog   = LOGFILE;
  Tstat  = STATFILE;
  
  /*
   * Process the command line arguments
   */

  while((c = getopt(argc, argv, "I:s:cfdexaitphv")) != EOF)
  {
    switch(c)
    {
         case 'h':
                  (void) usage();
         case 's':
                  strcpy(Hostname, optarg);
                    break;
         case 'c':
                  show_calls = TRUE;
                  ++report;
                  break;
         case 'd':
                  printf("%s: (I) config-file default: %s\n",Pgm_name,D_conf);
                  exit (0);
                  break;
         case 'f':
                  show_files = TRUE;
                  ++report;
                  break;
         case 'x':
                  show_commands = TRUE;
                  ++report;
                  break;
         case 'e':
                  show_efficiency = TRUE;
                  ++report;
                  break;
         case 'a':
                  show_calls = show_files = show_commands = show_efficiency = TRUE;
                  report = 4;
                  break;
         case 'i':
                  use_stdin = TRUE;
                  break;
         case 't':
                  show_all = TRUE;
                  report = MAXREP;
                  break;
         case 'p':
                  show_proto = TRUE;
                  ++report;
                  break;
         case 'I':
                  I_conf = (char *)getmem(sizeof(optarg));
                  I_conf = optarg;
                  break;
                  case 'q':
                  be_quiet = TRUE;
                  break;
         case 'v':
                  printf("%s\n",rcsid);
                  exit (0);
         default :
                  (void) usage();
     }
  }
  if (report == 0)           /* no options given */
     ++report;               /* at least summary can be printed */
  if (! be_quiet)
     hdrprt('i',0);         /* print header for environment info */

  /*
   * Adjust argv and argc to account for the args processed above.
   */

  argc -= (optind - 1);
  argv += (optind - 1);

  /*
   * If further args present, Assume rest are logfiles for us to process
   * which should be given in pairs (log plus stat) otherwise the results may
   * not come out as expected! If no further args are present take input from 
   * Log and Stat files provided in the compilation environment of Taylor UUCP. 
   * If -i was given, Log already points to stdin and no file args are accepted.
   */

   if (use_stdin)           /* If -i, read from stdin */
   {
      if (argc != 1)            /* No file arguments allowed */
      {
         fprintf(stderr,"\a%s: (E) %s\n",Pgm_name,
                         "it's not posssible to give file args with '-i'");
         (void) usage();
      }
      else
      {
         argc = 2;
         Log = stdin;
         if (! be_quiet)
            puts("   Input from stdin; no other files will be used\n");
      }
   }
   else
   {
      if (argc != 1)                    /* file arguments are present */
      {
         if (! be_quiet)
            puts("   No defaults used; will use passed file arguments\n");
      }
      else                            /* Read from current logs */
      {
         def_logs[1] = Tlog;      /* prime the */
         def_logs[2] = Tstat;     /*   file names */   
         if (! be_quiet)
            printf("   Config for this run: ");

         if (I_conf != NULL)
         {
            junk = 0;
            if (! be_quiet)
                printf("%s\n",I_conf);
            if (0 != (chk_config(I_conf,be_quiet,junk)))
               return (8);
         }
         else
         {
           if (D_conf != NULL)
           {
              junk = 1;             /* indicate default (compiled) config */
              if (! be_quiet)
                 printf("%s\n",D_conf);
              chk_config(D_conf,be_quiet,junk);
           }
           else
              if (! be_quiet)
                 printf("%s\n",noConf);
         }
         def_logs[1] = Tlog;      /* final setting of */
         def_logs[2] = Tstat;     /*   file names */   
         argv = def_logs;            /* Bash argvec to log/stat files */
         argc = sizeof(def_logs) / sizeof(def_logs[0]);
       }
   }

  /* --------------------------------------------------------------------
   *                 MAIN LOGFILE PROCESSING LOOP
   * --------------------------------------------------------------------
   */

  if (!use_stdin)
  {
     if (argc < 3 && ! be_quiet)
     {
        puts("   (W) there is only one input file!");
        puts("   (W) some reports may not be printed");
     }
     if (! be_quiet)
        hdrprt('d',0);      /* give subheaderline  */
  }

  while (argc > 1)
  {
    if (!use_stdin && (Log = fopen(argv[1], "r")) == NULL)
    {
       perror(argv[1]);
       exit (8);
    }
    setvbuf(Log,buff,_IOFBF,sizeof(buff));
    if ((flq = strrchr(argv[1], '/')) == NULL)
       strncpy(Filename,argv[1],sizeof(Filename)-1);
    else
       strncpy(Filename,++flq,sizeof(Filename)-1);
       
    strcpy(in_date,"   n/a");
    strcpy(in_time,"   n/a");
    p_done = FALSE;             /* no info printed yet */
    no_records = TRUE;          /* not read any record yet */

    /*
     * Read each line of the logfile and collect information
     */

    while (fgets(logline, sizeof(logline), Log))
    {
        /*
         * The host name of the other end of the connection is
         * always the second field of the log line, whether we
         * are reading a Log file or a Stats file. Set 'p' to
         * point to the second field, null-terminated. Skip
         * the line if something is funny. V2 and Taylor ar identical
         * up to this part. Put out the start/end date of the files read;
         */

      if (NULL == (p = strchr(logline, ' ')))
         continue;
      no_records = FALSE;          /* got one (usable) record at least */
      ++p;

      if (NULL != (stt = strchr(p, '(')))
      {
         if (! p_done && ! use_stdin && ! be_quiet)
         {  

#if HAVE_TAYLOR_LOGGING
         sscanf(++stt,"%s%*c%[^.]",in_date,in_time);
#endif /* HAVE_TAYLOR_LOGGING */

#if HAVE_V2_LOGGING
         sscanf(++stt,"%[^-]%*c%[1234567890:]",in_date,in_time);
#endif /* HAVE_V2_LOGGING */

            printf("   %-14s %10s %8s",Filename, in_date, in_time);
            strcpy(in_date,"   n/a");         /* reset to default */
            strcpy(in_time,"   n/a");
            p_done = TRUE;
         }
         else
         {
            if (! use_stdin && ! be_quiet)  /* save for last time stamp prt. */
               strncpy(dt_info,++stt,sizeof(dt_info)-1);
         }
      }

      if (NULL != (s = strchr(p, ' ')))
         *s = '\0';
      for (s = p; *s; ++s)
          if (isupper(*s))
             *s = tolower(*s);

        /*
         * Skip this line if we got -s <host> and
         * this line does not contain that host name.
         * Don't skip the `incoming call' line with the system name `-'.
         */

      if (Hostname[0] != '\0')
         if ( (p[0] != '-' || p[1] != '\0') && 0 != strcmp(p, Hostname) )
            continue;

        /*
         * We are within a call block now. If this line is a file
         * transfer record, determine the direction. If not then
         * skip the line if it is not interesting.
         */
      
      if ((s = strchr(++s, ')')) == NULL)
         continue;

#if ! HAVE_TAYLOR_LOGGING
#if HAVE_V2_LOGGING
      if ((strncmp(s,") (",3)) ==  0)      /* are we in stats file ?) */
         if ((s = strchr(++s, ')')) == NULL)
            continue;                     /* yes but strange layout */
#endif /* HAVE_V2_LOGGING */
#endif /* ! HAVE_TAYLOR_LOGGING  */ 

       logmsg = s + 2;            /* Message is 2 characters after ')' */
       if ((0 != strncmp(logmsg, "Call complete", 13)) &&
          (0 != strncmp(logmsg, "Calling system", 14)) &&
          (0 != strncmp(logmsg, "Incoming call", 13)) &&
          (0 != strncmp(logmsg, "Handshake successful", 20)) &&
          (0 != strncmp(logmsg, "Retry time not", 14)) &&
          (0 != strncmp(logmsg, "ERROR: All matching ports", 25)) &&
          (0 != strncmp(logmsg, "Executing", 9)) &&
          (0 != strncmp(logmsg, "Protocol ", 9)) &&
          (0 != strncmp(logmsg, "sent ", 5)) &&
          (0 != strncmp(logmsg, "received ", 9)) &&
          (0 != strncmp(logmsg, "failed after ", 13)) &&
          (0 != strncmp(logmsg, "Errors: ", 8)))
          continue;

        /*
         * Find the Host_entry for this host, or create a new
         * one and link it on to the list.
         */

       if ((cur == NULL) || (0 != strcmp(p, cur->Hostname)))
       {
          struct Host_entry *e, *last;

          for (e= cur= hosts; cur != NULL ; e= cur, cur= cur->next)
              if (0 == strcmp(cur->Hostname, p))
                 break;
              if (cur == NULL)
              {
                 cur= (struct Host_entry *)getmem(sizeof(*hosts));
                 strcpy(cur->Hostname, p);
                 if (hosts == NULL)
                    e= hosts= cur;
		 else {
                    e = hosts;
                    last = NULL;
                    while (e != NULL) {
                          if (strcmp(e->Hostname, cur->Hostname) <= 0) {
                             if (e->next == NULL) {
                                e->next = cur;
                                break;
                             }
                             last = e;
                             e = e->next;
                          }
                          else {
                             cur->next = e;
                             if (last == NULL)
                                hosts = cur;
                             else
                                last->next = cur;
                             break;
                          }
                     }   /*  while (e != NULL) */ 
                 }    /*  hosts == NULL  */ 
              }   /* cur == NULL */
       }

        /*
         * OK, if this is a uuxqt record, find the Execution_Command
         * structure for the command being executed, or create a new
         * one. Then count an execution of this command.
         * (Log file only)
         */

        if (0 == strncmp(logmsg, "Executing", 9))
        {
            if (NULL == (p = strchr(logmsg, '(')))
               continue;
            if ((s = strpbrk(++p, " )")) == NULL)
               continue;
            *s = '\0';
            inc_cmd(&cur->cmds, p);
            inc_cmd(&t_cmds, p);
            have_commands = TRUE;
            continue;
        }

        /*
         * Count start of outgoing call.
         */

        if ((0 == strncmp(logmsg, "Calling system", 14)) ||
            (0 == strncmp(logmsg, "Retry time not", 14)) ||
            (0 == strncmp(logmsg, "ERROR: All matching ports", 25)))
        {
           called = OUT;
           cur->call[OUT].calls++;
           have_calls = TRUE;
           s_prot = NULL;              /* destroy pointer to protocol */
           continue;
        }

        /*
         * Count start of incoming call.
         */

        if (0 == strncmp(logmsg, "Incoming call", 13))
        {
           called = IN;
           s_prot = NULL;              /* destroy pointer to protocol */
           continue;
        }

        /*
         * On an incoming call, get system name from the second line.
         * Get protocol type and size/window too
         */

        if (0 == strncmp(logmsg, "Handshake successful", 20))
        {
           if ( called==IN )
              cur->call[IN].calls++;
           have_calls = TRUE;
           s_prot = NULL;              /* destroy pointer to protocol */
           if (NULL == (p = strchr(logmsg, '(')))
              continue;
           if (0 == strncmp(p, "(protocol ", 10))
           {
              if (NULL == (p = strchr(p, '\'')))
                 continue;
              ss_prot = prot_sum(&cur->proto, ++p, 1);
              s_prot  = prot_sum(&t_prot, p, 1);
              continue;
           }
        }

        /*
         * check protocol type and get stats
         *
         */

        if (0 == strncmp(logmsg, "Protocol ", 9))
        {
           s_prot = NULL;              /* destroy pointer to protocol */
           if (NULL == (p = strchr(logmsg, '\'')))
              continue;
           ss_prot = prot_sum(&cur->proto, ++p, 2);
           s_prot = prot_sum(&t_prot, p, 2);
           continue;
        }

        /*
         * check protocol errors. Unfortunately the line does not contain
         * the used protocol, so if any previous line did contain that
         * information and we did process that line we will save the pointer
         * to that particular segment into s_prot. If this pointer is not set
         * the error info is lost for we don't know where to store.
         *
         */

        if ((0 == strncmp(logmsg, "Errors: header", 14)) && s_prot != NULL)
        {
          int i1,i2,i3,i4 = 0;
          sscanf(logmsg,"%*s %*s %d%*c%*s %d%*c%*s %d%*c%*s %*s%*c %d",&i1,&i2,&i3,&i4);
          ss_prot->pr_eheader += i1;
          ss_prot->pr_echksum += i2;
          ss_prot->pr_eorder += i3;
          ss_prot->pr_ereject += i4;
          s_prot->pr_eheader += i1;
          s_prot->pr_echksum += i2;
          s_prot->pr_eorder += i3;
          s_prot->pr_ereject += i4;
          s_prot = NULL;
          continue;
        }

        /*
         * Handle end of call. Pick up the connect time.
         * position is on the closing paren of date/time info
         * i.e: ) text....  
         */

        if (0 == strncmp(logmsg, "Call complete", 13))
        {
           cur->call[called].succs++;
           s_prot = NULL;              /* destroy pointer to protocol */
           if (NULL == (s = strchr(logmsg, '(')))
              continue;
           cur->call[called].connect_time += atof(s+1);
           continue;
        }

        /*
         * We are definitely in a Stats file now.
         * If we reached here, this must have been a file transfer
         * record. Count it in the field corresponding to the
         * direction of the transfer. Count bytes transferred and
         * the time to transfer as well.
         * Position within the record is at the word 'received' or 'sent'
         * depending on the direction.
         */

        sent = IN;              /* give it an initial value */
        if (0 == strncmp(logmsg, "failed after ",13))
           logmsg += 13;        /* the transmission failed for any reason */
                                /* so advance pointer */
        if (0 == strncmp(logmsg, "sent", 4)) 
           sent = OUT;
        else if (0 == strncmp(logmsg, "received", 8))
                sent = IN;
        have_files[sent] = TRUE;
        cur->call[called].flow[sent].files++;
        if (NULL == (s = strchr(logmsg, ' ')))       /* point past keyword */
           continue;                                 /* nothing follows */
                                   /* we should be at the bytes column now*/
#if HAVE_TAYLOR_LOGGING
        cur->call[called].flow[sent].bytes += atol(++s);
#endif /* HAVE_TAYLOR_LOGGING */
#if HAVE_V2_LOGGING
        if (NULL == (s = strpbrk(s, "0123456789")))  /* point to # bytes */
           continue;
        cur->call[called].flow[sent].bytes += atol(s);
#endif /* HAVE_V2_LOGGING */
        if (NULL == (s = strchr(s, ' ')))          /* point past # of bytes */
           continue;
        if (NULL == (s = strpbrk(s, "0123456789"))) /* point to # of seconds */
           continue;
        cur->call[called].flow[sent].time += atof(s);

    }   /* end of while (fgets(logline...)) */

    if (stt != NULL && ! use_stdin && ! be_quiet && ! no_records)
    {  

#if HAVE_TAYLOR_LOGGING
         sscanf(dt_info,"%s%*c%[^.]",in_date,in_time);
#endif /* HAVE_TAYLOR_LOGGING */

#if HAVE_V2_LOGGING
         sscanf(dt_info,"%[^-]%*c%[1234567890:]",in_date,in_time);
#endif /* HAVE_V2_LOGGING */

       printf("  %10s %8s\n",in_date, in_time);
       p_done = FALSE;
    }
    if (Log != stdin)
    {
       if (0 != ferror(Log))
       {
          if (! be_quiet)
             printf("   %-14s data is incomplete; read error"," ");
          else
            fprintf(stderr,"%s (W) data is incomplete; read error on %s\n",
                                   Pgm_name,argv[1]);
       }
       else
       {
          if (! be_quiet && no_records)
             printf("   %-14s %10s\n",Filename, " is empty ");
       }         
     }
     fclose(Log);

    argc--;
    argv++;
  }  /* end of while (for (argv ....) */

  /*
   *   do we have *any* data ?
   */

  if (cur == NULL)
  {
     puts("\n(I) Sorry! No data is available for any requested report\n");
     exit(0);
  }

  /*
   *   truncate hostname, alloc the structure holding the totals and
   *   collect the totals data
   */

  for (cur = hosts; cur != NULL;cur = cur->next)
  {
      cur->Hostname[MAXDNAME] = '\0';
      if (cur->next == NULL)            /* last so will have to alloc totals */
      {
         cur->next = (struct Host_entry *)getmem(sizeof(*hosts));
         strcpy(cur->next->Hostname,"Totals");
         tot = cur->next;
         for (cur = hosts; cur != NULL; cur = cur->next)
         {
           if (cur->next != NULL)        /* don't count totals to totals */
           {
              tot->call[IN].flow[IN].bytes += cur->call[IN].flow[IN].bytes;
              tot->call[OUT].flow[IN].bytes += cur->call[OUT].flow[IN].bytes;
              tot->call[IN].flow[OUT].bytes  += cur->call[IN].flow[OUT].bytes;
              tot->call[OUT].flow[OUT].bytes += cur->call[OUT].flow[OUT].bytes;
              tot->call[IN].flow[IN].time  += cur->call[IN].flow[IN].time;
              tot->call[OUT].flow[IN].time += cur->call[OUT].flow[IN].time;
              tot->call[IN].flow[OUT].time  += cur->call[IN].flow[OUT].time;
              tot->call[OUT].flow[OUT].time += cur->call[OUT].flow[OUT].time;
              tot->call[IN].flow[IN].files  += cur->call[IN].flow[IN].files;
              tot->call[OUT].flow[IN].files += cur->call[OUT].flow[IN].files;
              tot->call[IN].flow[OUT].files  += cur->call[IN].flow[OUT].files;
              tot->call[OUT].flow[OUT].files += cur->call[OUT].flow[OUT].files;
              tot->call[OUT].succs += cur->call[OUT].succs; 
              tot->call[OUT].calls += cur->call[OUT].calls; 
              tot->call[OUT].connect_time += cur->call[OUT].connect_time;
              tot->call[IN].succs += cur->call[IN].succs; 
              tot->call[IN].calls += cur->call[IN].calls; 
              tot->call[IN].connect_time += cur->call[IN].connect_time;
           }
         }
         break;                   /* totals is last in Host_Entry */
     }
  }

  /*
   *                       ***********
   *                       * REPORTS *
   *                       ***********
   */

#if _DEBUG_
  putchar('\n');
#endif

  /* ------------------------------------------------------------------
   *
   * Summary report only when no other report except option -t is given
   *
   * I know, this code could be tightened (rbd)...
   * ------------------------------------------------------------------
   */

  if (  !(show_calls || show_files ||
          show_efficiency || show_commands || show_proto) || show_all)
  {
     if (have_calls || have_files[IN] || have_files[OUT])
     {
        char t1[32], t2[32], t3[32], t4[32], t5[32];
        long ib, ob, b, rf, sf;
        double it, ot, ir, or;

        hdr_done = FALSE;
        for (cur = hosts; cur != NULL; cur = cur->next)
        {
           ib = (cur->call[IN].flow[IN].bytes +
                cur->call[OUT].flow[IN].bytes);
           fmbytes(ib, t1);

           ob = (cur->call[IN].flow[OUT].bytes +
                cur->call[OUT].flow[OUT].bytes);
           fmbytes(ob, t2);

                 /* Don't print null-lines. */
           if (( b= ib+ob ) == 0 )
              continue;
                 /* Don't print the header twice. */
             if (! hdr_done)
             {
                hdrprt('s',0);            /* print the header line(s) */
                hdr_done = TRUE;
             }

             fmbytes(b, t3);

             it = cur->call[IN].flow[IN].time +
                  cur->call[OUT].flow[IN].time;
             fmtime(it, t4);

             ot = cur->call[IN].flow[OUT].time +
                  cur->call[OUT].flow[OUT].time;
             fmtime(ot, t5);

             rf = cur->call[IN].flow[IN].files +
                  cur->call[OUT].flow[IN].files;

             sf = cur->call[IN].flow[OUT].files +
                  cur->call[OUT].flow[OUT].files;

             ir = (it == 0.0) ? 0.0 : (ib / it);
             or = (ot == 0.0) ? 0.0 : (ob / ot);

             if (cur->next == NULL)            /* totals line reached ? */
                hdrprt('s',1);                 /* print the separator line */

             printf("%-8s %4d %4d %9s %9s %9s %9s %9s %5.0f %5.0f\n",
                   cur->Hostname, rf, sf,
                   t1, t2, t3, t4, t5,
                   ir, or);
        } 
        if (! hdr_done)
        {
            puts("\n(I) No data found to print Compact summary report");
        }
     }
     else
     {
        puts("\n(I) No data available for Compact summary report");
        --report;
     }
  }

  /* ------------------------------------------------------------------
   *                     Protocol statistics report
   * ------------------------------------------------------------------
   */

  if (show_proto || show_all)
  {
     if (have_proto)
     {
                        /* ---------------------  */
                        /* protocol packet report */
                        /* ---------------------  */

        char *type = NULL;
        hdr_done = FALSE;
        for (cur = hosts; cur != NULL; cur = cur->next)
        {
            type = cur->Hostname;
            if (cur->next == NULL)
            {
               if (hdr_done)
   puts("-------------------------------------------------------------------");
            cur->proto = t_prot;
            }
            for (prot = cur->proto; prot != NULL; prot = prot->next)
            {
                if (! hdr_done)
                {
                    hdrprt('p',0);            /* print the header line(s) */
                    hdr_done = TRUE;
                }
                printf("%-8s %3s  %4d %4d %5d %4d    %10d %7d %10d\n",
                                    type == NULL ? " ":cur->Hostname,
                                    prot->type,
                                    prot->pr_psizemin,
                                    prot->pr_psizemax,
                                    prot->pr_pwinmin,
                                    prot->pr_pwinmax,
                                    prot->pr_psent,
                                    prot->pr_present,
                                    prot->pr_preceived);
                type = NULL;
             }
         }
         if (! hdr_done)
            puts("\n(I) No data found to print Protocol packet report");

                        /* --------------------- */
                        /* protocol error report */
                        /* --------------------- */

        type = NULL;
        hdr_done = FALSE;
        if (t_prot != NULL)
        {
           for (cur = hosts; cur != NULL; cur = cur->next)
           {
               type = cur->Hostname;
               if (cur->next == NULL)
               {
                  if (hdr_done)
        puts("--------------------------------------------------------------");
               cur->proto = t_prot;
               }

               for (prot = cur->proto; prot != NULL; prot = prot->next)
               {
                   if ((prot->pr_eheader + prot->pr_echksum +
                      prot->pr_eorder + prot->pr_ereject) != 0)
                   {
                      if (! hdr_done)
                      {
                         hdrprt('p',1);       /* print the header line(s) */
                         hdr_done = TRUE;
                      }
                      printf("%-8s %3s  %11d %11d  %11d %11d\n",
                                    type == NULL ? " ":cur->Hostname,
                                    prot->type,
                                    prot->pr_eheader,
                                    prot->pr_echksum,
                                    prot->pr_eorder,
                                    prot->pr_ereject);
                      type = NULL;
                   } 
                }
            }
        }
        if (! hdr_done)
           puts("\n(I) No data found to print Protocol error report");
     }
     else
     {
        puts("\n(I) No data available for Protocol reports");
        --report;
     }
  }

  /* ------------------------------------------------------------------
   *                     Call statistics report
   * ------------------------------------------------------------------
   */

  if (show_calls || show_all)
  {
     if (have_calls)
     {
        char t1[32], t2[32];

        hdr_done = FALSE;
        for (cur = hosts; cur != NULL; cur = cur->next)
        {
            if (cur->next == NULL)
            {
               if (hdr_done)
                  hdrprt('c',1);                 /* print the separator line */
            }
            else
            {
                  /* Don't print null-lines on deatail lines */
               if ( cur->call[OUT].calls + cur->call[IN].calls == 0 )
                  continue;

                 /* Don't print the header twice. */
               if (! hdr_done)
               {
                   hdrprt('c',0);               /* print the header line(s) */
                   hdr_done = TRUE;
               }
            }
            if ( cur->call[OUT].calls > 0 || cur->next == NULL)
            {
               fmtime(cur->call[OUT].connect_time, t1);
               printf( "   %-8s %7d %7d %7d %9s",
                     cur->Hostname,
                     cur->call[OUT].succs,
                     cur->call[OUT].calls - cur->call[OUT].succs,
                     cur->call[OUT].calls,
                     t1 );
             }
             else
             {
                printf( "   %-42s", cur->Hostname );
             }
             if ( cur->call[IN].calls > 0 || cur->next == NULL )
             {
                fmtime(cur->call[IN].connect_time, t2);
                printf( " %7d %7d %7d %9s",
                       cur->call[IN].succs,
                       cur->call[IN].calls - cur->call[IN].succs,
                       cur->call[IN].calls,
                       t2 );
              }
              putchar('\n');
        }
        if (! hdr_done)
        {
            puts("\n(I) No data found to print Call statistics report");
        }
     }
     else
     {
        puts("\n(I) No data available for Call statistics report");
        --report;
     }
  }

  /* ------------------------------------------------------------------
   *                    File statistics report
   * ------------------------------------------------------------------
   */

  if (show_files || show_all)
  {
     if (have_files[IN] || have_files[OUT])
     {
        char t1[32], t2[32];
        double rate = 0, time = 0;
        int b = 0; 
        int lineOut = 0;

        hdr_done = FALSE;
        for (cur = hosts; cur != NULL; cur = cur->next)
        {
            lineOut = 0;
            for (sent= IN; sent <= OUT; ++sent)
            {  
                b    = cur->call[IN].flow[sent].bytes +
                       cur->call[OUT].flow[sent].bytes;
                time = cur->call[IN].flow[sent].time +
                       cur->call[OUT].flow[sent].time;

                   /* Don't print null-lines on detail lines. */
                if ( (b != 0 && time != 0.0) || cur->next == NULL)
                {
                      /* Don't print the header twice. */
                   if (! hdr_done)
                   {
                      hdrprt('f',0);          /* print the header line(s) */
                      hdr_done = TRUE;
                   }
                   fmbytes(b, t1);
                   rate = (cur->call[IN].flow[sent].bytes +
                          cur->call[OUT].flow[sent].bytes) / time;
                   fmtime((cur->call[IN].flow[sent].time +
                          cur->call[OUT].flow[sent].time), t2);

                   if (lineOut == 0)         /* first half not printed yet ? */
                   {
                      if (cur->next == NULL)       /* totals line ? */
                         hdrprt('f',1);          /* print the separator line */
                      printf("   %-8s", cur->Hostname);
                      if (sent == OUT)     /* can't happen whith totals line */
                         printf("%34s", " ");
                    }

                    printf(" %5d %11s %9s %5.0f",
                          cur->call[IN].flow[sent].files +
                          cur->call[OUT].flow[sent].files,
                          t1, t2, rate);
                    lineOut = 1;
                 }
            }    /* end:  for (sent ... ) */  
            if (lineOut)
                printf("\n");
        }    /* end:  for (cur= ... ) */
        if (! hdr_done)
        {
           puts("\n(I) No data found to print File statistics report");
        }
     }
     else
     {
        puts("\n(I) No data available for File statistics report");
        --report;
     }
  }

  /* ------------------------------------------------------------------
   *                       Efficiency report
   * ------------------------------------------------------------------
   */

  if (show_efficiency || show_all)
  {
     if (have_files[IN] || have_files[OUT])
     {
        char t1[32], t2[32], t3[32];
        double total, flow;

        hdr_done = FALSE;
        for (cur = hosts; cur != NULL; cur = cur->next)
        {
                 /* Don't print null-lines. */
            if ( 0 == cur->call[IN].flow[IN].files +
                      cur->call[IN].flow[OUT].files +
                      cur->call[OUT].flow[IN].files +
                      cur->call[OUT].flow[OUT].files ||
                 0.0 == (total= cur->call[IN].connect_time +
                        cur->call[OUT].connect_time))
            {
               continue;
            }

            if (! hdr_done)
            {
               hdrprt('e',0);                 /* print the header line(s) */
               hdr_done = TRUE;
            }

            flow = cur->call[IN].flow[IN].time + 
                   cur->call[IN].flow[OUT].time +
                   cur->call[OUT].flow[IN].time +
                   cur->call[OUT].flow[OUT].time;
             fmtime(total, t1);
             fmtime(flow, t2);
             fmtime(total-flow, t3);

            if (cur->next == NULL)
               hdrprt('e',1);                 /* print the separator line */

            printf("   %-8s %10s %10s %10s %7.2f\n",
                   cur->Hostname, t1, t2, t3,
            flow >= total ? 100.0: flow*100.0/total);
        }   /* end: for (cur= .. */
        if (! hdr_done)
        {
           puts("\n(I) No data found to print Efficiency report");
        }
     }
     else
     {
        puts("\n(I) No data available for Efficiency report");
        --report;
     }
  }

  /* ------------------------------------------------------------------
   *                   Command execution report
   * ------------------------------------------------------------------
   */

  if (show_commands || show_all)
  { 
     if (have_commands)
     {
        int ncmds, i, match;

        /* 
         *  layout the header line. The column's header is the command name
         */

        hdr_done = FALSE;
        for (ncmds= 0, cmd= t_cmds;
             cmd != NULL && ncmds <= MAXCOLS-1;
             ncmds++, cmd= cmd->next)
        {
            if (! hdr_done)
            {
               puts("\nCommand executions:");
               puts("-------------------");
               puts("   Name of ");
               fputs("   site    ", stdout);
               hdr_done = TRUE;
            }
            printf(" %7s", cmd->Commandname);
         }
         if (! hdr_done)
         {
            puts("\n(I) No data found to print Command execution report");
         }
         else
         {
           fputs("\n   --------", stdout);
           for (i= 0; i<ncmds; i++)
               fputs("  ------", stdout);
           putchar('\n');

        /* 
         *  print out the number of executions for each host/command
         */

           for (cur= hosts; cur != NULL; cur= cur->next)
           {
               if (cur->next == NULL)
                  break;

                 /* Don't print null-lines. */

              if (cur->cmds == NULL)
                 continue;

              printf("   %-8s", cur->Hostname);
              for (cmd= t_cmds; cmd != NULL; cmd= cmd->next)
              {
                  struct Execution_Command *ec;
                  match = FALSE;
                  for(ec= cur->cmds; ec != NULL; ec= ec->next)
                  {
                     if ( 0 == strcmp(cmd->Commandname, ec->Commandname) )
                     { 
                        printf(" %7d", ec->count);
                        match = TRUE;
                        break;
                     }
                   }
                   if (! match)
                      printf("%8s"," ");    /* blank out column */
               }
               putchar('\n');
            }

         /*
          *  print the totals line 
          */

            fputs("   --------", stdout);
            for (i= 0; i<ncmds; i++)
                fputs("--------", stdout);
            printf("\n   %-8s", cur->Hostname);
            for (cmd= t_cmds; cmd != NULL; cmd= cmd->next)
            {
                printf(" %7d", cmd->count);
            }
            putchar('\n');
        }
     }
     else
     {
        puts("\n(I) No data available for Command execution report");
        --report;
     }
  }
  if (report <= 0 )       /* any reports ? */
  {
     puts("\n(I) Sorry! No data is available for any requested report\n");
     exit(1);
  }

  puts("\n(I) End of reports\n");
  exit (0);
}  /* end of main */

  /* ------------------------------------------------------------------
   *                       * Functions *
   * ------------------------------------------------------------------
   */

  /* ------------------------------------------------------------------
   *                    display the help 
   * ------------------------------------------------------------------
   */

void usage()
{
  fprintf(stderr,"Usage uurate [-acdefhiptvx] [-s hostname] [-I config file] [logfile(s) ... logfile(s)]\n");
  fprintf(stderr,"where:\t-a\tPrint reports c,e,f,x\n");
  fprintf(stderr,"\t-c\tReport call statistics\n");
  fprintf(stderr,"\t-d\tPrint the name of the default config file\n");
  fprintf(stderr,"\t-e\tReport efficiency statistics\n");
  fprintf(stderr,"\t-f\tReport file transfer statistics\n");
  fprintf(stderr,"\t-h\tPrint this help\n");
  fprintf(stderr,"\t-i\tRead log info from standard input\n");
  fprintf(stderr,"\t-p\tReport protocol statistics\n");
  fprintf(stderr,"\t-t\tAll available reports plus compact summary report\n");
  fprintf(stderr,"\t-v\tPrint version number\n");
  fprintf(stderr,"\t-x\tReport command execution statistics\n");
  fprintf(stderr,"\t-s host\tReport activities involving HOST only\n");
  fprintf(stderr,"\t-I config Use config instead of standard config file\n");
  fprintf(stderr,"If no report options given, a compact summary report is printed.\n");
  fprintf(stderr,"log files should be given as pairs that is Log/Stats ... .\n");
  fprintf(stderr,"If neither -i nor logfiles given, those names found in config will be used\n");

  exit (1);
}

 /* ------------------------------------------------------------------
  *                    getmem - get some memory
  * ------------------------------------------------------------------
  */

static pointer *getmem(n)
                    unsigned n;
{
  pointer *p;

  if( NULL== (p= calloc(1, n)) )
    {
      fprintf(stderr,"\a%s (C) %s\n",Pgm_name, "out of memory\n");
      exit (8);
    }
  return p;
}

  /* ------------------------------------------------------------------
   *             inc_cmd - increment command count
   * ------------------------------------------------------------------
   */

static void inc_cmd(cmds, name)
                    struct Execution_Command **cmds;
                    char *name;
{
  int cnt = 0;
  struct Execution_Command *cmd, *ec;

  for (ec = cmd = *cmds; cmd != NULL; ec= cmd, cmd= cmd->next, cnt++)
      if ( (0 == strcmp(cmd->Commandname, name)) ||
           (0 == strcmp(cmd->Commandname, "Misc.")) )
         break;
  if (cmd == NULL)
  {
     cmd= (struct Execution_Command *)getmem(sizeof(*cmd));
     if (cnt <= MAXCOLS-1)   /* first col prints site name therefore < max-1 */
     {
        strcpy(cmd->Commandname, name);
        if (*cmds == NULL)
           ec = *cmds = cmd;
        else
           ec->next= cmd;
     }
     else
     {
        strcpy(ec->Commandname, "Misc.");  /* reached high-water-mark */
        cmd = ec;                          /* backtrack */
     }
  }
  cmd->count++;
}


  /* ------------------------------------------------------------------
   *             prot_sum - collect protocol data
   * ------------------------------------------------------------------
   */

   struct Protocol_Summary *
   prot_sum(proto, ptype, ind)
                    struct Protocol_Summary **proto;
                    char *ptype;
                    int ind;
{
  int cnt = 0;
  int i1, i2, i3 = 0;
  struct Protocol_Summary *cur, *first;

  for (first = cur = *proto; cur != NULL; first= cur, cur= cur->next, cnt++)
  {
      if ( (0 == strncmp(cur->type, ptype,strlen(cur->type))))
         break;
  }
  if (cur == NULL)
  {
     cur= (struct Protocol_Summary *)getmem(sizeof(*cur));
     sscanf(ptype,"%[^\' ]3",cur->type);
     if (*proto == NULL)
        first = *proto = cur;
     else
        first->next= cur;
  }
  if (NULL == (ptype = strchr(ptype, ' ')))
         return (NULL);
  cur->pr_cnt++;
  have_proto = TRUE;
  ++ptype;
  switch(ind)
  {
     case 1:              /* used protocol line */
  /*
   * uucp-1.04 format: .... packet size ssss window ww)
   * uucp-1.05 format: .... remote packet/window ssss/ww local ssss/ww)
   *           (the remote packet/window will be used!)
   */

          i1 = i2 = 0;    /* reset */

          if (NULL == (strchr(ptype, '/')))
             sscanf(ptype,"%*s %*s %d %*s %d",&i1,&i2);
          else
             sscanf(ptype,"%*s %*s %d/%d",&i1,&i2);

          if (i1 > cur->pr_psizemax)
             cur->pr_psizemax = i1;
          if (i1 < cur->pr_psizemin || cur->pr_psizemin == 0)
             cur->pr_psizemin = i1;

          if (i2 > cur->pr_pwinmax)
             cur->pr_pwinmax = i2;
          if (i2 < cur->pr_pwinmin || cur->pr_pwinmin == 0)
             cur->pr_pwinmin = i2;
          break;
     case 2:              /* protocol statistics line */
          i1 = i2 = i3 = 0;    /* reset */
          sscanf(ptype,"%*s %*s %d%*c %*s %d%*c %*s %d",&i1,&i2,&i3);
          cur->pr_psent += i1;
          cur->pr_present += i2;
          cur->pr_preceived += i3;
          break;
     default:
          break;
  }
  return (cur);
}
  /* ------------------------------------------------------------------
   *           fmtime() - Format time in hours & minutes & seconds;
   * ------------------------------------------------------------------
   */

static void fmtime(dsec, buf)
                  double dsec;
                  char *buf;
{
  long hrs, min, lsec;

  if( dsec <= 0 )
    {
      strcpy(buf, "0" );
      return;
    }
  lsec = fmod(dsec+0.5, 60L);        /* round to the next full second */
  hrs = dsec / 3600L;
  min = ((long)dsec / 60L) % 60L;
  if (hrs == 0)
     if (min == 0)
       sprintf(buf,"%6s%2ld"," ",lsec);
     else
       sprintf(buf,"%3s%2ld:%02ld"," ",min,lsec);
  else
    sprintf(buf,"%2ld:%02ld:%02ld",hrs,min,lsec);

}

  /* ------------------------------------------------------------------
   *                 fmbytes - Format size in bytes
   * ------------------------------------------------------------------
   */

static void fmbytes(n, buf)
                   unsigned long n;
                   char *buf;
{
  if ( n == 0 )
  {
     strcpy( buf, "0.0" );
     return;
  }
  sprintf(buf, "%.1f", (double)n / 1000.0);    /* Display in Kilobytes */
}


  /* ------------------------------------------------------------------
   *                 chk_config - Read the config file
   *    check on keywords: logfile and statfile. When found override
   *    the corresponding default
   * ------------------------------------------------------------------
   */

int chk_config(char *T_conf,int be_quiet, int type)
{
   FILE *Conf;
   char keywrd[9];
   char name[MAXPATHLEN+1];
   char *pos1, *pos2;
   int i = 0;
   int logf = FALSE;
   int statf = FALSE;

   if ((Conf = fopen(T_conf, "r")) == NULL)
   {
      if (! be_quiet)
      {
         puts("   Could not open config");
         if (type == 0)
         {
            puts("   The run will be aborted\n");
            return (8);
         }
      }
      else
      {
         fprintf(stderr,"%s (E) %s %s \n",Pgm_name,
                                     "could not open config:",
                                      T_conf);
         if (type != 0)
            fprintf(stderr,"%s (W) defaults used for all files\n",
                                               Pgm_name);
         else
         {
            fprintf(stderr,"%s (C) ended due to errors\n",
                                               Pgm_name);
            return (8);
         } 
      }
   }
   else
   {
      while (fgets(logline, sizeof(logline), Conf))
      {
        if (logline[0] == '#')
           continue;
        sscanf(logline,"%8s %s",keywrd,name);
        if (0 == strncmp(keywrd,"logfile",7))
        {
           pos1 = pos2 = name;
           for (i=0;(i<=MAXPATHLEN && *pos1 != '\0');pos1++,pos2++,i++)
           {
               if (*pos1 == '#')     /* name immed followed by comment */
                  break;
               if (*pos1 == '\\')    /* quoted comment (filename has #) */
               {
                  ++pos1;               /* skip escape char */
                  if (*pos1 != '#')     /* continuation ? */
                  {
                     puts("   Config error:");
                     puts("   Found filename continuation; bailing out\n");
                     exit (8);
                  }
               }
               *pos2 = *pos1;        /* move char */
           }
           *pos2 = '\0';             /* terminate string */
           Tlog   = (char *)getmem(strlen(name)+1);
           strcpy(Tlog,name);
           if (! be_quiet)
              printf("   logfile used:        %s\n",Tlog);
           logf = TRUE;
           if  (statf)                /* statsfile still to come ? */
               break;                 /* no finished */
           continue;
        }

        if (0 == strncmp(keywrd,"statfile",8))
        {
           pos1 = pos2 = name;
           for (i=0;(i<=MAXPATHLEN && *pos1 != '\0');pos1++,pos2++,i++)
           {
               if (*pos1 == '#')     /* name immed followed by comment */
                  break;
               if (*pos1 == '\\')    /* quoted comment (filename has #) */
               {
                  ++pos1;               /* skip escape char */
                  if (*pos1 != '#')     /* continuation ? */
                  {
                     puts("   Config error:");
                     puts("   Found filename continuation; bailing out\n");
                     exit (8);
                  }
               }
               *pos2 = *pos1;        /* move char */
           }
           *pos2 = '\0';             /* terminate string */
           Tstat   = (char *)getmem(strlen(name)+1);
           strcpy(Tstat,name);
           if (! be_quiet)
              printf("   statfile used:       %s\n",Tstat);
           statf = TRUE;
           if  (logf)                 /* logfile still to come ? */
               break;                 /* no finished */
           continue;
        }
      }
      fclose(Conf);
   }

   if (! be_quiet)
   {
      if (! logf)
         puts("   logfile used:        - default -");
      if (! statf)
         puts("   statfile used:       - default -");
   }

return 0;
}


  /* ------------------------------------------------------------------
   *   hdrprt - Print Header/Trailer lines (constant data)
   * ------------------------------------------------------------------
   */

static void hdrprt(char head, int bot)
{
  switch(head)
  {
     case('s'):                   /* standard summary report */
          if (bot == 0)
          {
             puts("\nCompact summary:");
             puts("----------------");
             puts("\
Name of  + Files + +------- Bytes/1000 --------+ +------ Time -----+ + Avg CPS +\n\
site       in  out   inbound  outbound     total   inbound  outbound    in   out\n\
-------- ---- ---- --------- --------- --------- --------- --------- ----- -----");
          }
          else
             puts("\
--------------------------------------------------------------------------------");
          break;


     case('f'):                   /* file statistic report */
          if (bot == 0)
          {
             puts("\nFile statistics:");
             puts("----------------");
             puts("   Name of  +----------- Inbound -----------+ +---------- Outbound -----------+");
            puts("   site     files  Bytes/1000  xfr time B/sec files  Bytes/1000  xfr time B/sec");
            puts("   -------- ----- ----------- --------- ----- ----- ----------- --------- -----");
          }
          else
            puts("\
   ----------------------------------------------------------------------------");
          break;


     case('c'):                   /* calls statistic report */
          if (bot == 0)
          {
             puts("\nCall statistics:");
             puts("----------------");
             puts("   Name of   +------- Outbound Calls -------+  +-------- Inbound Calls  ------+");
             puts("   site       succ.  failed   total      time   succ.  failed   total      time");
            puts("   --------  ------  ------  ------ ---------  ------  ------  ------ ---------");
          }
          else
            puts("\
   ----------------------------------------------------------------------------");
          break;


     case('e'):                   /* efficiency statistic report */
          if (bot == 0)
          {
             puts("\nEfficiency:");
             puts("-----------");
             puts("   Name of   +------ Times inbound/outbound -------+");
             puts("   site      connected   xfr time   overhead  eff. %");
             puts("   --------  ---------  ---------  ---------  ------");
          }
          else
            puts("   -------------------------------------------------");
          break;

     case('i'):                   /* Environment information */
          if (bot == 0)
          {
             puts("\nEnvironment Information:");
             puts("------------------------");
             printf("   Default config:      %s\n",D_conf == NULL ?
                                                   noConf:D_conf);
             printf("   Default logfile:     %s\n",Tlog);
             printf("   Default statfile:    %s\n\n",Tstat);
          }
          break;

     case('d'):                   /* Date/time coverage */
          if (bot == 0)
          {
             puts("\n   Date coverage of input files:");
             puts("   Name of        +----- Start -----+  +------ End ------+");
             puts("   file                 date     time        date     time");
             puts("   --------       ---------- --------  ---------- --------");
          }
          break;

     case('p'):                   /* Protocol stats */
          if (bot == 0)
          {
             puts("\nProtocol packet report:");
             puts("-----------------------");
             puts("          +------- protocol -----+   +--------- Packets ----------+");
             puts("Name of         packet     window ");
             puts("site      typ  min  max   min  max          sent  resent   received");
            puts("--------  --- ---- ----  ---- ----   ----------- ------- ----------");
          }
          else
          {
             puts("\nProtocol error report:");
             puts("----------------------");
             puts("Name of   +----------------- Error Types --------------------+");
             puts("site      typ      header    checksum        order  rem-reject");
             puts("--------  --- -----------  ----------  -----------  ----------");
          }
          break;

     default:
          if (bot == 0)
          {
             puts("\nNo header for this report defined:");
          }
          else
            puts("  ");
         break;
   }
}
