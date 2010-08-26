/*
 * qperf - main.
 * Measure socket and RDMA performance.
 *
 * Copyright (c) 2002-2009 Johann George.  All rights reserved.
 * Copyright (c) 2006-2009 QLogic Corporation.  All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#define _GNU_SOURCE
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <sched.h>
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <sys/times.h>
#include <sys/select.h>
#include <sys/utsname.h>
#include "qperf.h"


/*
 * Configurable parameters.  If your change makes this version of qperf
 * incompatible with previous versions (usually a change to the Req structure),
 * increment VER_MIN and set VER_INC to 0.  Otherwise, just increment VER_INC.
 * VER_MAJ is reserved for major changes.
 */
#define VER_MAJ 0                       /* Major version */
#define VER_MIN 4                       /* Minor version */
#define VER_INC 6                       /* Incremental version */
#define LISTENQ 5                       /* Size of listen queue */
#define BUFSIZE 1024                    /* Size of buffers */


/*
 * Default parameter values.
 */
#define DEF_TIME        2               /* Test duration */
#define DEF_TIMEOUT     5               /* Timeout */
#define DEF_PRECISION   3               /* Precision displayed */
#define DEF_LISTEN_PORT 19765           /* Listen port */


/*
 * Option list.
 */
typedef struct OPTION {
    char       *name;                   /* Name of option */
    char       *type;                   /* Type */
    int         arg1;                   /* First argument */
    int         arg2;                   /* Second argument */
} OPTION;


/*
 * Used to loop through a range of values.
 */
typedef struct LOOP {
    struct LOOP  *next;                 /* Pointer to next loop */
    OPTION       *option;               /* Loop variable */
    long          init;                 /* Initial value */
    long          last;                 /* Last value */
    long          incr;                 /* Increment */
    int           mult;                 /* If set, multiply, otherwise add */
} LOOP;


/*
 * Parameter information.
 */
typedef struct PAR_INFO {
    PAR_INDEX   index;                  /* Index into parameter table */
    int         type;                   /* Type */
    void       *ptr;                    /* Pointer to value */
    char       *name;                   /* Option name */
    int         set;                    /* Parameter has been set */
    int         used;                   /* Parameter has been used */
    int         inuse;                  /* Parameter is in use */
} PAR_INFO;


/*
 * Parameter name association.
 */
typedef struct PAR_NAME {
    char       *name;                   /* Name */
    PAR_INDEX   loc_i;                  /* Local index */
    PAR_INDEX   rem_i;                  /* Remote index */
} PAR_NAME;


/*
 * A simple mapping between two strings.
 */
typedef struct DICT {
    char       *str1;                   /* String 1 */
    char       *str2;                   /* String 2 */
} DICT;


/*
 * Test prototype.
 */
typedef struct TEST {
    char       *name;                   /* Test name */
    void      (*client)(void);          /* Client function */
    void      (*server)(void);          /* Server function */
} TEST;


/*
 * Used to save output data for formatting.
 */
typedef struct SHOW {
    char    *pref;                      /* Name prefix */
    char    *name;                      /* Name */
    char    *data;                      /* Data */
    char    *unit;                      /* Unit */
    char    *altn;                      /* Alternative value */
} SHOW;


/*
 * Configuration information.
 */
typedef struct CONF {
    char        node[STRSIZE];          /* Node */
    char        cpu[STRSIZE];           /* CPU */
    char        os[STRSIZE];            /* Operating System */
    char        qperf[STRSIZE];         /* Qperf version */
} CONF;


/*
 * Function prototypes.
 */
static void      add_ustat(USTAT *l, USTAT *r);
static long      arg_long(char ***argvp);
static long      arg_size(char ***argvp);
static char     *arg_strn(char ***argvp);
static long      arg_time(char ***argvp);
static void      calc_node(RESN *resn, STAT *stat);
static void      calc_results(void);
static void      client(TEST *test);
static int       cmpsub(char *s2, char *s1);
static char     *commify(char *data);
static void      dec_req_data(REQ *host);
static void      dec_req_version(REQ *host);
static void      dec_stat(STAT *host);
static void      dec_ustat(USTAT *host);
static void      do_args(char *args[]);
static void      do_loop(LOOP *loop, TEST *test);
static void      do_option(OPTION *option, char ***argvp);
static void      enc_req(REQ *host);
static void      enc_stat(STAT *host);
static void      enc_ustat(USTAT *host);
static TEST     *find_test(char *name);
static OPTION   *find_option(char *name);
static void      get_conf(CONF *conf);
static void      get_cpu(CONF *conf);
static void      get_times(CLOCK timex[T_N]);
static void      initialize(void);
static void      init_lstat(void);
static char     *loop_arg(char **pp);
static int       nice_1024(char *pref, char *name, long long value);
static PAR_INFO *par_info(PAR_INDEX index);
static PAR_INFO *par_set(char *name, PAR_INDEX index);
static int       par_isset(PAR_INDEX index);
static void      parse_loop(char ***argvp);
static void      place_any(char *pref, char *name, char *unit, char *data,
                           char *altn);
static void      place_show(void);
static void      place_val(char *pref, char *name, char *unit, double value);
static void      remotefd_close(void);
static void      remotefd_setup(void);
static void      run_client_conf(void);
static void      run_client_quit(void);
static void      run_server_conf(void);
static void      run_server_quit(void);
static void      server(void);
static void      server_listen(void);
static int       server_recv_request(void);
static void      set_affinity(void);
static void      set_signals(void);
static void      show_debug(void);
static void      show_info(MEASURE measure);
static void      show_rest(void);
static void      show_used(void);
static void      sig_alrm(int signo, siginfo_t *siginfo, void *ucontext);
static void      sig_quit(int signo, siginfo_t *siginfo, void *ucontext);
static void      sig_urg(int signo, siginfo_t *siginfo, void *ucontext);
static char     *skip_colon(char *s);
static void      start_test_timer(int seconds);
static long      str_size(char *arg, char *str);
static void      strncopy(char *d, char *s, int n);
static char     *two_args(char ***argvp);
static int       verbose(int type, double value);
static void      version_error(void);
static void      view_band(int type, char *pref, char *name, double value);
static void      view_cost(int type, char *pref, char *name, double value);
static void      view_cpus(int type, char *pref, char *name, double value);
static void      view_rate(int type, char *pref, char *name, double value);
static void      view_long(int type, char *pref, char *name, long long value);
static void      view_size(int type, char *pref, char *name, long long value);
static void      view_strn(int type, char *pref, char *name, char *value);
static void      view_time(int type, char *pref, char *name, double value);


/*
 * Configurable variables.
 */
static int  ListenPort      = DEF_LISTEN_PORT;
static int  Precision       = DEF_PRECISION;
static int  ServerWait      = DEF_TIMEOUT;
static int  UseBitsPerSec   = 0;


/*
 * Static variables.
 */
static REQ      RReq;
static STAT     IStat;
static int      ListenFD;
static LOOP    *Loops;
static int      ProcStatFD;
static STAT     RStat;
static int      ShowIndex;
static SHOW     ShowTable[256];
static int      UnifyUnits;
static int      UnifyNodes;
static int      VerboseConf;
static int      VerboseStat;
static int      VerboseTime;
static int      VerboseUsed;


/*
 * Global variables.
 */
RES          Res;
REQ          Req;
STAT         LStat;
char        *TestName;
char        *ServerName;
SS           ServerAddr;
int          ServerAddrLen;
int          RemoteFD;
int          Debug;
volatile int Finished;


/*
 * Parameter names.  This is used to print out the names of the parameters that
 * have been set.
 */
PAR_NAME ParName[] ={
    { "access_recv",    L_ACCESS_RECV,    R_ACCESS_RECV   },
    { "affinity",       L_AFFINITY,       R_AFFINITY      },
    { "alt_port",       L_ALT_PORT,       R_ALT_PORT      },
    { "flip",           L_FLIP,           R_FLIP          },
    { "id",             L_ID,             R_ID            },
    { "msg_size",       L_MSG_SIZE,       R_MSG_SIZE      },
    { "mtu_size",       L_MTU_SIZE,       R_MTU_SIZE      },
    { "no_msgs",        L_NO_MSGS,        R_NO_MSGS       },
    { "poll_mode",      L_POLL_MODE,      R_POLL_MODE     },
    { "port",           L_PORT,           R_PORT          },
    { "rd_atomic",      L_RD_ATOMIC,      R_RD_ATOMIC     },
    { "service_level",  L_SL,             R_SL            },
    { "sock_buf_size",  L_SOCK_BUF_SIZE,  R_SOCK_BUF_SIZE },
    { "src_path_bits",  L_SRC_PATH_BITS,  R_SRC_PATH_BITS },
    { "time",           L_TIME,           R_TIME          },
    { "timeout",        L_TIMEOUT,        R_TIMEOUT       },
    { "use_cm",         L_USE_CM,         R_USE_CM        },
};


/*
 * Parameters.  These must be listed in the same order as the indices are
 * defined.
 */
PAR_INFO ParInfo[P_N] ={
    { P_NULL,                                       },
    { L_ACCESS_RECV,    'l',  &Req.access_recv      },
    { R_ACCESS_RECV,    'l',  &RReq.access_recv     },
    { L_AFFINITY,       'l',  &Req.affinity         },
    { R_AFFINITY,       'l',  &RReq.affinity        },
    { L_ALT_PORT,       'l',  &Req.alt_port         },
    { R_ALT_PORT,       'l',  &RReq.alt_port        },
    { L_FLIP,           'l',  &Req.flip             },
    { R_FLIP,           'l',  &RReq.flip            },
    { L_ID,             'p',  &Req.id               },
    { R_ID,             'p',  &RReq.id              },
    { L_MSG_SIZE,       's',  &Req.msg_size         },
    { R_MSG_SIZE,       's',  &RReq.msg_size        },
    { L_MTU_SIZE,       's',  &Req.mtu_size         },
    { R_MTU_SIZE,       's',  &RReq.mtu_size        },
    { L_NO_MSGS,        'l',  &Req.no_msgs          },
    { R_NO_MSGS,        'l',  &RReq.no_msgs         },
    { L_POLL_MODE,      'l',  &Req.poll_mode        },
    { R_POLL_MODE,      'l',  &RReq.poll_mode       },
    { L_PORT,           'l',  &Req.port             },
    { R_PORT,           'l',  &RReq.port            },
    { L_RD_ATOMIC,      'l',  &Req.rd_atomic        },
    { R_RD_ATOMIC,      'l',  &RReq.rd_atomic       },
    { L_SL,             'l',  &Req.sl               },
    { R_SL,             'l',  &RReq.sl              },
    { L_SOCK_BUF_SIZE,  's',  &Req.sock_buf_size    },
    { R_SOCK_BUF_SIZE,  's',  &RReq.sock_buf_size   },
    { L_SRC_PATH_BITS,  's',  &Req.src_path_bits    },
    { R_SRC_PATH_BITS,  's',  &RReq.src_path_bits   },
    { L_STATIC_RATE,    'p',  &Req.static_rate      },
    { R_STATIC_RATE,    'p',  &RReq.static_rate     },
    { L_TIME,           't',  &Req.time             },
    { R_TIME,           't',  &RReq.time            },
    { L_TIMEOUT,        't',  &Req.timeout          },
    { R_TIMEOUT,        't',  &RReq.timeout         },
    { L_USE_CM,         'l',  &Req.use_cm           },
    { R_USE_CM,         'l',  &RReq.use_cm          },
};


