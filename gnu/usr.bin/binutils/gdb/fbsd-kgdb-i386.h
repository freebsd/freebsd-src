/* $FreeBSD$ */

#ifndef FBSD_KGDB_I386_H
#define FBSD_KGDB_I386_H

/* On FreeBSD, sigtramp has size 0x18 and is immediately below the
   ps_strings struct which has size 0x10 and is at the top of the
   user stack.  */

#undef  SIGTRAMP_START
#define SIGTRAMP_START(pc)	0xbfbfdfd8
#undef  SIGTRAMP_END
#define SIGTRAMP_END(pc)	0xbfbfdff0
 
 
/* Override FRAME_SAVED_PC to enable the recognition of signal handlers.  */

extern CORE_ADDR fbsd_kern_frame_saved_pc(struct frame_info *fr);

#undef  FRAME_SAVED_PC
#define FRAME_SAVED_PC(FRAME) \
  (kernel_debugging \
    ? fbsd_kern_frame_saved_pc (FRAME) : \
    (FRAME)->signal_handler_caller \
      ? sigtramp_saved_pc (FRAME) \
      : read_memory_integer ((FRAME)->frame + 4, 4))

/* Offset to saved PC in sigcontext, from <sys/signal.h>.  */
#define SIGCONTEXT_PC_OFFSET 20

#endif /* FBSD_KGDB_I386_H */
