/*
 * Copyright (c) 1997-2003 Erez Zadok
 * Copyright (c) 1990 Jan-Simon Pendry
 * Copyright (c) 1990 Imperial College of Science, Technology & Medicine
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Jan-Simon Pendry at Imperial College, London.
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
 *    must display the following acknowledgment:
 *      This product includes software developed by the University of
 *      California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *      %W% (Berkeley) %G%
 *
 * $Id: amq.c,v 1.7.2.8 2002/12/27 22:44:45 ezk Exp $
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * Automounter query tool
 */

#ifndef lint
char copyright[] = "\
@(#)Copyright (c) 1997-2003 Erez Zadok\n\
@(#)Copyright (c) 1990 Jan-Simon Pendry\n\
@(#)Copyright (c) 1990 Imperial College of Science, Technology & Medicine\n\
@(#)Copyright (c) 1990 The Regents of the University of California.\n\
@(#)All rights reserved.\n";
#if __GNUC__ < 2
static char rcsid[] = "$Id: amq.c,v 1.7.2.8 2002/12/27 22:44:45 ezk Exp $";
static char sccsid[] = "%W% (Berkeley) %G%";
#endif /* __GNUC__ < 2 */
#endif /* not lint */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif /* HAVE_CONFIG_H */
#include <am_defs.h>
#include <amq.h>

/* locals */
static int flush_flag;
static int minfo_flag;
static int getpid_flag;
static int unmount_flag;
static int stats_flag;
static int getvers_flag;
static int amd_program_number = AMQ_PROGRAM;
static int use_tcp_flag, use_udp_flag;
static int getpwd_flag;
static char *debug_opts;
static char *amq_logfile;
static char *xlog_optstr;
static char localhost[] = "localhost";
static char *def_server = localhost;

/* externals */
extern int optind;
extern char *optarg;

/* structures */
enum show_opt {
  Full, Stats, Calc, Short, ShowDone
};


/*
 * If (e) is Calc then just calculate the sizes
 * Otherwise display the mount node on stdout
 */
static void
show_mti(amq_mount_tree *mt, enum show_opt e, int *mwid, int *dwid, int *twid)
{
  switch (e) {
  case Calc:
    {
      int mw = strlen(mt->mt_mountinfo);
      int dw = strlen(mt->mt_directory);
      int tw = strlen(mt->mt_type);
      if (mw > *mwid)
	*mwid = mw;
      if (dw > *dwid)
	*dwid = dw;
      if (tw > *twid)
	*twid = tw;
    }
  break;

  case Full:
    {
      struct tm *tp = localtime((time_t *) &mt->mt_mounttime);
      printf("%-*.*s %-*.*s %-*.*s %s\n\t%-5d %-7d %-6d %-7d %-7d %-6d %02d/%02d/%02d %02d:%02d:%02d\n",
	     *dwid, *dwid,
	     *mt->mt_directory ? mt->mt_directory : "/",	/* XXX */
	     *twid, *twid,
	     mt->mt_type,
	     *mwid, *mwid,
	     mt->mt_mountinfo,
	     mt->mt_mountpoint,

	     mt->mt_mountuid,
	     mt->mt_getattr,
	     mt->mt_lookup,
	     mt->mt_readdir,
	     mt->mt_readlink,
	     mt->mt_statfs,

	     tp->tm_year > 99 ? tp->tm_year - 100 : tp->tm_year,
	     tp->tm_mon + 1, tp->tm_mday,
	     tp->tm_hour, tp->tm_min, tp->tm_sec);
    }
  break;

  case Stats:
    {
      struct tm *tp = localtime((time_t *) &mt->mt_mounttime);
      printf("%-*.*s %-5d %-7d %-6d %-7d %-7d %-6d %02d/%02d/%02d %02d:%02d:%02d\n",
	     *dwid, *dwid,
	     *mt->mt_directory ? mt->mt_directory : "/",	/* XXX */

	     mt->mt_mountuid,
	     mt->mt_getattr,
	     mt->mt_lookup,
	     mt->mt_readdir,
	     mt->mt_readlink,
	     mt->mt_statfs,

	     tp->tm_year > 99 ? tp->tm_year - 100 : tp->tm_year,
	     tp->tm_mon + 1, tp->tm_mday,
	     tp->tm_hour, tp->tm_min, tp->tm_sec);
    }
  break;

  case Short:
    {
      printf("%-*.*s %-*.*s %-*.*s %s\n",
	     *dwid, *dwid,
	     *mt->mt_directory ? mt->mt_directory : "/",
	     *twid, *twid,
	     mt->mt_type,
	     *mwid, *mwid,
	     mt->mt_mountinfo,
	     mt->mt_mountpoint);
    }
  break;

  default:
    break;
  }
}


