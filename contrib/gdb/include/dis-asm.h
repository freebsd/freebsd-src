/* Interface between the opcode library and its callers.
   Written by Cygnus Support, 1993.

   The opcode library (libopcodes.a) provides instruction decoders for
   a large variety of instruction sets, callable with an identical
   interface, for making instruction-processing programs more independent
   of the instruction set being processed.  */

#ifndef DIS_ASM_H
#define DIS_ASM_H

#include <stdio.h>
#include "bfd.h"

typedef int (*fprintf_ftype) PARAMS((FILE*, const char*, ...));

enum dis_insn_type {
  dis_noninsn,			/* Not a valid instruction */
  dis_nonbranch,		/* Not a branch instruction */
  dis_branch,			/* Unconditional branch */
  dis_condbranch,		/* Conditional branch */
  dis_jsr,			/* Jump to subroutine */
  dis_condjsr,			/* Conditional jump to subroutine */
  dis_dref,			/* Data reference instruction */
  dis_dref2			/* Two data references in instruction */
};

/* This struct is passed into the instruction decoding routine, 
   and is passed back out into each callback.  The various fields are used
   for conveying information from your main routine into your callbacks,
   for passing information into the instruction decoders (such as the
   addresses of the callback functions), or for passing information
   back from the instruction decoders to their callers.

   It must be initialized before it is first passed; this can be done
   by hand, or using one of the initialization macros below.  */

typedef struct disassemble_info {
  fprintf_ftype fprintf_func;
  FILE *stream;
  PTR application_data;

  /* Target description.  We could replace this with a pointer to the bfd,
     but that would require one.  There currently isn't any such requirement
     so to avoid introducing one we record these explicitly.  */
  /* The bfd_arch value.  */
  enum bfd_architecture arch;
  /* The bfd_mach value.  */
  unsigned long mach;
  /* Endianness (for bi-endian cpus).  Mono-endian cpus can ignore this.  */
  enum bfd_endian endian;

  /* For use by the disassembler.
     The top 16 bits are reserved for public use (and are documented here).
     The bottom 16 bits are for the internal use of the disassembler.  */
  unsigned long flags;
  PTR private_data;

  /* Function used to get bytes to disassemble.  MEMADDR is the
     address of the stuff to be disassembled, MYADDR is the address to
     put the bytes in, and LENGTH is the number of bytes to read.
     INFO is a pointer to this struct.
     Returns an errno value or 0 for success.  */
  int (*read_memory_func)
    PARAMS ((bfd_vma memaddr, bfd_byte *myaddr, int length,
	     struct disassemble_info *info));

  /* Function which should be called if we get an error that we can't
     recover from.  STATUS is the errno value from read_memory_func and
     MEMADDR is the address that we were trying to read.  INFO is a
     pointer to this struct.  */
  void (*memory_error_func)
    PARAMS ((int status, bfd_vma memaddr, struct disassemble_info *info));

  /* Function called to print ADDR.  */
  void (*print_address_func)
    PARAMS ((bfd_vma addr, struct disassemble_info *info));

  /* These are for buffer_read_memory.  */
  bfd_byte *buffer;
  bfd_vma buffer_vma;
  int buffer_length;

  /* Results from instruction decoders.  Not all decoders yet support
     this information.  This info is set each time an instruction is
     decoded, and is only valid for the last such instruction.

     To determine whether this decoder supports this information, set
     insn_info_valid to 0, decode an instruction, then check it.  */

  char insn_info_valid;		/* Branch info has been set. */
  char branch_delay_insns;	/* How many sequential insn's will run before
				   a branch takes effect.  (0 = normal) */
  char data_size;		/* Size of data reference in insn, in bytes */
  enum dis_insn_type insn_type;	/* Type of instruction */
  bfd_vma target;		/* Target address of branch or dref, if known;
				   zero if unknown.  */
  bfd_vma target2;		/* Second target address for dref2 */

} disassemble_info;


/* Standard disassemblers.  Disassemble one instruction at the given
   target address.  Return number of bytes processed.  */
typedef int (*disassembler_ftype)
     PARAMS((bfd_vma, disassemble_info *));