/*
 * Renamed options.  First is old, second is new.
 */
DICT Renamed[] = {
    /* -a becomes -ca (--cpu_affinity) */
    { "--affinity",         "--cpu_affinity"        },
    {   "-a",               "-ca"                   },
    {  "--loc_affinity",    "--loc_cpu_affinity"    },
    {   "-la",              "-lca"                  },
    {  "--rem_affinity",    "--rem_cpu_affinity"    },
    {   "-ra",              "-rca"                  },
    /* -r becomes -sr (--static_rate) */
    { "--rate",             "--static_rate"         },
    {   "-r",               "-sr"                   },
    {  "--loc_rate",        "--loc_static_rate"     },
    {   "-lr",              "-lsr"                  },
    {  "--rem_rate",        "--rem_static_rate"     },
    {   "-rr",              "-rsr"                  },
    /* -p becomes -ip (--ip_port) */
    { "--port",             "--ip_port"             },
    {   "-p",               "-ip"                   },
    /* -P becomes -cp (--cq_poll) */
    { "--poll",             "--cq_poll"             },
    {   "-P",               "-cp"                   },
    {  "--loc_poll",        "--loc_cq_poll"         },
    {   "-lP",              "-lcp"                  },
    {  "--rem_poll",        "--rem_cq_poll"         },
    {   "-rP",              "-rcp"                  },
    /* -R becomes -nr (--rd_atomic) */
    {   "-R",               "-nr"                   },
    {   "-lR",              "-lnr"                  },
    {   "-rR",              "-rnr"                  },
    /* -T becomes -to (--timeout) */
    {   "-T",               "-to"                   },
    {   "-lT",              "-lto"                  },
    {   "-rT",              "-rto"                  },
    /* -S becomes -sb (--sock_buf_size) */
    {   "-S",               "-sb"                   },
    {   "-lS",              "-lsb"                  },
    {   "-rS",              "-rsb"                  },
    /* -W becomes -ws (--wait_server) */
    { "--wait",             "--wait_server"         },
    {   "-W",               "-ws"                   },
    /* verbose options */
    {   "-vC",              "-vvc",                 },
    {   "-vS",              "-vvs",                 },
    {   "-vT",              "-vvt",                 },
    {   "-vU",              "-vvu",                 },
    /* options that are on */
    {   "-aro",             "-ar1"                  },
    {   "-cmo",             "-cm1"                  },
    {   "-fo",              "-f1"                   },
    {   "-cpo",             "-cp1"                  },
    {   "-lcpo",            "-lcp1"                 },
    {   "-rcpo",            "-rcp1"                 },
    /* miscellaneous */
    {   "-Ar",              "-ar"                   },
    {   "-M",               "-mt"                   },
    {   "-u",               "-uu",                  },
};


/*
 * Options.  The type field (2nd column) is used by do_option.  If it begins
 * with a S, it is a valid server option.  If it begins with a X, it is
 * obsolete and will eventually go away.
 */
OPTION Options[] ={
    { "--access_recv",        "int",   L_ACCESS_RECV,   R_ACCESS_RECV   },
    {   "-ar",                "int",   L_ACCESS_RECV,   R_ACCESS_RECV   },
    {   "-ar1",               "set1",  L_ACCESS_RECV,   R_ACCESS_RECV   },
    { "--alt_port",           "int",   L_ALT_PORT,      R_ALT_PORT      },
    {   "-ap",                "int",   L_ALT_PORT,      R_ALT_PORT      },
    {  "--loc_alt_port",      "int",   L_ALT_PORT,                      },
    {   "-lap",               "int",   L_ALT_PORT,                      },
    {  "--rem_alt_port",      "int",   R_ALT_PORT                       },
    {   "-rap",               "int",   R_ALT_PORT                       },
    { "--cpu_affinity",       "int",   L_AFFINITY,      R_AFFINITY      },
    {   "-ca",                "int",   L_AFFINITY,      R_AFFINITY      },
    {  "--loc_cpu_affinity",  "int",   L_AFFINITY,                      },
    {   "-lca",               "int",   L_AFFINITY,                      },
    {  "--rem_cpu_affinity",  "int",   R_AFFINITY                       },
    {   "-rca",               "int",   R_AFFINITY                       },
    { "--debug",              "Sdebug",                                 },
    {   "-D",                 "Sdebug",                                 },
    { "--flip",               "int",   L_FLIP,          R_FLIP          },
    {   "-f",                 "int",   L_FLIP,          R_FLIP          },
    {   "-f1",                "set1",  L_FLIP,          R_FLIP          },
    { "--help",               "help"                                    }, 
    {   "-h",                 "help"                                    }, 
    { "--host",               "host",                                   },
    {   "-H",                 "host",                                   },
    { "--id",                 "str",   L_ID,            R_ID            },
    {   "-i",                 "str",   L_ID,            R_ID            },
    {  "--loc_id",            "str",   L_ID,                            },
    {   "-li",                "str",   L_ID,                            },
    {  "--rem_id",            "str",   R_ID                             },
    {   "-ri",                "str",   R_ID                             },
    { "--listen_port",        "Slp",                                    },
    {   "-lp",                "Slp",                                    },
    { "--loop",               "loop",                                   },
    {   "-oo",                "loop",                                   },
    { "--msg_size",           "size",  L_MSG_SIZE,      R_MSG_SIZE      },
    {   "-m",                 "size",  L_MSG_SIZE,      R_MSG_SIZE      },
    { "--mtu_size",           "size",  L_MTU_SIZE,      R_MTU_SIZE      },
    {   "-mt",                "size",  L_MTU_SIZE,      R_MTU_SIZE      },
    { "--no_msgs",            "int",   L_NO_MSGS,       R_NO_MSGS       },
    {   "-n",                 "int",   L_NO_MSGS,       R_NO_MSGS       },
    { "--cq_poll",            "int",   L_POLL_MODE,     R_POLL_MODE     },
    {  "-cp",                 "int",   L_POLL_MODE,     R_POLL_MODE     },
    {   "-cp1",               "set1",  L_POLL_MODE,     R_POLL_MODE     },
    {  "--loc_cq_poll",       "int",   L_POLL_MODE,                     },
    {   "-lcp",               "int",   L_POLL_MODE,                     },
    {   "-lcp1",              "set1",  L_POLL_MODE                      },
    {  "--rem_cq_poll",       "int",   R_POLL_MODE                      },
    {   "-rcp",               "int",   R_POLL_MODE                      },
    {   "-rcp1",              "set1",  R_POLL_MODE                      },
    { "--ip_port",            "int",   L_PORT,          R_PORT          },
    {   "-ip",                "int",   L_PORT,          R_PORT          },
    { "--precision",          "precision",                              },
    {   "-e",                 "precision",                              },
    { "--rd_atomic",          "int",   L_RD_ATOMIC,     R_RD_ATOMIC     },
    {   "-nr",                "int",   L_RD_ATOMIC,     R_RD_ATOMIC     },
    {  "--loc_rd_atomic",     "int",   L_RD_ATOMIC,                     },
    {   "-lnr",               "int",   L_RD_ATOMIC,                     },
    {  "--rem_rd_atomic",     "int",   R_RD_ATOMIC                      },
    {   "-rnr",               "int",   R_RD_ATOMIC                      },
    { "--service_level",      "sl",    L_SL,            R_SL            },
    {   "-sl",                "sl",    L_SL,            R_SL            },
    {  "--loc_service_level", "sl",    L_SL                             },
    {   "-lsl",               "sl",    L_SL                             },
    {  "--rem_service_level", "sl",    R_SL                             },
    {   "-rsl",               "sl",    R_SL                             },
    { "--sock_buf_size",      "size",  L_SOCK_BUF_SIZE, R_SOCK_BUF_SIZE },
    {   "-sb",                "size",  L_SOCK_BUF_SIZE, R_SOCK_BUF_SIZE },
    {  "--loc_sock_buf_size", "size",  L_SOCK_BUF_SIZE                  },
    {   "-lsb",               "size",  L_SOCK_BUF_SIZE                  },
    {  "--rem_sock_buf_size", "size",  R_SOCK_BUF_SIZE                  },
    {   "-rsb",               "size",  R_SOCK_BUF_SIZE                  },
    { "--src_path_bits",      "size",  L_SRC_PATH_BITS, R_SRC_PATH_BITS },
    {   "-sp",                "size",  L_SRC_PATH_BITS, R_SRC_PATH_BITS },
    {  "--loc_src_path_bits", "size",  L_SRC_PATH_BITS                  },
    {   "-lsp",               "size",  L_SRC_PATH_BITS                  },
    {  "--rem_src_path_bits", "size",  R_SRC_PATH_BITS                  },
    {   "-rsp",               "size",  R_SRC_PATH_BITS                  },
    { "--static_rate",        "str",   L_STATIC_RATE,   R_STATIC_RATE   },
    {   "-sr",                "str",   L_STATIC_RATE,   R_STATIC_RATE   },
    {  "--loc_static_rate",   "str",   L_STATIC_RATE                    },
    {   "-lsr",               "str",   L_STATIC_RATE                    },
    {  "--rem_static_rate",   "str",   R_STATIC_RATE                    },
    {   "-rsr",               "str",   R_STATIC_RATE                    },
    { "--time",               "time",  L_TIME,          R_TIME          },
    {   "-t",                 "time",  L_TIME,          R_TIME          },
    { "--timeout",            "time",  L_TIMEOUT,       R_TIMEOUT       },
    {   "-to",                "time",  L_TIMEOUT,       R_TIMEOUT       },
    {  "--loc_timeout",       "Stime", L_TIMEOUT                        },
    {   "-lto",               "Stime", L_TIMEOUT                        },
    {  "--rem_timeout",       "time",  R_TIMEOUT                        },
    {   "-rto",               "time",  R_TIMEOUT                        },
    { "--unify_nodes",        "un",                                     },
    {   "-un",                "un",                                     },
    { "--unify_units",        "uu",                                     },
    {   "-uu",                "uu",                                     },
    { "--use_bits_per_sec",   "ub",                                     },
    {   "-ub",                "ub",                                     },
    { "--use_cm",             "int",   L_USE_CM,        R_USE_CM        },
    {   "-cm",                "int",   L_USE_CM,        R_USE_CM        },
    {   "-cm1",               "set1",  L_USE_CM,        R_USE_CM        },
    { "--verbose",            "v",                                      },
    {   "-v",                 "v",                                      },
    { "--verbose_conf",       "vc",                                     },
    {   "-vc",                "vc",                                     },
    { "--verbose_stat",       "vs",                                     },
    {   "-vs",                "vs",                                     },
    { "--verbose_time",       "vt",                                     },
    {   "-vt",                "vt",                                     },
    { "--verbose_used",       "vu",                                     },
    {   "-vu",                "vu",                                     },
    { "--verbose_more",       "vv",                                     },
    {   "-vv",                "vv",                                     },
    { "--verbose_more_conf",  "vvc",                                    },
    {   "-vvc",               "vvc",                                    },
    { "--verbose_more_stat",  "vvs",                                    },
    {   "-vvs",               "vvs",                                    },
    { "--verbose_more_time",  "vvt",                                    },
    {   "-vvt",               "vvt",                                    },
    { "--verbose_more_used",  "vvu",                                    },
    {   "-vvu",               "vvu",                                    },
    { "--version",            "version",                                },
    {   "-V",                 "version",                                },
    { "--wait_server",        "wait",                                   },
    {   "-ws",                "wait",                                   },
};