/*
 * Display a pwd data
 */
static void
show_pwd(amq_mount_tree *mt, char *path, int *flag)
{
  int len;

  while (mt) {
    len = strlen(mt->mt_mountpoint);
    if (NSTREQ(path, mt->mt_mountpoint, len) &&
	!STREQ(mt->mt_directory, mt->mt_mountpoint)) {
      char buf[MAXPATHLEN+1];
      strcpy(buf, mt->mt_directory);
      strcat(buf, &path[len]);
      strcpy(path, buf);
      *flag = 1;
    }
    show_pwd(mt->mt_next, path, flag);
    mt = mt->mt_child;
  }
}


/*
 * Display a mount tree.
 */
static void
show_mt(amq_mount_tree *mt, enum show_opt e, int *mwid, int *dwid, int *pwid)
{
  while (mt) {
    show_mti(mt, e, mwid, dwid, pwid);
    show_mt(mt->mt_next, e, mwid, dwid, pwid);
    mt = mt->mt_child;
  }
}


static void
show_mi(amq_mount_info_list *ml, enum show_opt e, int *mwid, int *dwid, int *twid)
{
  int i;

  switch (e) {

  case Calc:
    {
      for (i = 0; i < ml->amq_mount_info_list_len; i++) {
	amq_mount_info *mi = &ml->amq_mount_info_list_val[i];
	int mw = strlen(mi->mi_mountinfo);
	int dw = strlen(mi->mi_mountpt);
	int tw = strlen(mi->mi_type);
	if (mw > *mwid)
	  *mwid = mw;
	if (dw > *dwid)
	  *dwid = dw;
	if (tw > *twid)
	  *twid = tw;
      }
    }
  break;

  case Full:
    {
      for (i = 0; i < ml->amq_mount_info_list_len; i++) {
	amq_mount_info *mi = &ml->amq_mount_info_list_val[i];
	printf("%-*.*s %-*.*s %-*.*s %-3d %s is %s",
	       *mwid, *mwid, mi->mi_mountinfo,
	       *dwid, *dwid, mi->mi_mountpt,
	       *twid, *twid, mi->mi_type,
	       mi->mi_refc, mi->mi_fserver,
	       mi->mi_up > 0 ? "up" :
	       mi->mi_up < 0 ? "starting" : "down");
	if (mi->mi_error > 0) {
	  if (mi->mi_error < sys_nerr)
#ifdef HAVE_STRERROR
	    printf(" (%s)", strerror(mi->mi_error));
#else /* not HAVE_STRERROR */
	    printf(" (%s)", sys_errlist[mi->mi_error]);
#endif /* not HAVE_STRERROR */
	  else
	    printf(" (Error %d)", mi->mi_error);
	} else if (mi->mi_error < 0) {
	  fputs(" (in progress)", stdout);
	}
	fputc('\n', stdout);
      }
    }
  break;

  default:
    break;
  }
}


/*
 * Display general mount statistics
 */
