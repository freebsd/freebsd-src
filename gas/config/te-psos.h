/* This file is te-psos.h for embedded systems running pSOS.
   Contributed by Martin Anantharaman (martin@mail.imech.uni-duisburg.de).  */

#define TE_PSOS

/* Added these, because if we don't know what we're targeting we may
   need an assembler version of libgcc, and that will use local
   labels.  */
#define LOCAL_LABELS_DOLLAR 1
#define LOCAL_LABELS_FB 1

/* This makes GAS more versatile and blocks some ELF'isms in
   tc-m68k.h.  */
#define REGISTER_PREFIX_OPTIONAL 1

#include "obj-format.h"

/* end of te-psos.h */