/*
 * Tests.
 */
#define test(n) { #n, run_client_##n, run_server_##n }
TEST Tests[] ={
    test(conf),
    test(quit),
    test(rds_bw),
    test(rds_lat),
    test(sctp_bw),
    test(sctp_lat),
    test(sdp_bw),
    test(sdp_lat),
    test(tcp_bw),
    test(tcp_lat),
    test(udp_bw),
    test(udp_lat),
#ifdef RDMA
    test(rc_bi_bw),
    test(rc_bw),
    test(rc_compare_swap_mr),
    test(rc_fetch_add_mr),
    test(rc_lat),
    test(rc_rdma_read_bw),
    test(rc_rdma_read_lat),
    test(rc_rdma_write_bw),
    test(rc_rdma_write_lat),
    test(rc_rdma_write_poll_lat),
    test(uc_bi_bw),
    test(uc_bw),
    test(uc_lat),
    test(uc_rdma_write_bw),
    test(uc_rdma_write_lat),
    test(uc_rdma_write_poll_lat),
    test(ud_bi_bw),
    test(ud_bw),
    test(ud_lat),
    test(ver_rc_compare_swap),
    test(ver_rc_fetch_add),
    test(xrc_bi_bw),
    test(xrc_bw),
    test(xrc_lat),
#endif
};


int
main(int argc, char *argv[])
{
    initialize();
    set_signals();
    do_args(&argv[1]);
    return 0;
}


/*
 * Initialize variables.
 */
static void
initialize(void)
{
    int i;

    RemoteFD = -1;
    for (i = 0; i < P_N; ++i)
        if (ParInfo[i].index != i)
            error(BUG, "initialize: ParInfo: out of order: %d", i);
    ProcStatFD = open("/proc/stat", 0);
    if (ProcStatFD < 0)
        error(SYS, "cannot open /proc/stat");
    IStat.no_cpus = sysconf(_SC_NPROCESSORS_ONLN);
    IStat.no_ticks = sysconf(_SC_CLK_TCK);
}


/*
 * Look for a colon and skip past it and any spaces.
 */
static char *
skip_colon(char *s)
{
    for (;;) {
        int c = *s++;

        if (c == ':')
            break;
        if (c == '\0')
            return 0;
    }
    while (*s == ' ')
        s++;
    return s;
}


/*
 * A case insensitive string compare.  s2 must at least contain all of s1 but
 * can be longer.
 */
static int
cmpsub(char *s2, char *s1)
{
    for (;;) {
        int c1 = *s1++;
        int c2 = *s2++;

        if (c1 == '\0')
            return 1;
        if (c2 == '\0')
            return 0;
        if (tolower(c1) != tolower(c2))
            return 0;
    }
}


/*
 * Set up signal handlers.
 */
static void
set_signals(void)
{
    struct sigaction act ={
        .sa_flags = SA_SIGINFO
    };

    act.sa_sigaction = sig_alrm;
    sigaction(SIGALRM, &act, 0);
    sigaction(SIGPIPE, &act, 0);

    act.sa_sigaction = sig_quit;
    sigaction(SIGQUIT, &act, 0);

    act.sa_sigaction = sig_urg;
    sigaction(SIGURG, &act, 0);
}


/*
 * Note that time is up.
 */
static void
sig_alrm(int signo, siginfo_t *siginfo, void *ucontext)
{
    set_finished();
}


/*
 * Our child sends us a quit when it wishes us to exit.
 */
static void
sig_quit(int signo, siginfo_t *siginfo, void *ucontext)
{
    exit(0);
}


/*
 * Called when a TCP/IP out-of-band message is received.
 */
static void
sig_urg(int signo, siginfo_t *siginfo, void *ucontext)
{
    urgent();
}


/*
 * Parse arguments.
 */
static void
do_args(char *args[])
{
    int isClient = 0;
    int testSpecified = 0;

    while (*args) {
        char *arg = *args;
        if (arg[0] == '-') {
            OPTION *option = find_option(arg);
            if (!option)
                error(0, "%s: bad option; try: qperf --help options", arg);
            if (option->type[0] != 'S')
                isClient = 1;
            do_option(option, &args);
        } else {
            isClient = 1;
            if (!ServerName)
                ServerName = arg;
            else {
                TEST *test = find_test(arg);

                if (!test)
                    error(0, "%s: bad test; try: qperf --help tests", arg);
                do_loop(Loops, test);
                testSpecified = 1;
            }
            ++args;
        }
    }

    if (!isClient)
        server();
    else if (!testSpecified) {
        if (!ServerName)
            error(0, "you used a client-only option but did not specify the "
                      "server name.\nDo you want to be a client or server?");
        if (find_test(ServerName))
            error(0, "must specify host name first; try: qperf --help");
        error(0, "must specify a test type; try: qperf --help");
    }
}


/*
 * Loop through a series of tests.
 */
static void
do_loop(LOOP *loop, TEST *test)
{
    if (!loop)
        client(test);
    else {
        long l = loop->init;

        while (l <= loop->last) {
            char   buf[64];
            char  *args[2] = {loop->option->name, buf};
            char **argv = args;

            snprintf(buf, sizeof(buf), "%ld", l);
            do_option(loop->option, &argv);
            do_loop(loop->next, test);
            if (loop->mult)
                l *= loop->incr;
            else
                l += loop->incr;
        }
    }
}


/*
 * Given the name of an option, find it.
 */
static OPTION *
find_option(char *name)
{
    int n;
    DICT *d;
    OPTION *p;

    n = cardof(Renamed);
    d = Renamed;
    for (; n--; ++d) {
        if (streq(name, d->str1)) {
            char *msg = "warning: obsolete option: %s; use %s instead";
            error(RET, msg, name, d->str2);
            name = d->str2;
            break;
        }
    }

    n = cardof(Options);
    p = Options;
    for (; n--; ++p)
        if (streq(name, p->name))
            return p;
    return 0;
}


/*
 * Given the name of a test, find it.
 */
static TEST *
find_test(char *name)
{
    int n = cardof(Tests);
    TEST *p = Tests;

    for (; n--; ++p)
        if (streq(name, p->name))
            return p;
    return 0;
}


/*
 * Handle options.
 */
static void
do_option(OPTION *option, char ***argvp)
{
    char *t = option->type;

    if (*t == 'S')
        ++t;

    if (streq(t, "debug")) {
        Debug = 1;
        *argvp += 1;
    } else if (streq(t, "help")) {
        /* Help */
        char **usage;
        char *category = (*argvp)[1];

        if (!category)
            category = "main";
        for (usage = Usage; *usage; usage += 2)
            if (streq(*usage, category))
                break;
        if (!*usage) {
            error(0,
                "cannot find help category %s; try: qperf --help categories",
                                                                    category);
        }
        printf("%s", usage[1]);
        exit(0);
    } else if (streq(t, "host")) {
        ServerName = arg_strn(argvp);
    } else if (streq(t, "int")) {
        long v = arg_long(argvp);
        setp_u32(option->name, option->arg1, v);
        setp_u32(option->name, option->arg2, v);
    } else if (streq(t, "loop")) {
        parse_loop(argvp);
    } else if (streq(t, "lp")) {
        ListenPort = arg_long(argvp);
    } else if (streq(t, "precision")) {
        Precision = arg_long(argvp);
    } else if (streq(t, "set1")) {
        setp_u32(option->name, option->arg1, 1);
        setp_u32(option->name, option->arg2, 1);
        *argvp += 1;
    } else if (streq(t, "size")) {
        long v = arg_size(argvp);
        setp_u32(option->name, option->arg1, v);
        setp_u32(option->name, option->arg2, v);
    } else if (streq(t, "sl")) {
        long v = arg_long(argvp);
        if (v < 0 || v > 15)
            error(0, "service level must be between 0 and 15: %d given", v);
        setp_u32(option->name, option->arg1, v);
        setp_u32(option->name, option->arg2, v);
    } else if (streq(t, "str")) {
        char *s = arg_strn(argvp);
        setp_str(option->name, option->arg1, s);
        setp_str(option->name, option->arg2, s);
    } else if (streq(t, "time")) {
        long v = arg_time(argvp);
        setp_u32(option->name, option->arg1, v);
        setp_u32(option->name, option->arg2, v);
    } else if (streq(t, "ub")) {
        UseBitsPerSec = 1;
        *argvp += 1;
    } else if (streq(t, "un")) {
        UnifyNodes = 1;
        *argvp += 1;
    } else if (streq(t, "uu")) {
        UnifyUnits = 1;
        *argvp += 1;
    } else if (streq(t, "v")) {
        if (VerboseConf < 1)
            VerboseConf = 1;
        if (VerboseStat < 1)
            VerboseStat = 1;
        if (VerboseTime < 1)
            VerboseTime = 1;
        if (VerboseUsed < 1)
            VerboseUsed = 1;
        *argvp += 1;
    } else if (streq(t, "vc")) {
        VerboseConf = 1;
        *argvp += 1;
    } else if (streq(t, "version")) {
        printf("qperf %d.%d.%d\n", VER_MAJ, VER_MIN, VER_INC);
        exit(0);
    } else if (streq(t, "vs")) {
        VerboseStat = 1;
        *argvp += 1;
    } else if (streq(t, "vt")) {
        VerboseTime = 1;
        *argvp += 1;
    } else if (streq(t, "vu")) {
        VerboseUsed = 1;
        *argvp += 1;
    } else if (streq(t, "vv")) {
        VerboseConf = 2;
        VerboseStat = 2;
        VerboseTime = 2;
        VerboseUsed = 2;
        *argvp += 1;
    } else if (streq(t, "vvc")) {
        VerboseConf = 2;
        *argvp += 1;
    } else if (streq(t, "vvs")) {
        VerboseStat = 2;
        *argvp += 1;
    } else if (streq(t, "vvt")) {
        VerboseTime = 2;
        *argvp += 1;
    } else if (streq(t, "vvu")) {
        VerboseUsed = 2;
        *argvp += 1;
    } else if (streq(t, "wait")) {
        ServerWait = arg_time(argvp);
    } else
        error(BUG, "do_option: unknown type: %s", t);
}