extern int print_insn_big_mips		PARAMS ((bfd_vma, disassemble_info*));
extern int print_insn_little_mips	PARAMS ((bfd_vma, disassemble_info*));
extern int print_insn_i386		PARAMS ((bfd_vma, disassemble_info*));
extern int print_insn_m68k		PARAMS ((bfd_vma, disassemble_info*));
extern int print_insn_z8001		PARAMS ((bfd_vma, disassemble_info*));
extern int print_insn_z8002		PARAMS ((bfd_vma, disassemble_info*));
extern int print_insn_h8300		PARAMS ((bfd_vma, disassemble_info*));
extern int print_insn_h8300h		PARAMS ((bfd_vma, disassemble_info*));
extern int print_insn_h8500		PARAMS ((bfd_vma, disassemble_info*));
extern int print_insn_alpha		PARAMS ((bfd_vma, disassemble_info*));
extern int print_insn_big_arm		PARAMS ((bfd_vma, disassemble_info*));
extern int print_insn_little_arm	PARAMS ((bfd_vma, disassemble_info*));
extern int print_insn_sparc		PARAMS ((bfd_vma, disassemble_info*));
extern int print_insn_sparc64		PARAMS ((bfd_vma, disassemble_info*));
extern int print_insn_big_a29k		PARAMS ((bfd_vma, disassemble_info*));
extern int print_insn_little_a29k	PARAMS ((bfd_vma, disassemble_info*));
extern int print_insn_i960		PARAMS ((bfd_vma, disassemble_info*));
extern int print_insn_sh		PARAMS ((bfd_vma, disassemble_info*));
extern int print_insn_shl		PARAMS ((bfd_vma, disassemble_info*));
extern int print_insn_hppa		PARAMS ((bfd_vma, disassemble_info*));
extern int print_insn_m88k		PARAMS ((bfd_vma, disassemble_info*));
extern int print_insn_ns32k		PARAMS ((bfd_vma, disassemble_info*));
extern int print_insn_big_powerpc	PARAMS ((bfd_vma, disassemble_info*));
extern int print_insn_little_powerpc	PARAMS ((bfd_vma, disassemble_info*));
extern int print_insn_rs6000		PARAMS ((bfd_vma, disassemble_info*));
extern int print_insn_w65		PARAMS ((bfd_vma, disassemble_info*));

/* Fetch the disassembler for a given BFD, if that support is available.  */
extern disassembler_ftype disassembler	PARAMS ((bfd *));


/* This block of definitions is for particular callers who read instructions
   into a buffer before calling the instruction decoder.  */

/* Here is a function which callers may wish to use for read_memory_func.
   It gets bytes from a buffer.  */
extern int buffer_read_memory
  PARAMS ((bfd_vma, bfd_byte *, int, struct disassemble_info *));

/* This function goes with buffer_read_memory.
   It prints a message using info->fprintf_func and info->stream.  */
extern void perror_memory PARAMS ((int, bfd_vma, struct disassemble_info *));


/* Just print the address in hex.  This is included for completeness even
   though both GDB and objdump provide their own (to print symbolic
   addresses).  */
extern void generic_print_address
  PARAMS ((bfd_vma, struct disassemble_info *));

/* Macro to initialize a disassemble_info struct.  This should be called
   by all applications creating such a struct.  */
#define INIT_DISASSEMBLE_INFO(INFO, STREAM, FPRINTF_FUNC) \
  (INFO).fprintf_func = (FPRINTF_FUNC), \
  (INFO).stream = (STREAM), \
  (INFO).buffer = NULL, \
  (INFO).buffer_vma = 0, \
  (INFO).buffer_length = 0, \
  (INFO).read_memory_func = buffer_read_memory, \
  (INFO).memory_error_func = perror_memory, \
  (INFO).print_address_func = generic_print_address, \
  (INFO).arch = bfd_arch_unknown, \
  (INFO).mach = 0, \
  (INFO).endian = BFD_ENDIAN_UNKNOWN, \
  (INFO).flags = 0, \
  (INFO).insn_info_valid = 0

#endif /* ! defined (DIS_ASM_H) */
