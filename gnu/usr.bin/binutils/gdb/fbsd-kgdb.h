/* Native-kernel debugging definitions for FreeBSD.
 * $FreeBSD$ 
 */

#ifndef FBSD_KGDB_H
#define FBSD_KGDB_H

extern int kernel_debugging;
extern int kernel_writablecore;

#define ADDITIONAL_OPTIONS \
       {"kernel", no_argument, &kernel_debugging, 1}, \
       {"k", no_argument, &kernel_debugging, 1}, \
       {"wcore", no_argument, &kernel_writablecore, 1}, \
       {"w", no_argument, &kernel_writablecore, 1},

#define ADDITIONAL_OPTION_HELP \
       "\
  --kernel           Enable kernel debugging.\n\
  --wcore            Make core file writable (only works for /dev/mem).\n\
                     This option only works while debugging a kernel !!\n\
"

#define DEFAULT_PROMPT kernel_debugging?"(kgdb) ":"(gdb) "

/* misuse START_PROGRESS to test whether we're running as kgdb */   
/* START_PROGRESS is called at the top of main */
#undef START_PROGRESS
#define START_PROGRESS(STR,N) \
  if (!strcmp (STR, "kgdb")) \
     kernel_debugging = 1;

#endif /* FBSD_KGDB_H */