/*
 * Parse a loop option.
 */
static void
parse_loop(char ***argvp)
{
    char *opt  = **argvp;
    char *s    = two_args(argvp);
    char *name = loop_arg(&s);
    char *init = loop_arg(&s);
    char *last = loop_arg(&s);
    char *incr = loop_arg(&s);
    LOOP *loop = qmalloc(sizeof(LOOP));

    memset(loop, 0, sizeof(*loop));

    /* Parse variable name */
    {
        int n = cardof(Options);
        OPTION *p = Options;

        if (!name)
            name = "msg_size";
        for (;;) {
            char *s = p->name;

            if (n-- == 0)
                error(0, "%s: %s: no such variable", opt, name);
            if (*s++ != '-')
                continue;
            if (*s == '-')
                s++;
            if (streq(name, s))
                break;
            p++;
        }
        loop->option = p;
    }

    /* Parse increment */
    if (!incr)
        loop->incr = 0;
    else {
        if (incr[0] == '*') {
            incr++;
            loop->mult = 1;
        }
        loop->incr = str_size(incr, opt);
        if (loop->incr < 1)
            error(0, "%s: %s: increment must be positive", opt, incr);
    }

    /* Parse initial value */
    if (init)
        loop->init = str_size(init, opt);
    else
        loop->init = loop->mult ? 1 : 0;

    /* Parse last value */
    if (!last)
        error(0, "%s: must specify limit", opt);
    loop->last = str_size(last, opt);

    /* Insert into loop list */
    if (!Loops)
        Loops = loop;
    else {
        LOOP *l = Loops;

        while (l->next)
            l = l->next;
        l->next = loop;
    }
}


/*
 * Given a string consisting of arguments separated by colons, return the next
 * argument and prepare for scanning the next one.
 */
static char *
loop_arg(char **pp)
{
    char *a = *pp;
    char *p = a;

    while (*p) {
        if (*p == ':') {
            *p = '\0';
            *pp = p + 1;
            break;
        }
        ++p;
    }
    return a[0] ? a : 0;
}


/*
 * Ensure that two arguments exist.
 */
static char *
two_args(char ***argvp)
{
    char **argv = *argvp;

    if (!argv[1])
        error(0, "%s: missing argument", argv[0]);
    *argvp += 2;
    return argv[1];
}


/*
 * Return the value of a long argument.  It must be non-negative.
 */
static long
arg_long(char ***argvp)
{
    char **argv = *argvp;
    char *p;
    long l;

    if (!argv[1])
        error(0, "missing argument to %s", argv[0]);
    l = strtol(argv[1], &p, 10);
    if (p[0] != '\0')
        error(0, "bad argument: %s", argv[1]);
    if (l < 0)
        error(0, "%s requires a non-negative number", argv[0]);
    *argvp += 2;
    return l;
}


/*
 * Return the value of a size argument.
 */
static long
arg_size(char ***argvp)
{
    long l;
    char **argv = *argvp;

    *argvp += 2;
    if (!argv[1])
        error(0, "missing argument to %s", argv[0]);
    l = str_size(argv[1], argv[0]);
    if (l < 0)
        error(0, "%s requires a non-negative number", argv[0]);
    return l;
}


/*
 * Scan a size argument from a string.
 */
static long
str_size(char *str, char *arg)
{
    char *p;
    long m = 1;
    long double d = strtold(str, &p);

    if (p[0] == '\0')
        m = 1;
    else if (streq(p, "kb") || streq(p, "k"))
        m = 1000;
    else if (streq(p, "mb") || streq(p, "m"))
        m = 1000 * 1000;
    else if (streq(p, "gb") || streq(p, "g"))
        m = 1000 * 1000 * 1000;
    else if (streq(p, "kib") || streq(p, "K"))
        m = 1024;
    else if (streq(p, "mib") || streq(p, "M"))
        m = 1024 * 1024;
    else if (streq(p, "gib") || streq(p, "G"))
        m = 1024 * 1024 * 1024;
    else
        error(0, "%s: bad size: %s", arg, str);

    return d * m;
}


/*
 * Return the value of a string argument.
 */
static char *
arg_strn(char ***argvp)
{
    char **argv = *argvp;

    if (!argv[1])
        error(0, "missing argument to %s", argv[0]);
    *argvp += 2;
    return argv[1];
}


/*
 * Return the value of a size argument.
 */
static long
arg_time(char ***argvp)
{
    char *p;
    long double d;
    long l = 0;
    char **argv = *argvp;

    if (!argv[1])
        error(0, "missing argument to %s", argv[0]);

    d = strtold(argv[1], &p);
    if (d < 0)
        error(0, "%s requires a non-negative number", argv[0]);

    if (p[0] == '\0')
        l = (long)d;
    else {
        int u = *p;
        if (p[1] != '\0')
            error(0, "bad argument: %s", argv[1]);
        if (u == 's' || u == 'S')
            l = (long)d;
        else if (u == 'm' || u == 'M')
            l = (long)(d * (60));
        else if (u == 'h' || u == 'H')
            l = (long)(d * (60 * 60));
        else if (u == 'd' || u == 'D')
            l = (long)(d * (60 * 60 * 24));
        else
            error(0, "bad argument: %s", argv[1]);
    }

    *argvp += 2;
    return l;
}


/*
 * Set a value stored in a 32 bit value without letting anyone know we set it.
 */
void
setv_u32(PAR_INDEX index, uint32_t l)
{
    PAR_INFO *p = par_info(index);
    *((uint32_t *)p->ptr) = l;
}


/*
 * Set an option stored in a 32 bit value.
 */
void
setp_u32(char *name, PAR_INDEX index, uint32_t l)
{
    PAR_INFO *p = par_set(name, index);

    if (!p)
        return;
    *((uint32_t *)p->ptr) = l;
}


/*
 * Set an option stored in a string vector.
 */
void
setp_str(char *name, PAR_INDEX index, char *s)
{
    PAR_INFO *p = par_set(name, index);

    if (!p)
        return;
    if (strlen(s) >= STRSIZE)
        error(0, "%s: too long", s);
    strcpy(p->ptr, s);
}


/*
 * Note a parameter as being used.
 */
void
par_use(PAR_INDEX index)
{
    PAR_INFO *p = par_info(index);

    p->used = 1;
    p->inuse = 1;
}


/*
 * Set the PAR_INFO.name value.
 */
static PAR_INFO *
par_set(char *name, PAR_INDEX index)
{
    PAR_INFO *p = par_info(index);

    if (index == P_NULL)
        return 0;
    if (name) {
        p->name = name;
        p->set = 1;
    } else {
        p->used = 1;
        p->inuse = 1;
        if (p->name)
            return 0;
    }
    return p;
}


/*
 * Determine if a parameter is set.
 */
static int
par_isset(PAR_INDEX index)
{
    return par_info(index)->name != 0;
}


/*
 * Index the ParInfo table.
 */
static PAR_INFO *
par_info(PAR_INDEX index)
{
    PAR_INFO *p = &ParInfo[index];

    if (index != p->index)
        error(BUG, "par_info: table out of order: %d != %d", index, p-index);
    return p;
}


/*
 * If any options were set but were not used, print out a warning message for
 * the user.
 */
void
opt_check(void)
{
    PAR_INFO *p;
    PAR_INFO *q;
    PAR_INFO *r = endof(ParInfo);

    for (p = ParInfo; p < r; ++p) {
        if (p->used || !p->set)
            continue;
        error(RET, "warning: %s set but not used in test %s",
                                                        p->name, TestName);
        for (q = p+1; q < r; ++q)
            if (q->set && q->name == p->name)
                q->set = 0;
    }
}


/*
 * Server.
 */
static void
server(void)
{
    server_listen();
    for (;;) {
        REQ req;
        pid_t pid;
        TEST *test;
        int s = offset(REQ, req_index);

        debug("ready for requests");
        if (!server_recv_request())
            continue;
        pid = fork();
        if (pid < 0) {
            error(SYS|RET, "fork failed");
            continue;
        }
        if (pid > 0) {
            remotefd_close();
            waitpid(pid, 0, 0);
            continue;
        }
        remotefd_setup();

        recv_mesg(&req, s, "request version");
        dec_init(&req);
        dec_req_version(&Req);
        if (Req.ver_maj != VER_MAJ || Req.ver_min != VER_MIN)
            version_error();
        recv_mesg(&req.req_index, sizeof(req)-s, "request data");
        dec_req_data(&Req);
        if (Req.req_index >= cardof(Tests))
            error(0, "bad request index: %d", Req.req_index);

        test = &Tests[Req.req_index];
        TestName = test->name;
        debug("received request: %s", TestName);
        init_lstat();
        set_affinity();
        (test->server)();
        exit(0);
    }
    close(ListenFD);
}


/*
 * If there is a version mismatch of qperf between the client and server, tell
 * the user which needs to be upgraded.
 */
static void
version_error(void)
{
    int hi_maj = Req.ver_maj;
    int hi_min = Req.ver_min;
    int hi_inc = Req.ver_inc;
    int lo_maj = VER_MAJ;
    int lo_min = VER_MIN;
    int lo_inc = VER_INC;
    char *msg = "upgrade qperf on %s from %d.%d.%d to %d.%d.%d";
    char *low = "server";

    if (lo_maj > hi_maj || (lo_maj == hi_maj && lo_min > hi_min)) {
        hi_maj = VER_MAJ;
        hi_min = VER_MIN;
        hi_inc = VER_INC;
        lo_maj = Req.ver_maj;
        lo_min = Req.ver_min;
        lo_inc = Req.ver_inc;
        low   = "client";
    }
    error(0, msg, low, lo_maj, lo_min, lo_inc, hi_maj, hi_min, hi_inc);
}


/*
 * Listen for any requests.
 */
static void
server_listen(void)
{
    AI *ai;
    AI hints ={
        .ai_flags    = AI_PASSIVE | AI_NUMERICSERV,
        .ai_family   = AF_UNSPEC,
        .ai_socktype = SOCK_STREAM
    };
    AI *ailist = getaddrinfo_port(0, ListenPort, &hints);

    for (ai = ailist; ai; ai = ai->ai_next) {
        ListenFD = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (ListenFD < 0)
            continue;
        setsockopt_one(ListenFD, SO_REUSEADDR);
        if (bind(ListenFD, ai->ai_addr, ai->ai_addrlen) == SUCCESS0)
            break;
        close(ListenFD);
    }
    freeaddrinfo(ailist);
    if (!ai)
        error(0, "unable to bind to listen port");

    if (!Req.timeout)
        Req.timeout = DEF_TIMEOUT;
    if (listen(ListenFD, LISTENQ) < 0)
        error(SYS, "listen failed");
}


/*
 * Accept a request from a client.
 */
static int
server_recv_request(void)
{
    socklen_t clientLen;
    struct sockaddr_in clientAddr;

    clientLen = sizeof(clientAddr);
    RemoteFD = accept(ListenFD, (struct sockaddr *)&clientAddr, &clientLen);
    if (RemoteFD < 0)
        return error(SYS|RET, "accept failed");
    return 1;
}