static void
show_ms(amq_mount_stats *ms)
{
  printf("\
requests  stale     mount     mount     unmount\n\
deferred  fhandles  ok        failed    failed\n\
%-9d %-9d %-9d %-9d %-9d\n",
	 ms->as_drops, ms->as_stale, ms->as_mok, ms->as_merr, ms->as_uerr);
}


#if defined(HAVE_CLUSTER_H) && defined(HAVE_CNODEID) && defined(HAVE_GETCCENT)
static char *
cluster_server(void)
{
  struct cct_entry *cp;

  if (cnodeid() == 0) {
    /*
     * Not clustered
     */
    return def_server;
  }
  while (cp = getccent())
    if (cp->cnode_type == 'r')
      return cp->cnode_name;

  return def_server;
}
#endif /* defined(HAVE_CLUSTER_H) && defined(HAVE_CNODEID) && defined(HAVE_GETCCENT) */


/*
 * MAIN
 */
int
main(int argc, char *argv[])
{
  int opt_ch;
  int errs = 0;
  char *server;
  struct sockaddr_in server_addr;
  CLIENT *clnt = NULL;
  struct hostent *hp;
  int nodefault = 0;
  struct timeval tv;
  char *progname = NULL;

  /*
   * Compute program name
   */
  if (argv[0]) {
    progname = strrchr(argv[0], '/');
    if (progname && progname[1])
      progname++;
    else
      progname = argv[0];
  }
  if (!progname)
    progname = "amq";
  am_set_progname(progname);

  /*
   * Parse arguments
   */
  while ((opt_ch = getopt(argc, argv, "Hfh:l:msuvx:D:pP:TUw")) != -1)
    switch (opt_ch) {
    case 'H':
      goto show_usage;
      break;

    case 'f':
      flush_flag = 1;
      nodefault = 1;
      break;

    case 'h':
      def_server = optarg;
      break;

    case 'l':
      amq_logfile = optarg;
      nodefault = 1;
      break;

    case 'm':
      minfo_flag = 1;
      nodefault = 1;
      break;

    case 'p':
      getpid_flag = 1;
      nodefault = 1;
      break;

    case 's':
      stats_flag = 1;
      nodefault = 1;
      break;

    case 'u':
      unmount_flag = 1;
      nodefault = 1;
      break;

    case 'v':
      getvers_flag = 1;
      nodefault = 1;
      break;

    case 'x':
      xlog_optstr = optarg;
      nodefault = 1;
      break;

    case 'D':
      debug_opts = optarg;
      nodefault = 1;
      break;

    case 'P':
      amd_program_number = atoi(optarg);
      break;

    case 'T':
      use_tcp_flag = 1;
      break;

    case 'U':
      use_udp_flag = 1;
      break;

    case 'w':
      getpwd_flag = 1;
      break;

    default:
      errs = 1;
      break;
    }

  if (optind == argc) {
    if (unmount_flag)
      errs = 1;
  }
  if (errs) {
  show_usage:
    fprintf(stderr, "\
Usage: %s [-fmpsvwHTU] [-h hostname] [-l log_file|\"syslog\"]\n\
\t[-x log_options] [-D debug_options]\n\
\t[-P program_number] [[-u] directory ...]\n",
	    am_get_progname()
    );
    exit(1);
  }


  /* set use_udp and use_tcp flags both to on if none are defined */
  if (!use_tcp_flag && !use_udp_flag)
    use_tcp_flag = use_udp_flag = 1;

#if defined(HAVE_CLUSTER_H) && defined(HAVE_CNODEID) && defined(HAVE_GETCCENT)
  /*
   * Figure out root server of cluster
   */
  if (def_server == localhost)
    server = cluster_server();
  else
#endif /* defined(HAVE_CLUSTER_H) && defined(HAVE_CNODEID) && defined(HAVE_GETCCENT) */
    server = def_server;

  /*
   * Get address of server
   */
  if ((hp = gethostbyname(server)) == 0 && !STREQ(server, localhost)) {
    fprintf(stderr, "%s: Can't get address of %s\n",
	    am_get_progname(), server);
    exit(1);
  }
  memset(&server_addr, 0, sizeof server_addr);
  server_addr.sin_family = AF_INET;
  if (hp) {
    memmove((voidp) &server_addr.sin_addr, (voidp) hp->h_addr,
	    sizeof(server_addr.sin_addr));
  } else {
    /* fake "localhost" */
    server_addr.sin_addr.s_addr = htonl(0x7f000001);
  }

  /*
   * Create RPC endpoint
   */
  tv.tv_sec = 5;		/* 5 seconds for timeout or per retry */
  tv.tv_usec = 0;

  if (use_tcp_flag)	/* try tcp first */
    clnt = clnt_create(server, amd_program_number, AMQ_VERSION, "tcp");
  if (!clnt && use_udp_flag) {	/* try udp next */
    clnt = clnt_create(server, amd_program_number, AMQ_VERSION, "udp");
    /* if ok, set timeout (valid for connectionless transports only) */
    if (clnt)
      clnt_control(clnt, CLSET_RETRY_TIMEOUT, (char *) &tv);
  }
  if (!clnt) {
    fprintf(stderr, "%s: ", am_get_progname());
    clnt_pcreateerror(server);
    exit(1);
  }

  /*
   * Control debugging
   */
  if (debug_opts) {
    int *rc;
    amq_setopt opt;
    opt.as_opt = AMOPT_DEBUG;
    opt.as_str = debug_opts;
    rc = amqproc_setopt_1(&opt, clnt);
    if (rc && *rc < 0) {
      fprintf(stderr, "%s: daemon not compiled for debug\n",
	      am_get_progname());
      errs = 1;
    } else if (!rc || *rc > 0) {
      fprintf(stderr, "%s: debug setting for \"%s\" failed\n",
	      am_get_progname(), debug_opts);
      errs = 1;
    }
  }

  /*
   * Control logging
   */
  if (xlog_optstr) {
    int *rc;
    amq_setopt opt;
    opt.as_opt = AMOPT_XLOG;
    opt.as_str = xlog_optstr;
    rc = amqproc_setopt_1(&opt, clnt);
    if (!rc || *rc) {
      fprintf(stderr, "%s: setting log level to \"%s\" failed\n",
	      am_get_progname(), xlog_optstr);
      errs = 1;
    }
  }

  /*
   * Control log file
   */
  if (amq_logfile) {
    int *rc;
    amq_setopt opt;
    opt.as_opt = AMOPT_LOGFILE;
    opt.as_str = amq_logfile;
    rc = amqproc_setopt_1(&opt, clnt);
    if (!rc || *rc) {
      fprintf(stderr, "%s: setting logfile to \"%s\" failed\n",
	      am_get_progname(), amq_logfile);
      errs = 1;
    }
  }

  /*
   * Flush map cache
   */
  if (flush_flag) {
    int *rc;
    amq_setopt opt;
    opt.as_opt = AMOPT_FLUSHMAPC;
    opt.as_str = "";
    rc = amqproc_setopt_1(&opt, clnt);
    if (!rc || *rc) {
      fprintf(stderr, "%s: amd on %s cannot flush the map cache\n",
	      am_get_progname(), server);
      errs = 1;
    }
  }

  /*
   * getpwd info
   */
  if (getpwd_flag) {
    char path[MAXPATHLEN+1];
    char *wd = getcwd(path, MAXPATHLEN+1);
    amq_mount_tree_list *mlp = amqproc_export_1((voidp) 0, clnt);
    amq_mount_tree_p mt;
    int i, flag;

    if (!wd) {
      perror("getcwd");
      exit(1);
    }
    for (i = 0; mlp && i < mlp->amq_mount_tree_list_len; i++) {
      mt = mlp->amq_mount_tree_list_val[i];
      while (1) {
	flag = 0;
	show_pwd(mt, path, &flag);
	if (!flag) {
	  printf("%s\n", path);
	  break;
	}
      }
    }
    exit(0);
  }

  /*
   * Mount info
   */
  if (minfo_flag) {
    int dummy;
    amq_mount_info_list *ml = amqproc_getmntfs_1(&dummy, clnt);
    if (ml) {
      int mwid = 0, dwid = 0, twid = 0;
      show_mi(ml, Calc, &mwid, &dwid, &twid);
      mwid++;
      dwid++;
      twid++;
      show_mi(ml, Full, &mwid, &dwid, &twid);

    } else {
      fprintf(stderr, "%s: amd on %s cannot provide mount info\n",
	      am_get_progname(), server);
    }
  }

  /*
   * Get Version
   */
  if (getvers_flag) {
    amq_string *spp = amqproc_getvers_1((voidp) 0, clnt);
    if (spp && *spp) {
      fputs(*spp, stdout);
      XFREE(*spp);
    } else {
      fprintf(stderr, "%s: failed to get version information\n",
	      am_get_progname());
      errs = 1;
    }
  }

  /*
   * Get PID of amd
   */
  if (getpid_flag) {
    int *ip = amqproc_getpid_1((voidp) 0, clnt);
    if (ip && *ip) {
      printf("%d\n", *ip);
    } else {
      fprintf(stderr, "%s: failed to get PID of amd\n", am_get_progname());
      errs = 1;
    }
  }

  /*
   * Apply required operation to all remaining arguments
   */
  if (optind < argc) {
    do {
      char *fs = argv[optind++];
      if (unmount_flag) {
	/*
	 * Unmount request
	 */
	amqproc_umnt_1(&fs, clnt);
      } else {
	/*
	 * Stats request
	 */
	amq_mount_tree_p *mtp = amqproc_mnttree_1(&fs, clnt);
	if (mtp) {
	  amq_mount_tree *mt = *mtp;
	  if (mt) {
	    int mwid = 0, dwid = 0, twid = 0;
	    show_mt(mt, Calc, &mwid, &dwid, &twid);
	    mwid++;
	    dwid++, twid++;
	    printf("%-*.*s Uid   Getattr Lookup RdDir   RdLnk   Statfs Mounted@\n",
		   dwid, dwid, "What");
	    show_mt(mt, Stats, &mwid, &dwid, &twid);
	  } else {
	    fprintf(stderr, "%s: %s not automounted\n", am_get_progname(), fs);
	  }
	  xdr_pri_free((XDRPROC_T_TYPE) xdr_amq_mount_tree_p, (caddr_t) mtp);
	} else {
	  fprintf(stderr, "%s: ", am_get_progname());
	  clnt_perror(clnt, server);
	  errs = 1;
	}
      }
    } while (optind < argc);

  } else if (unmount_flag) {
    goto show_usage;

  } else if (stats_flag) {
    amq_mount_stats *ms = amqproc_stats_1((voidp) 0, clnt);
    if (ms) {
      show_ms(ms);
    } else {
      fprintf(stderr, "%s: ", am_get_progname());
      clnt_perror(clnt, server);
      errs = 1;
    }

  } else if (!nodefault) {
    amq_mount_tree_list *mlp = amqproc_export_1((voidp) 0, clnt);
    if (mlp) {
      enum show_opt e = Calc;
      int mwid = 0, dwid = 0, pwid = 0;
      while (e != ShowDone) {
	int i;
	for (i = 0; i < mlp->amq_mount_tree_list_len; i++) {
	  show_mt(mlp->amq_mount_tree_list_val[i],
		  e, &mwid, &dwid, &pwid);
	}
	mwid++;
	dwid++, pwid++;
	if (e == Calc)
	  e = Short;
	else if (e == Short)
	  e = ShowDone;
      }

    } else {
      fprintf(stderr, "%s: ", am_get_progname());
      clnt_perror(clnt, server);
      errs = 1;
    }
  }
  exit(errs);
  return errs; /* should never reach here */
}
