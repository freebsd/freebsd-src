/* $FreeBSD$ */

#include "defs.h"

/* Size of elements in jmpbuf */

#define JB_ELEMENT_SIZE 8

#define JB_PC 2

int kernel_debugging = 0;

fetch_kcore_registers (pcbp)
  struct pcb *pcbp;
{
  return;
}

void
fetch_inferior_registers (regno)
     int regno;
{
  return;
}

void
store_inferior_registers (regno)
     int regno;
{
  return;
}

/* From gdb/alpha-nat.c.  */

/* Figure out where the longjmp will land.
   We expect the first arg to be a pointer to the jmp_buf structure from which
   we extract the pc (JB_PC) that we will land at.  The pc is copied into PC.
   This routine returns true on success. */

int
get_longjmp_target (pc)
     CORE_ADDR *pc;
{
  CORE_ADDR jb_addr;
  char raw_buffer[MAX_REGISTER_RAW_SIZE];

  jb_addr = read_register(A0_REGNUM);

  if (target_read_memory(jb_addr + JB_PC * JB_ELEMENT_SIZE, raw_buffer,
			 sizeof(CORE_ADDR)))
    return 0;

  *pc = extract_address (raw_buffer, sizeof(CORE_ADDR));
  return 1;
}