/*
 * Client.
 */
static void
client(TEST *test)
{
    int i;

    for (i = 0; i < P_N; ++i)
        ParInfo[i].inuse = 0;
    if (!par_isset(L_NO_MSGS))
        setp_u32(0, L_TIME, DEF_TIME);
    if (!par_isset(R_NO_MSGS))
        setp_u32(0, R_TIME, DEF_TIME);
    setp_u32(0, L_TIMEOUT, DEF_TIMEOUT);
    setp_u32(0, R_TIMEOUT, DEF_TIMEOUT);
    par_use(L_AFFINITY);
    par_use(R_AFFINITY);
    par_use(L_TIME);
    par_use(R_TIME);

    set_affinity();
    RReq.ver_maj = VER_MAJ;
    RReq.ver_min = VER_MIN;
    RReq.ver_inc = VER_INC;
    RReq.req_index = test - Tests;
    TestName = test->name;
    debug("sending request: %s", TestName);
    init_lstat();
    printf("%s:\n", TestName);
    (*test->client)();
    remotefd_close();
    place_show();
}


/*
 * Send a request to the server.
 */
void
client_send_request(void)
{
    REQ req;
    AI *a;
    AI hints ={
        .ai_family   = AF_UNSPEC,
        .ai_socktype = SOCK_STREAM
    };
    AI *ailist = getaddrinfo_port(ServerName, ListenPort, &hints);

    RemoteFD = -1;
    if (ServerWait)
        start_test_timer(ServerWait);
    for (;;) {
        for (a = ailist; a; a = a->ai_next) {
            if (Finished)
                break;
            RemoteFD = socket(a->ai_family, a->ai_socktype, a->ai_protocol);
            if (RemoteFD < 0)
                continue;
            if (connect(RemoteFD, a->ai_addr, a->ai_addrlen) != SUCCESS0) {
                remotefd_close();
                continue;
            }
            ServerAddrLen = a->ai_addrlen;
            memcpy(&ServerAddr, a->ai_addr, ServerAddrLen);
            break;
        }
        if (RemoteFD >= 0 || !ServerWait || Finished)
            break;
        sleep(1);
    }

    if (ServerWait)
        stop_test_timer();
    freeaddrinfo(ailist);

    if (RemoteFD < 0)
        error(0, "%s: failed to connect", ServerName);
    remotefd_setup();
    enc_init(&req);
    enc_req(&RReq);
    send_mesg(&req, sizeof(req), "request data");
}


/*
 * Configure the remote file descriptor.
 */
static void
remotefd_setup(void)
{
    int one = 1;

    if (ioctl(RemoteFD, FIONBIO, &one) < 0)
        error(SYS, "ioctl FIONBIO failed");
    if (fcntl(RemoteFD, F_SETOWN, getpid()) < 0)
        error(SYS, "fcntl F_SETOWN failed");
}


/*
 * Close the remote file descriptor.
 * Set a file descriptor to non-blocking.
 */
static void
remotefd_close(void)
{
    close(RemoteFD);
    RemoteFD = -1;
}


/*
 * Exchange results.  We sync up only to ensure that the client is out of its
 * loop so we can close our socket or whatever communication medium we are
 * using.
 */
void
exchange_results(void)
{
    STAT stat;

    if (is_client()) {
        recv_mesg(&stat, sizeof(stat), "results");
        dec_init(&stat);
        dec_stat(&RStat);
        send_sync("synchronization after test");
    } else {
        enc_init(&stat);
        enc_stat(&LStat);
        send_mesg(&stat, sizeof(stat), "results");
        recv_sync("synchronization after test");
    }
}


/*
 * Initialize local status information.
 */
static void
init_lstat(void)
{
    memcpy(&LStat, &IStat, sizeof(LStat));
}


/*
 * Show configuration (client side).
 */
static void
run_client_conf(void)
{
    CONF lconf;
    CONF rconf;

    client_send_request();
    recv_mesg(&rconf, sizeof(rconf), "configuration");
    get_conf(&lconf);
    view_strn('a', "", "loc_node",  lconf.node);
    view_strn('a', "", "loc_cpu",   lconf.cpu);
    view_strn('a', "", "loc_os",    lconf.os);
    view_strn('a', "", "loc_qperf", lconf.qperf);
    view_strn('a', "", "rem_node",  rconf.node);
    view_strn('a', "", "rem_cpu",   rconf.cpu);
    view_strn('a', "", "rem_os",    rconf.os);
    view_strn('a', "", "rem_qperf", rconf.qperf);
}


/*
 * Show configuration (server side).
 */
static void
run_server_conf(void)
{
    CONF conf;

    get_conf(&conf);
    send_mesg(&conf, sizeof(conf), "configuration");
}


/*
 * Get configuration.
 */
static void
get_conf(CONF *conf)
{
    struct utsname utsname;

    uname(&utsname);
    strncopy(conf->node, utsname.nodename, sizeof(conf->node));
    snprintf(conf->os, sizeof(conf->os), "%s %s", utsname.sysname,
                                                  utsname.release);
    get_cpu(conf);
    snprintf(conf->qperf, sizeof(conf->qperf), "%d.%d.%d",
                                        VER_MAJ, VER_MIN, VER_INC);
}


/*
 * Get CPU information.
 */
static void
get_cpu(CONF *conf)
{
    char count[STRSIZE];
    char speed[STRSIZE];
    char buf[BUFSIZE];
    char cpu[BUFSIZE];
    char mhz[BUFSIZE];
    int cpus = 0;
    int mixed = 0;
    FILE *fp = fopen("/proc/cpuinfo", "r");

    if (!fp)
        error(0, "cannot open /proc/cpuinfo");
    cpu[0] = '\0';
    mhz[0] = '\0';
    while (fgets(buf, sizeof(buf), fp)) {
        int n = strlen(buf);
        if (cmpsub(buf, "model name")) {
            ++cpus;
            if (!mixed) {
                if (cpu[0] == '\0')
                    strncopy(cpu, buf, sizeof(cpu));
                else if (!streq(buf, cpu))
                    mixed = 1;
            }
        } else if (cmpsub(buf, "cpu MHz")) {
            if (!mixed) {
                if (mhz[0] == '\0')
                    strncopy(mhz, buf, sizeof(mhz));
                else if (!streq(buf, mhz))
                    mixed = 1;
            }
        }
        while (n && buf[n-1] != '\n') {
            if (!fgets(buf, sizeof(buf), fp))
                break;
            n = strlen(buf);
        }
    }
    fclose(fp);

    /* CPU name */
    if (mixed)
        strncopy(cpu, "Mixed CPUs", sizeof(cpu));
    else {
        char *p = cpu;
        char *q = skip_colon(cpu);
        if (!q)
            return;
        for (;;) {
            if (*q == '(' && cmpsub(q, "(r)"))
                q += 3;
            else if (*q == '(' && cmpsub(q, "(tm)"))
                q += 4;
            if (tolower(*q) == 'c' && cmpsub(q, "cpu "))
                q += 4;
            if (tolower(*q) == 'p' && cmpsub(q, "processor "))
                q += 10;
            else if (q[0] == ' ' && q[1] == ' ')
                q += 1;
            else if (q[0] == '\n')
                q += 1;
            else if (!(*p++ = *q++))
                break;
        }
    }

    /* CPU speed */
    speed[0] = '\0';
    if (!mixed) {
        int n = strlen(cpu);
        if (n < 3 || cpu[n-2] != 'H' || cpu[n-1] != 'z') {
            char *q = skip_colon(mhz);
            if (q) {
                int freq = atoi(q);
                if (freq < 1000)
                    snprintf(speed, sizeof(speed), " %dMHz", freq);
                else
                    snprintf(speed, sizeof(speed), " %.1fGHz", freq/1000.0);
            }
        }
    }

    /* Number of CPUs */
    if (cpus == 1)
        count[0] = '\0';
    else
        snprintf(count, sizeof(count), "%d Cores: ", cpus);

    snprintf(conf->cpu, sizeof(conf->cpu), "%s%s%s", count, cpu, speed);
}


/*
 * Quit (client side).
 */
static void
run_client_quit(void)
{
    opt_check();
    client_send_request();
    sync_test();
    exit(0);
}


/*
 * Quit (server side).  The read is to ensure that the client first quits to
 * ensure that everything closes down cleanly.
 */
static void
run_server_quit(void)
{
    int z;
    char buf[1];

    sync_test();
    z = read(RemoteFD, buf, sizeof(buf));
    kill(getppid(), SIGQUIT);
    exit(0);
}


/*
 * Synchronize the client and server.
 */
void
sync_test(void)
{
    synchronize("synchronization before test");
    start_test_timer(Req.time);
}


/*
 * Start test timer.
 */
static void
start_test_timer(int seconds)
{
    struct itimerval itimerval = {{0}};

    Finished = 0;
    get_times(LStat.time_s);
    setitimer(ITIMER_REAL, &itimerval, 0);
    if (!seconds)
        return;

    debug("starting timer for %d seconds", seconds);
    itimerval.it_value.tv_sec = seconds;
    /*
     * SLES11 has high precision timers; too low an interval will cause timer
     * to fire extremely rapidly after first occurrence.  We set it to 10 ms.
     */
    itimerval.it_interval.tv_usec = 10000;
    setitimer(ITIMER_REAL, &itimerval, 0);
}


/*
 * Stop timing.  Note that the end time is obtained by the first call to
 * set_finished.  In the tests, when SIGALRM goes off, it may be executing a
 * system call which gets interrupted.  If SIGALRM goes off after Finished is
 * checked but before the system call is initiated, the system call will be
 * executed and it will take the second SIGALRM call generated by the interval
 * timer to wake it up.  Hence, we save the end times in sig_alrm.  Note that
 * if Finished is set, we reject any packets that are sent or arrive in order
 * not to cheat.  We clear Finished since code assumes that it is the default
 * state.
 */
void
stop_test_timer(void)
{
    struct itimerval itimerval = {{0}};

    set_finished();
    setitimer(ITIMER_REAL, &itimerval, 0);
    Finished = 0;
    debug("stopping timer");
}


/*
 * Establish the current test as finished.
 */
void
set_finished(void)
{
    if (Finished++ == 0)
        get_times(LStat.time_e);
}


/*
 * Show results.
 */
void
show_results(MEASURE measure)
{
    calc_results();
    show_info(measure);
}


/*
 * Calculate results.
 */
