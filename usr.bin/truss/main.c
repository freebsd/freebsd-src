/*
 * The main module for truss.  Suprisingly simple, but, then, the other
 * files handle the bulk of the work.  And, of course, the kernel has to
 * do a lot of the work :).
 */
/*
 * $Id: main.c,v 1.3 1997/12/06 14:41:41 peter Exp $
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <err.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/pioctl.h>

extern int setup_and_wait(char **);
extern int start_tracing(int, int);
extern void i386_syscall_entry(int, int);
extern void i386_syscall_exit(int, int);
extern void i386_linux_syscall_entry(int, int);
extern void i386_linux_syscall_exit(int, int);

/*
 * These should really be parameterized -- I don't like having globals,
 * but this is the easiest way, right now, to deal with them.
 */

int pid = 0;
int nosigs = 0;
FILE *outfile = stderr;
char *prog;
int Procfd;
char progtype[50];	/* OS and type of executable */

static inline void
usage(void) {
  fprintf(stderr, "usage:  %s [-o <file>] [-S] { [-p <pid> ] | "
	  "[ <command> <args>] }\n", prog);
  exit(1);
}

struct ex_types {
  char *type;
  void (*enter_syscall)(int, int);
  void (*exit_syscall)(int, int);
} ex_types[] = {
  { "FreeBSD a.out", i386_syscall_entry, i386_syscall_exit },
  { "FreeBSD ELF", i386_syscall_entry, i386_syscall_exit },
  { "Linux ELF", i386_linux_syscall_entry, i386_linux_syscall_exit },
  { 0, 0, 0 },
};

/*
 * Set the execution type.  This is called after every exec, and when
 * a process is first monitored.  The procfs pseudo-file "etype" has
 * the execution module type -- see /proc/curproc/etype for an example.
 */

static struct ex_types *
set_etype() {
  struct ex_types *funcs;
  char etype[24];
  char progtype[32];
  int fd;

  sprintf(etype, "/proc/%d/etype", pid);
  if ((fd = open(etype, O_RDONLY)) == -1) {
    strcpy(progtype, "FreeBSD a.out");
  } else {
    int len = read(fd, progtype, sizeof(progtype));
    progtype[len-1] = '\0';
    close(fd);
  }

  for (funcs = ex_types; funcs->type; funcs++)
    if (!strcmp(funcs->type, progtype))
      break;

  return funcs;
}

main(int ac, char **av) {
  int mask;
  int c;
  int i;
  char **command;
  struct procfs_status pfs;
  char etype[25];
  struct ex_types *funcs;
  int fd;
  int in_exec = 0;

  prog = av[0];

  while ((c = getopt(ac, av, "p:o:S")) != EOF) {
    switch (c) {
    case 'p':	/* specified pid */
      pid = atoi(optarg);
      break;
    case 'o':	/* Specified output file */
      if ((outfile = fopen(optarg, "w")) == NULL) {
	fprintf (stderr, "%s:  cannot open %s\n", av[0], optarg);
	exit(1);
      }
      break;
    case 'S':	/* Don't trace signals */ 
      nosigs = 1;
      break;
    default:
      usage();
    }
  }

  ac -= optind; av += optind;
  if ((pid == 0 && ac == 0) || (pid != 0 && ac != 0))
    usage();

  /*
   * If truss starts the process itself, it will ignore some signals --
   * they should be passed off to the process, which may or may not
   * exit.  If, however, we are examining an already-running process,
   * then we restore the event mask on these same signals.
   */

  if (pid == 0) {	/* Start a command ourselves */
    command = av;
    pid = setup_and_wait(command);
    signal(SIGINT, SIG_IGN);
    signal(SIGTERM, SIG_IGN);
    signal(SIGQUIT, SIG_IGN);
  } else {
    extern void restore_proc(int);
    signal(SIGINT, restore_proc);
    signal(SIGTERM, restore_proc);
    signal(SIGQUIT, restore_proc);
  }


  /*
   * At this point, if we started the process, it is stopped waiting to
   * be woken up, either in exit() or in execve().
   */

  Procfd = start_tracing(pid, S_EXEC | S_SCE | S_SCX | S_CORE | S_EXIT |
		     (nosigs ? 0 : S_SIG));
  pfs.why = 0;

  funcs = set_etype();
  /*
   * At this point, it's a simple loop, waiting for the process to
   * stop, finding out why, printing out why, and then continuing it.
   * All of the grunt work is done in the support routines.
   */

  do {
    int val = 0;

    if (ioctl(Procfd, PIOCWAIT, &pfs) == -1)
      perror("PIOCWAIT top of loop");
    else {
      switch(i = pfs.why) {
      case S_SCE:
	funcs->enter_syscall(pid, pfs.val);
	break;
      case S_SCX:
	/*
	 * This is so we don't get two messages for an exec -- one
	 * for the S_EXEC, and one for the syscall exit.  It also,
	 * conveniently, ensures that the first message printed out
	 * isn't the return-from-syscall used to create the process.
	 */

	if (in_exec) {
	  in_exec = 0;
	  break;
	}
	funcs->exit_syscall(pid, pfs.val);
	break;
      case S_SIG:
	fprintf(outfile, "SIGNAL %d\n", pfs.val);
	break;
      case S_EXIT:
	fprintf (outfile, "process exit, rval = %d\n", pfs.val);
	break;
      case S_EXEC:
	funcs = set_etype();
	in_exec = 1;
	break;
      default:
	fprintf (outfile, "Process stopped because of:  %d\n", i);
	break;
      }
    }
    if (ioctl(Procfd, PIOCCONT, &val) == -1)
      perror("PIOCCONT");
  } while (pfs.why != S_EXIT);
  return 0;
}