static void
calc_results(void)
{
    double no_msgs;
    double locTime;
    double remTime;
    double midTime;
    double gB = 1000 * 1000 * 1000;

    add_ustat(&LStat.s, &RStat.rem_s);
    add_ustat(&LStat.r, &RStat.rem_r);
    add_ustat(&RStat.s, &LStat.rem_s);
    add_ustat(&RStat.r, &LStat.rem_r);

    memset(&Res, 0, sizeof(Res));
    calc_node(&Res.l, &LStat);
    calc_node(&Res.r, &RStat);
    no_msgs = LStat.r.no_msgs + RStat.r.no_msgs;
    if (no_msgs)
        Res.latency = Res.l.time_real / no_msgs;

    locTime = Res.l.time_real;
    remTime = Res.r.time_real;
    midTime = (locTime + remTime) / 2;

    if (locTime == 0 || remTime == 0)
        return;

    /* Calculate messaging rate */
    if (!RStat.r.no_msgs)
        Res.msg_rate = LStat.r.no_msgs / remTime;
    else if (!LStat.r.no_msgs)
        Res.msg_rate = RStat.r.no_msgs / locTime;
    else
        Res.msg_rate = (LStat.r.no_msgs + RStat.r.no_msgs) / midTime;

    /* Calculate send bandwidth */
    if (!RStat.s.no_bytes)
        Res.send_bw = LStat.s.no_bytes / locTime;
    else if (!LStat.s.no_bytes)
        Res.send_bw = RStat.s.no_bytes / remTime;
    else
        Res.send_bw = (LStat.s.no_bytes + RStat.s.no_bytes) / midTime;

    /* Calculate receive bandwidth. */
    if (!RStat.r.no_bytes)
        Res.recv_bw = LStat.r.no_bytes / locTime;
    else if (!LStat.r.no_bytes)
        Res.recv_bw = RStat.r.no_bytes / remTime;
    else
        Res.recv_bw = (LStat.r.no_bytes + RStat.r.no_bytes) / midTime;

    /* Calculate costs */
    if (LStat.s.no_bytes && !LStat.r.no_bytes && !RStat.s.no_bytes)
        Res.send_cost = Res.l.time_cpu*gB / LStat.s.no_bytes;
    else if (RStat.s.no_bytes && !RStat.r.no_bytes && !LStat.s.no_bytes)
        Res.send_cost = Res.r.time_cpu*gB / RStat.s.no_bytes;
    if (RStat.r.no_bytes && !RStat.s.no_bytes && !LStat.r.no_bytes)
        Res.recv_cost = Res.r.time_cpu*gB / RStat.r.no_bytes;
    else if (LStat.r.no_bytes && !LStat.s.no_bytes && !RStat.r.no_bytes)
        Res.recv_cost = Res.l.time_cpu*gB / LStat.r.no_bytes;
}


/*
 * Determine the number of packets left to send.
 */
int
left_to_send(long *sentp, int room)
{
    int n;

    if (!Req.no_msgs)
        return room;
    n = Req.no_msgs - *sentp;
    if (n <= 0)
        return 0;
    if (n > room)
        return room;
    return n;
}


/*
 * Combine statistics that the remote node kept track of with those that the
 * local node kept.
 */
static void
add_ustat(USTAT *l, USTAT *r)
{
    l->no_bytes += r->no_bytes;
    l->no_msgs  += r->no_msgs;
    l->no_errs  += r->no_errs;
}


/*
 * Calculate time values for a node.
 */
static void
calc_node(RESN *resn, STAT *stat)
{
    int i;
    CLOCK cpu;
    double s = stat->time_e[T_REAL] - stat->time_s[T_REAL];

    memset(resn, 0, sizeof(*resn));
    if (s == 0)
        return;
    if (stat->no_ticks == 0)
        return;

    resn->time_real = s / stat->no_ticks;

    cpu = 0;
    for (i = 0; i < T_N; ++i)
        if (i != T_REAL && i != T_IDLE)
            cpu += stat->time_e[i] - stat->time_s[i];
    resn->time_cpu = (float) cpu / stat->no_ticks;

    resn->cpu_user = (stat->time_e[T_USER] - stat->time_s[T_USER]
                   + stat->time_e[T_NICE] - stat->time_s[T_NICE]) / s;

    resn->cpu_intr = (stat->time_e[T_IRQ] - stat->time_s[T_IRQ]
                   +  stat->time_e[T_SOFTIRQ] - stat->time_s[T_SOFTIRQ]) / s;

    resn->cpu_idle = (stat->time_e[T_IDLE] - stat->time_s[T_IDLE]) / s;

    resn->cpu_kernel = (stat->time_e[T_KERNEL] - stat->time_s[T_KERNEL]
                     +  stat->time_e[T_STEAL] - stat->time_s[T_STEAL]) / s;

    resn->cpu_io_wait = (stat->time_e[T_IOWAIT] - stat->time_s[T_IOWAIT]) / s;

    resn->cpu_total = resn->cpu_user + resn->cpu_intr
                    + resn->cpu_kernel + resn->cpu_io_wait;
}


/*
 * Show relevant values.
 */
static void
show_info(MEASURE measure)
{
    if (measure == LATENCY) {
        view_time('a', "", "latency", Res.latency);
        view_rate('s', "", "msg_rate", Res.msg_rate);
    } else if (measure == MSG_RATE) {
        view_rate('a', "", "msg_rate", Res.msg_rate);
    } else if (measure == BANDWIDTH) {
        view_band('a', "", "bw", Res.recv_bw);
        view_rate('s', "", "msg_rate", Res.msg_rate);
    } else if (measure == BANDWIDTH_SR) {
        view_band('a', "", "send_bw", Res.send_bw);
        view_band('a', "", "recv_bw", Res.recv_bw);
        view_rate('s', "", "msg_rate", Res.msg_rate);
    }
    show_used();
    view_cost('t', "", "send_cost", Res.send_cost);
    view_cost('t', "", "recv_cost", Res.recv_cost);
    show_rest();
    if (Debug)
        show_debug();
}


/*
 * Show parameters the user set.
 */
static void
show_used(void)
{
    PAR_NAME *p;
    PAR_NAME *q = endof(ParName);

    if (!VerboseUsed)
        return;
    for (p = ParName; p < q; ++p) {
        PAR_INFO *l = par_info(p->loc_i);
        PAR_INFO *r = par_info(p->rem_i);

        if (!l->inuse && !r->inuse)
            continue;
        if (VerboseUsed < 2 && !l->set & !r->set)
            continue;
        if (l->type == 'l') {
            uint32_t lv = *(uint32_t *)l->ptr;
            uint32_t rv = *(uint32_t *)r->ptr;
            if (lv == rv)
                view_long('u', "", p->name, lv);
            else {
                view_long('u', "loc_", p->name, lv);
                view_long('u', "rem_", p->name, rv);
            }
        } else if (l->type == 'p') {
            if (streq(l->ptr, r->ptr))
                view_strn('u', "", p->name, l->ptr);
            else {
                view_strn('u', "loc_", p->name, l->ptr);
                view_strn('u', "rem_", p->name, r->ptr);
            }
        } else if (l->type == 's') {
            uint32_t lv = *(uint32_t *)l->ptr;
            uint32_t rv = *(uint32_t *)r->ptr;
            if (lv == rv)
                view_size('u', "", p->name, lv);
            else {
                view_size('u', "loc_", p->name, lv);
                view_size('u', "rem_", p->name, rv);
            }
        } else if (l->type == 't') {
            uint32_t lv = *(uint32_t *)l->ptr;
            uint32_t rv = *(uint32_t *)r->ptr;
            if (lv == rv)
                view_time('u', "", p->name, lv);
            else {
                view_time('u', "loc_", p->name, lv);
                view_time('u', "rem_", p->name, rv);
            }
        }
    }
}


/*
 * Show the remaining parameters.
 */
static void
show_rest(void)
{
    RESN *resnS;
    RESN *resnR;
    STAT *statS;
    STAT *statR;
    int srmode = 0;

    if (!UnifyNodes) {
        uint64_t ls = LStat.s.no_bytes;
        uint64_t lr = LStat.r.no_bytes;
        uint64_t rs = RStat.s.no_bytes;
        uint64_t rr = RStat.r.no_bytes;
        
        if (ls && !rs && rr && !lr) {
            srmode = 1;
            resnS = &Res.l;
            resnR = &Res.r;
            statS = &LStat;
            statR = &RStat;
        } else if (rs && !ls && lr && !rr) {
            srmode = 1;
            resnS = &Res.r;
            resnR = &Res.l;
            statS = &RStat;
            statR = &LStat;
        }
    }

    if (srmode) {
        view_cpus('t', "", "send_cpus_used",   resnS->cpu_total);
        view_cpus('T', "", "send_cpus_user",   resnS->cpu_user);
        view_cpus('T', "", "send_cpus_intr",   resnS->cpu_intr);
        view_cpus('T', "", "send_cpus_kernel", resnS->cpu_kernel);
        view_cpus('T', "", "send_cpus_iowait", resnS->cpu_io_wait);
        view_time('T', "", "send_real_time",   resnS->time_real);
        view_time('T', "", "send_cpu_time",    resnS->time_cpu);
        view_long('S', "", "send_errors",      statS->s.no_errs);
        view_size('S', "", "send_bytes",       statS->s.no_bytes);
        view_long('S', "", "send_msgs",        statS->s.no_msgs);
        view_long('S', "", "send_max_cqe",     statS->max_cqes);

        view_cpus('t', "", "recv_cpus_used",   resnR->cpu_total);
        view_cpus('T', "", "recv_cpus_user",   resnR->cpu_user);
        view_cpus('T', "", "recv_cpus_intr",   resnR->cpu_intr);
        view_cpus('T', "", "recv_cpus_kernel", resnR->cpu_kernel);
        view_cpus('T', "", "recv_cpus_iowait", resnR->cpu_io_wait);
        view_time('T', "", "recv_real_time",   resnR->time_real);
        view_time('T', "", "recv_cpu_time",    resnR->time_cpu);
        view_long('S', "", "recv_errors",      statR->r.no_errs);
        view_size('S', "", "recv_bytes",       statR->r.no_bytes);
        view_long('S', "", "recv_msgs",        statR->r.no_msgs);
        view_long('S', "", "recv_max_cqe",     statR->max_cqes);
    } else {
        view_cpus('t', "", "loc_cpus_used",    Res.l.cpu_total);
        view_cpus('T', "", "loc_cpus_user",    Res.l.cpu_user);
        view_cpus('T', "", "loc_cpus_intr",    Res.l.cpu_intr);
        view_cpus('T', "", "loc_cpus_kernel",  Res.l.cpu_kernel);
        view_cpus('T', "", "loc_cpus_iowait",  Res.l.cpu_io_wait);
        view_time('T', "", "loc_real_time",    Res.l.time_real);
        view_time('T', "", "loc_cpu_time",     Res.l.time_cpu);
        view_long('S', "", "loc_send_errors",  LStat.s.no_errs);
        view_long('S', "", "loc_recv_errors",  LStat.r.no_errs);
        view_size('S', "", "loc_send_bytes",   LStat.s.no_bytes);
        view_size('S', "", "loc_recv_bytes",   LStat.r.no_bytes);
        view_long('S', "", "loc_send_msgs",    LStat.s.no_msgs);
        view_long('S', "", "loc_recv_msgs",    LStat.r.no_msgs);
        view_long('S', "", "loc_max_cqe",      LStat.max_cqes);

        view_cpus('t', "", "rem_cpus_used",    Res.r.cpu_total);
        view_cpus('T', "", "rem_cpus_user",    Res.r.cpu_user);
        view_cpus('T', "", "rem_cpus_intr",    Res.r.cpu_intr);
        view_cpus('T', "", "rem_cpus_kernel",  Res.r.cpu_kernel);
        view_cpus('T', "", "rem_cpus_iowait",  Res.r.cpu_io_wait);
        view_time('T', "", "rem_real_time",    Res.r.time_real);
        view_time('T', "", "rem_cpu_time",     Res.r.time_cpu);
        view_long('S', "", "rem_send_errors",  RStat.s.no_errs);
        view_long('S', "", "rem_recv_errors",  RStat.r.no_errs);
        view_size('S', "", "rem_send_bytes",   RStat.s.no_bytes);
        view_size('S', "", "rem_recv_bytes",   RStat.r.no_bytes);
        view_long('S', "", "rem_send_msgs",    RStat.s.no_msgs);
        view_long('S', "", "rem_recv_msgs",    RStat.r.no_msgs);
        view_long('S', "", "rem_max_cqe",      RStat.max_cqes);
    }
}


/*
 * Show all values.
 */
static void
show_debug(void)
{
    /* Local node */
    view_long('d', "", "l_no_cpus",  LStat.no_cpus);
    view_long('d', "", "l_no_ticks", LStat.no_ticks);
    view_long('d', "", "l_max_cqes", LStat.max_cqes);

    if (LStat.no_ticks) {
        double t = LStat.no_ticks;
        CLOCK *s = LStat.time_s;
        CLOCK *e = LStat.time_e;
        double real    = (e[T_REAL]    - s[T_REAL])    / t;
        double user    = (e[T_USER]    - s[T_USER])    / t;
        double nice    = (e[T_NICE]    - s[T_NICE])    / t;
        double system  = (e[T_KERNEL]  - s[T_KERNEL])  / t;
        double idle    = (e[T_IDLE]    - s[T_IDLE])    / t;
        double iowait  = (e[T_IOWAIT]  - s[T_IOWAIT])  / t;
        double irq     = (e[T_IRQ]     - s[T_IRQ])     / t;
        double softirq = (e[T_SOFTIRQ] - s[T_SOFTIRQ]) / t;
        double steal   = (e[T_STEAL]   - s[T_STEAL])   / t;

        view_time('d', "", "l_timer_real",    real);
        view_time('d', "", "l_timer_user",    user);
        view_time('d', "", "l_timer_nice",    nice);
        view_time('d', "", "l_timer_system",  system);
        view_time('d', "", "l_timer_idle",    idle);
        view_time('d', "", "l_timer_iowait",  iowait);
        view_time('d', "", "l_timer_irq",     irq);
        view_time('d', "", "l_timer_softirq", softirq);
        view_time('d', "", "l_timer_steal",   steal);
    }

    view_size('d', "", "l_s_no_bytes", LStat.s.no_bytes);
    view_long('d', "", "l_s_no_msgs",  LStat.s.no_msgs);
    view_long('d', "", "l_s_no_errs",  LStat.s.no_errs);

    view_size('d', "", "l_r_no_bytes", LStat.r.no_bytes);
    view_long('d', "", "l_r_no_msgs",  LStat.r.no_msgs);
    view_long('d', "", "l_r_no_errs",  LStat.r.no_errs);

    view_size('d', "", "l_rem_s_no_bytes", LStat.rem_s.no_bytes);
    view_long('d', "", "l_rem_s_no_msgs",  LStat.rem_s.no_msgs);
    view_long('d', "", "l_rem_s_no_errs",  LStat.rem_s.no_errs);

    view_size('d', "", "l_rem_r_no_bytes", LStat.rem_r.no_bytes);
    view_long('d', "", "l_rem_r_no_msgs",  LStat.rem_r.no_msgs);
    view_long('d', "", "l_rem_r_no_errs",  LStat.rem_r.no_errs);

    /* Remote node */
    view_long('d', "", "r_no_cpus",  RStat.no_cpus);
    view_long('d', "", "r_no_ticks", RStat.no_ticks);
    view_long('d', "", "r_max_cqes", RStat.max_cqes);

    if (RStat.no_ticks) {
        double t = RStat.no_ticks;
        CLOCK *s = RStat.time_s;
        CLOCK *e = RStat.time_e;

        double real    = (e[T_REAL]    - s[T_REAL])    / t;
        double user    = (e[T_USER]    - s[T_USER])    / t;
        double nice    = (e[T_NICE]    - s[T_NICE])    / t;
        double system  = (e[T_KERNEL]  - s[T_KERNEL])  / t;
        double idle    = (e[T_IDLE]    - s[T_IDLE])    / t;
        double iowait  = (e[T_IOWAIT]  - s[T_IOWAIT])  / t;
        double irq     = (e[T_IRQ]     - s[T_IRQ])     / t;
        double softirq = (e[T_SOFTIRQ] - s[T_SOFTIRQ]) / t;
        double steal   = (e[T_STEAL]   - s[T_STEAL])   / t;

        view_time('d', "", "r_timer_real",    real);
        view_time('d', "", "r_timer_user",    user);
        view_time('d', "", "r_timer_nice",    nice);
        view_time('d', "", "r_timer_system",  system);
        view_time('d', "", "r_timer_idle",    idle);
        view_time('d', "", "r_timer_iowait",  iowait);
        view_time('d', "", "r_timer_irq",     irq);
        view_time('d', "", "r_timer_softirq", softirq);
        view_time('d', "", "r_timer_steal",   steal);
    }

    view_size('d', "", "r_s_no_bytes", RStat.s.no_bytes);
    view_long('d', "", "r_s_no_msgs",  RStat.s.no_msgs);
    view_long('d', "", "r_s_no_errs",  RStat.s.no_errs);

    view_size('d', "", "r_r_no_bytes", RStat.r.no_bytes);
    view_long('d', "", "r_r_no_msgs",  RStat.r.no_msgs);
    view_long('d', "", "r_r_no_errs",  RStat.r.no_errs);

    view_size('d', "", "r_rem_s_no_bytes", RStat.rem_s.no_bytes);
    view_long('d', "", "r_rem_s_no_msgs",  RStat.rem_s.no_msgs);
    view_long('d', "", "r_rem_s_no_errs",  RStat.rem_s.no_errs);

    view_size('d', "", "r_rem_r_no_bytes", RStat.rem_r.no_bytes);
    view_long('d', "", "r_rem_r_no_msgs",  RStat.rem_r.no_msgs);
    view_long('d', "", "r_rem_r_no_errs",  RStat.rem_r.no_errs);
}


/*
 * Show a cost in terms of seconds per gigabyte.
 */
static void
view_cost(int type, char *pref, char *name, double value)
{
    int n = 0;
    char *tab[] ={ "ns/GB", "us/GB", "ms/GB", "sec/GB" };

    value *=  1E9;
    if (!verbose(type, value))
        return;
    if (!UnifyUnits) {
        while (value >= 1000 && n < (int)cardof(tab)-1) {
            value /= 1000;
            ++n;
        }
    }
    place_val(pref, name, tab[n], value);
}


/*
 * Show the number of cpus.
 */
static void
view_cpus(int type, char *pref, char *name, double value)
{
    value *= 100;
    if (!verbose(type, value))
        return;
    place_val(pref, name, "% cpus", value);
}


/*
 * Show a messaging rate.
 */
static void
view_rate(int type, char *pref, char *name, double value)
{
    int n = 0;
    char *tab[] ={ "/sec", "K/sec", "M/sec", "G/sec", "T/sec" };

    if (!verbose(type, value))
        return;
    if (!UnifyUnits) {
        while (value >= 1000 && n < (int)cardof(tab)-1) {
            value /= 1000;
            ++n;
        }
    }
    place_val(pref, name, tab[n], value);
}


/*
 * Show a number.
 */
static void
view_long(int type, char *pref, char *name, long long value)
{
    int n = 0;
    double val = value;
    char *tab[] ={ "", "thousand", "million", "billion", "trillion" };

    if (!verbose(type, val))
        return;
    if (!UnifyUnits && val >= 1000*1000) {
        while (val >= 1000 && n < (int)cardof(tab)-1) {
            val /= 1000;
            ++n;
        }
    }
    place_val(pref, name, tab[n], val);
}


/*
 * Show a bandwidth value.
 */
static void
view_band(int type, char *pref, char *name, double value)
{
    int n, s;
    char **tab;

    if (!verbose(type, value))
        return;
    if (UseBitsPerSec) {
        char *t[] ={ "bits/sec", "Kb/sec", "Mb/sec", "Gb/sec", "Tb/sec" };
        s = cardof(t);
        tab = t;
        value *= 8;
    } else {
        char *t[] ={ "bytes/sec", "KB/sec", "MB/sec", "GB/sec", "TB/sec" };
        s = cardof(t);
        tab = t;
    }

    n = 0;
    if (!UnifyUnits) {
        while (value >= 1000 && n < s-1) {
            value /= 1000;
            ++n;
        }
    }
    place_val(pref, name, tab[n], value);
}


/*
 * Show a size.
 */
static void
view_size(int type, char *pref, char *name, long long value)
{
    int n = 0;
    double val = value;
    char *tab[] ={ "bytes", "KB", "MB", "GB", "TB" };

    if (!verbose(type, val))
        return;
    if (!UnifyUnits) {
        if (nice_1024(pref, name, value))
            return;
        while (val >= 1000 && n < (int)cardof(tab)-1) {
            val /= 1000;
            ++n;
        }
    }
    place_val(pref, name, tab[n], val);
}


/*
 * Show a number if it can be expressed as a nice multiple of a power of 1024.
 */
static int
nice_1024(char *pref, char *name, long long value)
{
    char *data;
    char *altn;
    int n = 0;
    long long val = value;
    char *tab[] ={ "KiB", "MiB", "GiB", "TiB" };

    if (val < 1024 || val % 1024)
        return 0;
    val /= 1024;
    while (val >= 1024 && n < (int)cardof(tab)-1) {
        if (val % 1024)
            return 0;
        val /= 1024;
        ++n;
    }
    data = qasprintf("%lld", val);
    altn = qasprintf("%lld", value);
    place_any(pref, name, tab[n], commify(data), commify(altn));
    return 1;
}


/*
 * Show a string.
 */
static void
view_strn(int type, char *pref, char *name, char *value)
{
    if (!verbose(type, value[0] != '\0'))
        return;
    place_any(pref, name, 0, strdup(value), 0);
}


/*
 * Show a time.
 */
static void
view_time(int type, char *pref, char *name, double value)
{
    int n = 0;
    char *tab[] ={ "ns", "us", "ms", "sec" };

    value *= 1E9;
    if (!verbose(type, value))
        return;
    if (!UnifyUnits) {
        while (value >= 1000 && n < (int)cardof(tab)-1) {
            value /= 1000;
            ++n;
        }
    }
    place_val(pref, name, tab[n], value);
}


/*
 * Determine if we are verbose enough to show a value.
 */
static int
verbose(int type, double value)
{
    if (type == 'a')
        return 1;
    if (value <= 0)
        return 0;
    switch (type) {
    case 'd': return Debug;
    case 'c': return VerboseConf >= 1;
    case 's': return VerboseStat >= 1;
    case 't': return VerboseTime >= 1;
    case 'u': return VerboseUsed >= 1;
    case 'C': return VerboseConf >= 2;
    case 'S': return VerboseStat >= 2;
    case 'T': return VerboseTime >= 2;
    case 'U': return VerboseUsed >= 2;
    default:  error(BUG, "verbose: bad type: %c (%o)", type, type);
    }
    return 0;
}


/*
 * Place a value to be shown later.
 */
static void
place_val(char *pref, char *name, char *unit, double value)
{
    char *data = qasprintf("%.0f", value);
    char *p    = data;
    int   n    = Precision;

    if (*p == '-')
        ++p;
    while (isdigit(*p++))
        --n;
    if (n > 0) {
        free(data);
        data = qasprintf("%.*f", n, value);
        p = &data[strlen(data)];
        while (p > data && *--p == '0')
            ;
        if (p > data && *p == '.')
            --p;
        p[1] = '\0';
    }
    place_any(pref, name, unit, commify(data), 0);
}


/*
 * Place an entry in our show table.
 */
static void
place_any(char *pref, char *name, char *unit, char *data, char *altn)
{
    SHOW *show = &ShowTable[ShowIndex++];

    if (ShowIndex > cardof(ShowTable))
        error(BUG, "need to increase size of ShowTable");
    show->pref = pref;
    show->name = name;
    show->unit = unit;
    show->data = data;
    show->altn = altn;
}


/*
 * Show all saved values.
 */
static void
place_show(void)
{
    int i;
    int nameLen = 0;
    int dataLen = 0;
    int unitLen = 0;

    /* First compute formating sizes */
    for (i = 0; i < ShowIndex; ++i) {
        int n;
        SHOW *show = &ShowTable[i];
        n = (show->pref ? strlen(show->pref) : 0) + strlen(show->name);
        if (n > nameLen)
            nameLen = n;
        n = strlen(show->data);
        if (show->unit) {
            if (n > dataLen)
                dataLen = n;
            n = strlen(show->unit);
            if (n > unitLen)
                unitLen = n;
        }
    }

    /* Then display results */
    for (i = 0; i < ShowIndex; ++i) {
        int n = 0;
        SHOW *show = &ShowTable[i];

        printf("    ");
        if (show->pref) {
            n = strlen(show->pref);
            printf("%s", show->pref);
        }
        printf("%-*s", nameLen-n, show->name);
        if (show->unit) {
            printf("  =  %*s", dataLen, show->data);
            printf(" %s", show->unit);
        } else
            printf("  =  %s", show->data);
        if (show->altn)
            printf(" (%s)", show->altn);
        printf("\n");
        free(show->data);
        free(show->altn);
    }
    ShowIndex = 0;
}


/*
 * Set the processor affinity.
 */
static void
set_affinity(void)
{
    cpu_set_t set;
    int a = Req.affinity;

    if (!a)
        return;
    CPU_ZERO(&set);
    CPU_SET(a-1, &set);
    if (sched_setaffinity(0, sizeof(set), &set) < 0)
        error(SYS, "cannot set processor affinity (cpu %d)", a-1);
}


/*
 * Encode a REQ structure into a data stream.
 */
static void
enc_req(REQ *host)
{
    enc_int(host->ver_maj,       sizeof(host->ver_maj));
    enc_int(host->ver_min,       sizeof(host->ver_min));
    enc_int(host->ver_inc,       sizeof(host->ver_inc));
    enc_int(host->req_index,     sizeof(host->req_index));
    enc_int(host->access_recv,   sizeof(host->access_recv));
    enc_int(host->affinity,      sizeof(host->affinity));
    enc_int(host->alt_port,      sizeof(host->alt_port));
    enc_int(host->flip,          sizeof(host->flip));
    enc_int(host->msg_size,      sizeof(host->msg_size));
    enc_int(host->mtu_size,      sizeof(host->mtu_size));
    enc_int(host->no_msgs,       sizeof(host->no_msgs));
    enc_int(host->poll_mode,     sizeof(host->poll_mode));
    enc_int(host->port,          sizeof(host->port));
    enc_int(host->rd_atomic,     sizeof(host->rd_atomic));
    enc_int(host->sl,            sizeof(host->sl));
    enc_int(host->sock_buf_size, sizeof(host->sock_buf_size));
    enc_int(host->src_path_bits, sizeof(host->src_path_bits));
    enc_int(host->time,          sizeof(host->time));
    enc_int(host->timeout,       sizeof(host->timeout));
    enc_int(host->use_cm,        sizeof(host->use_cm));
    enc_str(host->id,            sizeof(host->id));
    enc_str(host->static_rate,   sizeof(host->static_rate));
}


/*
 * Decode the version part of a REQ structure from a data stream.  To decode
 * the entire REQ structure, call dec_req_version and dec_req_data in
 * succession.
 */
static void
dec_req_version(REQ *host)
{
    host->ver_maj       = dec_int(sizeof(host->ver_maj));
    host->ver_min       = dec_int(sizeof(host->ver_min));
    host->ver_inc       = dec_int(sizeof(host->ver_inc));
}


/*
 * Decode the data part of a REQ structure from a data stream.
 */
static void
dec_req_data(REQ *host)
{
    host->req_index     = dec_int(sizeof(host->req_index));
    host->access_recv   = dec_int(sizeof(host->access_recv));
    host->affinity      = dec_int(sizeof(host->affinity));
    host->alt_port      = dec_int(sizeof(host->alt_port));
    host->flip          = dec_int(sizeof(host->flip));
    host->msg_size      = dec_int(sizeof(host->msg_size));
    host->mtu_size      = dec_int(sizeof(host->mtu_size));
    host->no_msgs       = dec_int(sizeof(host->no_msgs));
    host->poll_mode     = dec_int(sizeof(host->poll_mode));
    host->port          = dec_int(sizeof(host->port));
    host->rd_atomic     = dec_int(sizeof(host->rd_atomic));
    host->sl            = dec_int(sizeof(host->sl));
    host->sock_buf_size = dec_int(sizeof(host->sock_buf_size));
    host->src_path_bits = dec_int(sizeof(host->src_path_bits));
    host->time          = dec_int(sizeof(host->time));
    host->timeout       = dec_int(sizeof(host->timeout));
    host->use_cm        = dec_int(sizeof(host->use_cm));
                          dec_str(host->id, sizeof(host->id));
                          dec_str(host->static_rate,sizeof(host->static_rate));
}


/*
 * Encode a STAT structure into a data stream.
 */
static void
enc_stat(STAT *host)
{
    int i;

    enc_int(host->no_cpus,  sizeof(host->no_cpus));
    enc_int(host->no_ticks, sizeof(host->no_ticks));
    enc_int(host->max_cqes, sizeof(host->max_cqes));
    for (i = 0; i < T_N; ++i)
        enc_int(host->time_s[i], sizeof(host->time_s[i]));
    for (i = 0; i < T_N; ++i)
        enc_int(host->time_e[i], sizeof(host->time_e[i]));
    enc_ustat(&host->s);
    enc_ustat(&host->r);
    enc_ustat(&host->rem_s);
    enc_ustat(&host->rem_r);
}


/*
 * Decode a STAT structure from a data stream.
 */
static void
dec_stat(STAT *host)
{
    int i;

    host->no_cpus  = dec_int(sizeof(host->no_cpus));
    host->no_ticks = dec_int(sizeof(host->no_ticks));
    host->max_cqes = dec_int(sizeof(host->max_cqes));
    for (i = 0; i < T_N; ++i)
        host->time_s[i] = dec_int(sizeof(host->time_s[i]));
    for (i = 0; i < T_N; ++i)
        host->time_e[i] = dec_int(sizeof(host->time_e[i]));
    dec_ustat(&host->s);
    dec_ustat(&host->r);
    dec_ustat(&host->rem_s);
    dec_ustat(&host->rem_r);
}


/*
 * Encode a USTAT structure into a data stream.
 */
static void
enc_ustat(USTAT *host)
{
    enc_int(host->no_bytes, sizeof(host->no_bytes));
    enc_int(host->no_msgs,  sizeof(host->no_msgs));
    enc_int(host->no_errs,  sizeof(host->no_errs));
}


/*
 * Decode a USTAT structure from a data stream.
 */
static void
dec_ustat(USTAT *host)
{
    host->no_bytes = dec_int(sizeof(host->no_bytes));
    host->no_msgs  = dec_int(sizeof(host->no_msgs));
    host->no_errs  = dec_int(sizeof(host->no_errs));
}


/*
 * Get various temporal parameters.
 */
static void
get_times(CLOCK timex[T_N])
{
    int n;
    char *p;
    char buf[BUFSIZE];
    struct tms tms;

    timex[0] = times(&tms);
    if (lseek(ProcStatFD, 0, 0) < 0)
        error(SYS, "failed to seek /proc/stat");
    n = read(ProcStatFD, buf, sizeof(buf)-1);
    buf[n] = '\0';
    if (strncmp(buf, "cpu ", 4))
        error(0, "/proc/stat does not start with 'cpu '");
    p = &buf[3];
    for (n = 1; n < T_N; ++n) {
        while (*p == ' ')
            ++p;
        if (!isdigit(*p)) {
            if (*p != '\n' || n < T_N-1)
                error(0, "/proc/stat has bad format");
            break;
        }
        timex[n] = strtoll(p, 0, 10);
        while (*p != ' ' && *p != '\n' && *p != '\0')
            ++p;
    }
    while (n < T_N)
        timex[n++] = 0;
}


/*
 * Insert commas within a number for readability.
 */
static char *
commify(char *data)
{
    int s;
    int d;
    int seqS;
    int seqE;
    int dataLen;
    int noCommas;

    if (!data)
        return data;
    if (UnifyUnits)
        return data;
    dataLen = strlen(data);
    seqS = seqE = dataLen;
    while (--seqS >= 0)
        if (!isdigit(data[seqS]))
            break;
    if (seqS >= 0 && data[seqS] == '.') {
        seqE = seqS;
        while (--seqS >= 0)
            if (!isdigit(data[seqS]))
                break;
    }
    noCommas = (--seqE - ++seqS) / 3;
    if (noCommas == 0)
        return data;
    data = realloc(data, dataLen+noCommas+1);
    if (!data)
        error(0, "out of space");
    s = dataLen;
    d = dataLen + noCommas;
    for (;;) {
        int n;
        data[d--] = data[s--];
        n = seqE - s;
        if (n > 0 && n%3 == 0) {
            data[d--] = ',';
            if (--noCommas == 0)
                break;
        }
    }
    return data;
}


/*
 * Like strncpy but ensures the destination is null terminated.
 */
static void
strncopy(char *d, char *s, int n)
{
    strncpy(d, s, n);
    d[n-1] = '\0';
}
