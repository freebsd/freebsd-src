/* Target-dependent code for the IA-64 for GDB, the GNU debugger.

   Copyright 1999, 2000, 2001, 2002 Free Software Foundation, Inc.

   This file is part of GDB.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

#include "defs.h"
#include "inferior.h"
#include "symfile.h"		/* for entry_point_address */
#include "gdbcore.h"
#include "arch-utils.h"
#include "floatformat.h"
#include "regcache.h"
#include "doublest.h"
#include "value.h"

#include "objfiles.h"
#include "elf/common.h"		/* for DT_PLTGOT value */
#include "elf-bfd.h"

/* Hook for determining the global pointer when calling functions in
   the inferior under AIX.  The initialization code in ia64-aix-nat.c
   sets this hook to the address of a function which will find the
   global pointer for a given address.  
   
   The generic code which uses the dynamic section in the inferior for
   finding the global pointer is not of much use on AIX since the
   values obtained from the inferior have not been relocated.  */

CORE_ADDR (*native_find_global_pointer) (CORE_ADDR) = 0;

/* An enumeration of the different IA-64 instruction types.  */

typedef enum instruction_type
{
  A,			/* Integer ALU ;    I-unit or M-unit */
  I,			/* Non-ALU integer; I-unit */
  M,			/* Memory ;         M-unit */
  F,			/* Floating-point ; F-unit */
  B,			/* Branch ;         B-unit */
  L,			/* Extended (L+X) ; I-unit */
  X,			/* Extended (L+X) ; I-unit */
  undefined		/* undefined or reserved */
} instruction_type;

/* We represent IA-64 PC addresses as the value of the instruction
   pointer or'd with some bit combination in the low nibble which
   represents the slot number in the bundle addressed by the
   instruction pointer.  The problem is that the Linux kernel
   multiplies its slot numbers (for exceptions) by one while the
   disassembler multiplies its slot numbers by 6.  In addition, I've
   heard it said that the simulator uses 1 as the multiplier.
   
   I've fixed the disassembler so that the bytes_per_line field will
   be the slot multiplier.  If bytes_per_line comes in as zero, it
   is set to six (which is how it was set up initially). -- objdump
   displays pretty disassembly dumps with this value.  For our purposes,
   we'll set bytes_per_line to SLOT_MULTIPLIER. This is okay since we
   never want to also display the raw bytes the way objdump does. */

#define SLOT_MULTIPLIER 1

/* Length in bytes of an instruction bundle */

#define BUNDLE_LEN 16

/* FIXME: These extern declarations should go in ia64-tdep.h.  */
extern CORE_ADDR ia64_linux_sigcontext_register_address (CORE_ADDR, int);
extern CORE_ADDR ia64_aix_sigcontext_register_address (CORE_ADDR, int);

static gdbarch_init_ftype ia64_gdbarch_init;

static gdbarch_register_name_ftype ia64_register_name;
static gdbarch_register_raw_size_ftype ia64_register_raw_size;
static gdbarch_register_virtual_size_ftype ia64_register_virtual_size;
static gdbarch_register_virtual_type_ftype ia64_register_virtual_type;
static gdbarch_register_byte_ftype ia64_register_byte;
static gdbarch_breakpoint_from_pc_ftype ia64_breakpoint_from_pc;
static gdbarch_frame_chain_ftype ia64_frame_chain;
static gdbarch_frame_saved_pc_ftype ia64_frame_saved_pc;
static gdbarch_skip_prologue_ftype ia64_skip_prologue;
static gdbarch_frame_init_saved_regs_ftype ia64_frame_init_saved_regs;
static gdbarch_get_saved_register_ftype ia64_get_saved_register;
static gdbarch_extract_return_value_ftype ia64_extract_return_value;
static gdbarch_extract_struct_value_address_ftype ia64_extract_struct_value_address;
static gdbarch_use_struct_convention_ftype ia64_use_struct_convention;
static gdbarch_frameless_function_invocation_ftype ia64_frameless_function_invocation;
static gdbarch_init_extra_frame_info_ftype ia64_init_extra_frame_info;
static gdbarch_store_return_value_ftype ia64_store_return_value;
static gdbarch_store_struct_return_ftype ia64_store_struct_return;
static gdbarch_push_arguments_ftype ia64_push_arguments;
static gdbarch_push_return_address_ftype ia64_push_return_address;
static gdbarch_pop_frame_ftype ia64_pop_frame;
static gdbarch_saved_pc_after_call_ftype ia64_saved_pc_after_call;
static void ia64_pop_frame_regular (struct frame_info *frame);
static struct type *is_float_or_hfa_type (struct type *t);

static int ia64_num_regs = 590;

static int pc_regnum = IA64_IP_REGNUM;
static int sp_regnum = IA64_GR12_REGNUM;
static int fp_regnum = IA64_VFP_REGNUM;
static int lr_regnum = IA64_VRAP_REGNUM;

static LONGEST ia64_call_dummy_words[] = {0};

/* Array of register names; There should be ia64_num_regs strings in
   the initializer.  */

static char *ia64_register_names[] = 
{ "r0",   "r1",   "r2",   "r3",   "r4",   "r5",   "r6",   "r7",
  "r8",   "r9",   "r10",  "r11",  "r12",  "r13",  "r14",  "r15",
  "r16",  "r17",  "r18",  "r19",  "r20",  "r21",  "r22",  "r23",
  "r24",  "r25",  "r26",  "r27",  "r28",  "r29",  "r30",  "r31",
  "r32",  "r33",  "r34",  "r35",  "r36",  "r37",  "r38",  "r39",
  "r40",  "r41",  "r42",  "r43",  "r44",  "r45",  "r46",  "r47",
  "r48",  "r49",  "r50",  "r51",  "r52",  "r53",  "r54",  "r55",
  "r56",  "r57",  "r58",  "r59",  "r60",  "r61",  "r62",  "r63",
  "r64",  "r65",  "r66",  "r67",  "r68",  "r69",  "r70",  "r71",
  "r72",  "r73",  "r74",  "r75",  "r76",  "r77",  "r78",  "r79",
  "r80",  "r81",  "r82",  "r83",  "r84",  "r85",  "r86",  "r87",
  "r88",  "r89",  "r90",  "r91",  "r92",  "r93",  "r94",  "r95",
  "r96",  "r97",  "r98",  "r99",  "r100", "r101", "r102", "r103",
  "r104", "r105", "r106", "r107", "r108", "r109", "r110", "r111",
  "r112", "r113", "r114", "r115", "r116", "r117", "r118", "r119",
  "r120", "r121", "r122", "r123", "r124", "r125", "r126", "r127",

  "f0",   "f1",   "f2",   "f3",   "f4",   "f5",   "f6",   "f7",
  "f8",   "f9",   "f10",  "f11",  "f12",  "f13",  "f14",  "f15",
  "f16",  "f17",  "f18",  "f19",  "f20",  "f21",  "f22",  "f23",
  "f24",  "f25",  "f26",  "f27",  "f28",  "f29",  "f30",  "f31",
  "f32",  "f33",  "f34",  "f35",  "f36",  "f37",  "f38",  "f39",
  "f40",  "f41",  "f42",  "f43",  "f44",  "f45",  "f46",  "f47",
  "f48",  "f49",  "f50",  "f51",  "f52",  "f53",  "f54",  "f55",
  "f56",  "f57",  "f58",  "f59",  "f60",  "f61",  "f62",  "f63",
  "f64",  "f65",  "f66",  "f67",  "f68",  "f69",  "f70",  "f71",
  "f72",  "f73",  "f74",  "f75",  "f76",  "f77",  "f78",  "f79",
  "f80",  "f81",  "f82",  "f83",  "f84",  "f85",  "f86",  "f87",
  "f88",  "f89",  "f90",  "f91",  "f92",  "f93",  "f94",  "f95",
  "f96",  "f97",  "f98",  "f99",  "f100", "f101", "f102", "f103",
  "f104", "f105", "f106", "f107", "f108", "f109", "f110", "f111",
  "f112", "f113", "f114", "f115", "f116", "f117", "f118", "f119",
  "f120", "f121", "f122", "f123", "f124", "f125", "f126", "f127",

  "p0",   "p1",   "p2",   "p3",   "p4",   "p5",   "p6",   "p7",
  "p8",   "p9",   "p10",  "p11",  "p12",  "p13",  "p14",  "p15",
  "p16",  "p17",  "p18",  "p19",  "p20",  "p21",  "p22",  "p23",
  "p24",  "p25",  "p26",  "p27",  "p28",  "p29",  "p30",  "p31",
  "p32",  "p33",  "p34",  "p35",  "p36",  "p37",  "p38",  "p39",
  "p40",  "p41",  "p42",  "p43",  "p44",  "p45",  "p46",  "p47",
  "p48",  "p49",  "p50",  "p51",  "p52",  "p53",  "p54",  "p55",
  "p56",  "p57",  "p58",  "p59",  "p60",  "p61",  "p62",  "p63",

  "b0",   "b1",   "b2",   "b3",   "b4",   "b5",   "b6",   "b7",

  "vfp", "vrap",

  "pr", "ip", "psr", "cfm",

  "kr0",   "kr1",   "kr2",   "kr3",   "kr4",   "kr5",   "kr6",   "kr7",
  "", "", "", "", "", "", "", "",
  "rsc", "bsp", "bspstore", "rnat",
  "", "fcr", "", "",
  "eflag", "csd", "ssd", "cflg", "fsr", "fir", "fdr",  "",
  "ccv", "", "", "", "unat", "", "", "",
  "fpsr", "", "", "", "itc",
  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "",
  "pfs", "lc", "ec",
  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "",
  "",
  "nat0",  "nat1",  "nat2",  "nat3",  "nat4",  "nat5",  "nat6",  "nat7",
  "nat8",  "nat9",  "nat10", "nat11", "nat12", "nat13", "nat14", "nat15",
  "nat16", "nat17", "nat18", "nat19", "nat20", "nat21", "nat22", "nat23",
  "nat24", "nat25", "nat26", "nat27", "nat28", "nat29", "nat30", "nat31",
  "nat32", "nat33", "nat34", "nat35", "nat36", "nat37", "nat38", "nat39",
  "nat40", "nat41", "nat42", "nat43", "nat44", "nat45", "nat46", "nat47",
  "nat48", "nat49", "nat50", "nat51", "nat52", "nat53", "nat54", "nat55",
  "nat56", "nat57", "nat58", "nat59", "nat60", "nat61", "nat62", "nat63",
  "nat64", "nat65", "nat66", "nat67", "nat68", "nat69", "nat70", "nat71",
  "nat72", "nat73", "nat74", "nat75", "nat76", "nat77", "nat78", "nat79",
  "nat80", "nat81", "nat82", "nat83", "nat84", "nat85", "nat86", "nat87",
  "nat88", "nat89", "nat90", "nat91", "nat92", "nat93", "nat94", "nat95",
  "nat96", "nat97", "nat98", "nat99", "nat100","nat101","nat102","nat103",
  "nat104","nat105","nat106","nat107","nat108","nat109","nat110","nat111",
  "nat112","nat113","nat114","nat115","nat116","nat117","nat118","nat119",
  "nat120","nat121","nat122","nat123","nat124","nat125","nat126","nat127",
};

struct frame_extra_info
  {
    CORE_ADDR bsp;	/* points at r32 for the current frame */
    CORE_ADDR cfm;	/* cfm value for current frame */
    int sof;		/* Size of frame  (decoded from cfm value) */
    int	sol;		/* Size of locals (decoded from cfm value) */
    CORE_ADDR after_prologue;
			/* Address of first instruction after the last
			   prologue instruction;  Note that there may
			   be instructions from the function's body
			   intermingled with the prologue. */
    int mem_stack_frame_size;
			/* Size of the memory stack frame (may be zero),
			   or -1 if it has not been determined yet. */
    int	fp_reg;		/* Register number (if any) used a frame pointer
			   for this frame.  0 if no register is being used
			   as the frame pointer. */
  };

struct gdbarch_tdep
  {
    int os_ident;	/* From the ELF header, one of the ELFOSABI_
                           constants: ELFOSABI_LINUX, ELFOSABI_AIX,
			   etc. */
    CORE_ADDR (*sigcontext_register_address) (CORE_ADDR, int);
    			/* OS specific function which, given a frame address
			   and register number, returns the offset to the
			   given register from the start of the frame. */
    CORE_ADDR (*find_global_pointer) (CORE_ADDR);
  };

#define SIGCONTEXT_REGISTER_ADDRESS \
  (gdbarch_tdep (current_gdbarch)->sigcontext_register_address)
#define FIND_GLOBAL_POINTER \
  (gdbarch_tdep (current_gdbarch)->find_global_pointer)

static char *
ia64_register_name (int reg)
{
  return ia64_register_names[reg];
}

int
ia64_register_raw_size (int reg)
{
  return (IA64_FR0_REGNUM <= reg && reg <= IA64_FR127_REGNUM) ? 16 : 8;
}

int
ia64_register_virtual_size (int reg)
{
  return (IA64_FR0_REGNUM <= reg && reg <= IA64_FR127_REGNUM) ? 16 : 8;
}

/* Return true iff register N's virtual format is different from
   its raw format. */
int
ia64_register_convertible (int nr)
{
  return (IA64_FR0_REGNUM <= nr && nr <= IA64_FR127_REGNUM);
}

const struct floatformat floatformat_ia64_ext =
{
  floatformat_little, 82, 0, 1, 17, 65535, 0x1ffff, 18, 64,
  floatformat_intbit_yes
};

void
ia64_register_convert_to_virtual (int regnum, struct type *type,
                                  char *from, char *to)
{
  if (regnum >= IA64_FR0_REGNUM && regnum <= IA64_FR127_REGNUM)
    {
      DOUBLEST val;
      floatformat_to_doublest (&floatformat_ia64_ext, from, &val);
      store_floating(to, TYPE_LENGTH(type), val);
    }
  else
    error("ia64_register_convert_to_virtual called with non floating point register number");
}

void
ia64_register_convert_to_raw (struct type *type, int regnum,
                              char *from, char *to)
{
  if (regnum >= IA64_FR0_REGNUM && regnum <= IA64_FR127_REGNUM)
    {
      DOUBLEST val = extract_floating (from, TYPE_LENGTH(type));
      floatformat_from_doublest (&floatformat_ia64_ext, &val, to);
    }
  else
    error("ia64_register_convert_to_raw called with non floating point register number");
}

struct type *
ia64_register_virtual_type (int reg)
{
  if (reg >= IA64_FR0_REGNUM && reg <= IA64_FR127_REGNUM)
    return builtin_type_long_double;
  else
    return builtin_type_long;
}

int
ia64_register_byte (int reg)
{
  return (8 * reg) +
   (reg <= IA64_FR0_REGNUM ? 0 : 8 * ((reg > IA64_FR127_REGNUM) ? 128 : reg - IA64_FR0_REGNUM));
}

/* Read the given register from a sigcontext structure in the
   specified frame.  */

static CORE_ADDR
read_sigcontext_register (struct frame_info *frame, int regnum)
{
  CORE_ADDR regaddr;

  if (frame == NULL)
    internal_error (__FILE__, __LINE__,
		    "read_sigcontext_register: NULL frame");
  if (!frame->signal_handler_caller)
    internal_error (__FILE__, __LINE__,
		    "read_sigcontext_register: frame not a signal_handler_caller");
  if (SIGCONTEXT_REGISTER_ADDRESS == 0)
    internal_error (__FILE__, __LINE__,
		    "read_sigcontext_register: SIGCONTEXT_REGISTER_ADDRESS is 0");

  regaddr = SIGCONTEXT_REGISTER_ADDRESS (frame->frame, regnum);
  if (regaddr)
    return read_memory_integer (regaddr, REGISTER_RAW_SIZE (regnum));
  else
    internal_error (__FILE__, __LINE__,
		    "read_sigcontext_register: Register %d not in struct sigcontext", regnum);
}

/* Extract ``len'' bits from an instruction bundle starting at
   bit ``from''.  */

static long long
extract_bit_field (char *bundle, int from, int len)
{
  long long result = 0LL;
  int to = from + len;
  int from_byte = from / 8;
  int to_byte = to / 8;
  unsigned char *b = (unsigned char *) bundle;
  unsigned char c;
  int lshift;
  int i;

  c = b[from_byte];
  if (from_byte == to_byte)
    c = ((unsigned char) (c << (8 - to % 8))) >> (8 - to % 8);
  result = c >> (from % 8);
  lshift = 8 - (from % 8);

  for (i = from_byte+1; i < to_byte; i++)
    {
      result |= ((long long) b[i]) << lshift;
      lshift += 8;
    }

  if (from_byte < to_byte && (to % 8 != 0))
    {
      c = b[to_byte];
      c = ((unsigned char) (c << (8 - to % 8))) >> (8 - to % 8);
      result |= ((long long) c) << lshift;
    }

  return result;
}

/* Replace the specified bits in an instruction bundle */

static void
replace_bit_field (char *bundle, long long val, int from, int len)
{
  int to = from + len;
  int from_byte = from / 8;
  int to_byte = to / 8;
  unsigned char *b = (unsigned char *) bundle;
  unsigned char c;

  if (from_byte == to_byte)
    {
      unsigned char left, right;
      c = b[from_byte];
      left = (c >> (to % 8)) << (to % 8);
      right = ((unsigned char) (c << (8 - from % 8))) >> (8 - from % 8);
      c = (unsigned char) (val & 0xff);
      c = (unsigned char) (c << (from % 8 + 8 - to % 8)) >> (8 - to % 8);
      c |= right | left;
      b[from_byte] = c;
    }
  else
    {
      int i;
      c = b[from_byte];
      c = ((unsigned char) (c << (8 - from % 8))) >> (8 - from % 8);
      c = c | (val << (from % 8));
      b[from_byte] = c;
      val >>= 8 - from % 8;

      for (i = from_byte+1; i < to_byte; i++)
	{
	  c = val & 0xff;
	  val >>= 8;
	  b[i] = c;
	}

      if (to % 8 != 0)
	{
	  unsigned char cv = (unsigned char) val;
	  c = b[to_byte];
	  c = c >> (to % 8) << (to % 8);
	  c |= ((unsigned char) (cv << (8 - to % 8))) >> (8 - to % 8);
	  b[to_byte] = c;
	}
    }
}

/* Return the contents of slot N (for N = 0, 1, or 2) in
   and instruction bundle */

static long long
slotN_contents (char *bundle, int slotnum)
{
  return extract_bit_field (bundle, 5+41*slotnum, 41);
}

/* Store an instruction in an instruction bundle */

static void
replace_slotN_contents (char *bundle, long long instr, int slotnum)
{
  replace_bit_field (bundle, instr, 5+41*slotnum, 41);
}

static enum instruction_type template_encoding_table[32][3] =
{
  { M, I, I },				/* 00 */
  { M, I, I },				/* 01 */
  { M, I, I },				/* 02 */
  { M, I, I },				/* 03 */
  { M, L, X },				/* 04 */
  { M, L, X },				/* 05 */
  { undefined, undefined, undefined },  /* 06 */
  { undefined, undefined, undefined },  /* 07 */
  { M, M, I },				/* 08 */
  { M, M, I },				/* 09 */
  { M, M, I },				/* 0A */
  { M, M, I },				/* 0B */
  { M, F, I },				/* 0C */
  { M, F, I },				/* 0D */
  { M, M, F },				/* 0E */
  { M, M, F },				/* 0F */
  { M, I, B },				/* 10 */
  { M, I, B },				/* 11 */
  { M, B, B },				/* 12 */
  { M, B, B },				/* 13 */
  { undefined, undefined, undefined },  /* 14 */
  { undefined, undefined, undefined },  /* 15 */
  { B, B, B },				/* 16 */
  { B, B, B },				/* 17 */
  { M, M, B },				/* 18 */
  { M, M, B },				/* 19 */
  { undefined, undefined, undefined },  /* 1A */
  { undefined, undefined, undefined },  /* 1B */
  { M, F, B },				/* 1C */
  { M, F, B },				/* 1D */
  { undefined, undefined, undefined },  /* 1E */
  { undefined, undefined, undefined },  /* 1F */
};

/* Fetch and (partially) decode an instruction at ADDR and return the
   address of the next instruction to fetch.  */

static CORE_ADDR
fetch_instruction (CORE_ADDR addr, instruction_type *it, long long *instr)
{
  char bundle[BUNDLE_LEN];
  int slotnum = (int) (addr & 0x0f) / SLOT_MULTIPLIER;
  long long template;
  int val;

  /* Warn about slot numbers greater than 2.  We used to generate
     an error here on the assumption that the user entered an invalid
     address.  But, sometimes GDB itself requests an invalid address.
     This can (easily) happen when execution stops in a function for
     which there are no symbols.  The prologue scanner will attempt to
     find the beginning of the function - if the nearest symbol
     happens to not be aligned on a bundle boundary (16 bytes), the
     resulting starting address will cause GDB to think that the slot
     number is too large.

     So we warn about it and set the slot number to zero.  It is
     not necessarily a fatal condition, particularly if debugging
     at the assembly language level.  */
  if (slotnum > 2)
    {
      warning ("Can't fetch instructions for slot numbers greater than 2.\n"
	       "Using slot 0 instead");
      slotnum = 0;
    }

  addr &= ~0x0f;

  val = target_read_memory (addr, bundle, BUNDLE_LEN);

  if (val != 0)
    return 0;

  *instr = slotN_contents (bundle, slotnum);
  template = extract_bit_field (bundle, 0, 5);
  *it = template_encoding_table[(int)template][slotnum];

  if (slotnum == 2 || (slotnum == 1 && *it == L))
    addr += 16;
  else
    addr += (slotnum + 1) * SLOT_MULTIPLIER;

  return addr;
}

/* There are 5 different break instructions (break.i, break.b,
   break.m, break.f, and break.x), but they all have the same
   encoding.  (The five bit template in the low five bits of the
   instruction bundle distinguishes one from another.)
   
   The runtime architecture manual specifies that break instructions
   used for debugging purposes must have the upper two bits of the 21
   bit immediate set to a 0 and a 1 respectively.  A breakpoint
   instruction encodes the most significant bit of its 21 bit
   immediate at bit 36 of the 41 bit instruction.  The penultimate msb
   is at bit 25 which leads to the pattern below.  
   
   Originally, I had this set up to do, e.g, a "break.i 0x80000"  But
   it turns out that 0x80000 was used as the syscall break in the early
   simulators.  So I changed the pattern slightly to do "break.i 0x080001"
   instead.  But that didn't work either (I later found out that this
   pattern was used by the simulator that I was using.)  So I ended up
   using the pattern seen below. */

#if 0
#define BREAKPOINT 0x00002000040LL
#endif
#define BREAKPOINT 0x00003333300LL

static int
ia64_memory_insert_breakpoint (CORE_ADDR addr, char *contents_cache)
{
  char bundle[BUNDLE_LEN];
  int slotnum = (int) (addr & 0x0f) / SLOT_MULTIPLIER;
  long long instr;
  int val;

  if (slotnum > 2)
    error("Can't insert breakpoint for slot numbers greater than 2.");

  addr &= ~0x0f;

  val = target_read_memory (addr, bundle, BUNDLE_LEN);
  instr = slotN_contents (bundle, slotnum);
  memcpy(contents_cache, &instr, sizeof(instr));
  replace_slotN_contents (bundle, BREAKPOINT, slotnum);
  if (val == 0)
    target_write_memory (addr, bundle, BUNDLE_LEN);

  return val;
}

static int
ia64_memory_remove_breakpoint (CORE_ADDR addr, char *contents_cache)
{
  char bundle[BUNDLE_LEN];
  int slotnum = (addr & 0x0f) / SLOT_MULTIPLIER;
  long long instr;
  int val;

  addr &= ~0x0f;

  val = target_read_memory (addr, bundle, BUNDLE_LEN);
  memcpy (&instr, contents_cache, sizeof instr);
  replace_slotN_contents (bundle, instr, slotnum);
  if (val == 0)
    target_write_memory (addr, bundle, BUNDLE_LEN);

  return val;
}

/* We don't really want to use this, but remote.c needs to call it in order
   to figure out if Z-packets are supported or not.  Oh, well. */
unsigned char *
ia64_breakpoint_from_pc (CORE_ADDR *pcptr, int *lenptr)
{
  static unsigned char breakpoint[] =
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
  *lenptr = sizeof (breakpoint);
#if 0
  *pcptr &= ~0x0f;
#endif
  return breakpoint;
}

CORE_ADDR
ia64_read_pc (ptid_t ptid)
{
  CORE_ADDR psr_value = read_register_pid (IA64_PSR_REGNUM, ptid);
  CORE_ADDR pc_value   = read_register_pid (IA64_IP_REGNUM, ptid);
  int slot_num = (psr_value >> 41) & 3;

  return pc_value | (slot_num * SLOT_MULTIPLIER);
}

void
ia64_write_pc (CORE_ADDR new_pc, ptid_t ptid)
{
  int slot_num = (int) (new_pc & 0xf) / SLOT_MULTIPLIER;
  CORE_ADDR psr_value = read_register_pid (IA64_PSR_REGNUM, ptid);
  psr_value &= ~(3LL << 41);
  psr_value |= (CORE_ADDR)(slot_num & 0x3) << 41;

  new_pc &= ~0xfLL;

  write_register_pid (IA64_PSR_REGNUM, psr_value, ptid);
  write_register_pid (IA64_IP_REGNUM, new_pc, ptid);
}

#define IS_NaT_COLLECTION_ADDR(addr) ((((addr) >> 3) & 0x3f) == 0x3f)

/* Returns the address of the slot that's NSLOTS slots away from
   the address ADDR. NSLOTS may be positive or negative. */
static CORE_ADDR
rse_address_add(CORE_ADDR addr, int nslots)
{
  CORE_ADDR new_addr;
  int mandatory_nat_slots = nslots / 63;
  int direction = nslots < 0 ? -1 : 1;

  new_addr = addr + 8 * (nslots + mandatory_nat_slots);

  if ((new_addr >> 9)  != ((addr + 8 * 64 * mandatory_nat_slots) >> 9))
    new_addr += 8 * direction;

  if (IS_NaT_COLLECTION_ADDR(new_addr))
    new_addr += 8 * direction;

  return new_addr;
}

/* The IA-64 frame chain is a bit odd.  We won't always have a frame
   pointer, so we use the SP value as the FP for the purpose of
   creating a frame.  There is sometimes a register (not fixed) which
   is used as a frame pointer.  When this register exists, it is not
   especially hard to determine which one is being used.  It isn't
   even really hard to compute the frame chain, but it can be
   computationally expensive.  So, instead of making life difficult
   (and slow), we pick a more convenient representation of the frame
   chain, knowing that we'll have to make some small adjustments
   in other places.  (E.g, note that read_fp() and write_fp() are
   actually read_sp() and write_sp() below in ia64_gdbarch_init()
   below.) 

   Okay, so what is the frame chain exactly?  It'll be the SP value
   at the time that the function in question was entered.

   Note that this *should* actually the frame pointer for the current
   function!  But as I note above, if we were to attempt to find the
   address of the beginning of the previous frame, we'd waste a lot
   of cycles for no good reason.  So instead, we simply choose to
   represent the frame chain as the end of the previous frame instead
   of the beginning.  */

CORE_ADDR
ia64_frame_chain (struct frame_info *frame)
{
  if (frame->signal_handler_caller)
    return read_sigcontext_register (frame, sp_regnum);
  else if (PC_IN_CALL_DUMMY (frame->pc, frame->frame, frame->frame))
    return frame->frame;
  else
    {
      FRAME_INIT_SAVED_REGS (frame);
      if (frame->saved_regs[IA64_VFP_REGNUM])
	return read_memory_integer (frame->saved_regs[IA64_VFP_REGNUM], 8);
      else
	return frame->frame + frame->extra_info->mem_stack_frame_size;
    }
}

CORE_ADDR
ia64_frame_saved_pc (struct frame_info *frame)
{
  if (frame->signal_handler_caller)
    return read_sigcontext_register (frame, pc_regnum);
  else if (PC_IN_CALL_DUMMY (frame->pc, frame->frame, frame->frame))
    return generic_read_register_dummy (frame->pc, frame->frame, pc_regnum);
  else
    {
      FRAME_INIT_SAVED_REGS (frame);

      if (frame->saved_regs[IA64_VRAP_REGNUM])
	return read_memory_integer (frame->saved_regs[IA64_VRAP_REGNUM], 8);
      else if (frame->next && frame->next->signal_handler_caller)
	return read_sigcontext_register (frame->next, IA64_BR0_REGNUM);
      else	/* either frameless, or not far enough along in the prologue... */
	return ia64_saved_pc_after_call (frame);
    }
}

/* Limit the number of skipped non-prologue instructions since examining
   of the prologue is expensive.  */
static int max_skip_non_prologue_insns = 10;

/* Given PC representing the starting address of a function, and
   LIM_PC which is the (sloppy) limit to which to scan when looking
   for a prologue, attempt to further refine this limit by using
   the line data in the symbol table.  If successful, a better guess
   on where the prologue ends is returned, otherwise the previous
   value of lim_pc is returned.  TRUST_LIMIT is a pointer to a flag
   which will be set to indicate whether the returned limit may be
   used with no further scanning in the event that the function is
   frameless.  */

static CORE_ADDR
refine_prologue_limit (CORE_ADDR pc, CORE_ADDR lim_pc, int *trust_limit)
{
  struct symtab_and_line prologue_sal;
  CORE_ADDR start_pc = pc;

  /* Start off not trusting the limit.  */
  *trust_limit = 0;

  prologue_sal = find_pc_line (pc, 0);
  if (prologue_sal.line != 0)
    {
      int i;
      CORE_ADDR addr = prologue_sal.end;

      /* Handle the case in which compiler's optimizer/scheduler
         has moved instructions into the prologue.  We scan ahead
	 in the function looking for address ranges whose corresponding
	 line number is less than or equal to the first one that we
	 found for the function.  (It can be less than when the
	 scheduler puts a body instruction before the first prologue
	 instruction.)  */
      for (i = 2 * max_skip_non_prologue_insns; 
           i > 0 && (lim_pc == 0 || addr < lim_pc);
	   i--)
        {
	  struct symtab_and_line sal;

	  sal = find_pc_line (addr, 0);
	  if (sal.line == 0)
	    break;
	  if (sal.line <= prologue_sal.line 
	      && sal.symtab == prologue_sal.symtab)
	    {
	      prologue_sal = sal;
	    }
	  addr = sal.end;
	}

      if (lim_pc == 0 || prologue_sal.end < lim_pc)
	{
	  lim_pc = prologue_sal.end;
	  if (start_pc == get_pc_function_start (lim_pc))
	    *trust_limit = 1;
	}
    }
  return lim_pc;
}

#define isScratch(_regnum_) ((_regnum_) == 2 || (_regnum_) == 3 \
  || (8 <= (_regnum_) && (_regnum_) <= 11) \
  || (14 <= (_regnum_) && (_regnum_) <= 31))
#define imm9(_instr_) \
  ( ((((_instr_) & 0x01000000000LL) ? -1 : 0) << 8) \
   | (((_instr_) & 0x00008000000LL) >> 20) \
   | (((_instr_) & 0x00000001fc0LL) >> 6))

static CORE_ADDR
examine_prologue (CORE_ADDR pc, CORE_ADDR lim_pc, struct frame_info *frame)
{
  CORE_ADDR next_pc;
  CORE_ADDR last_prologue_pc = pc;
  instruction_type it;
  long long instr;
  int do_fsr_stuff = 0;

  int cfm_reg  = 0;
  int ret_reg  = 0;
  int fp_reg   = 0;
  int unat_save_reg = 0;
  int pr_save_reg = 0;
  int mem_stack_frame_size = 0;
  int spill_reg   = 0;
  CORE_ADDR spill_addr = 0;
  char instores[8];
  char infpstores[8];
  int trust_limit;

  memset (instores, 0, sizeof instores);
  memset (infpstores, 0, sizeof infpstores);

  if (frame && !frame->saved_regs)
    {
      frame_saved_regs_zalloc (frame);
      do_fsr_stuff = 1;
    }

  if (frame 
      && !do_fsr_stuff
      && frame->extra_info->after_prologue != 0
      && frame->extra_info->after_prologue <= lim_pc)
    return frame->extra_info->after_prologue;

  lim_pc = refine_prologue_limit (pc, lim_pc, &trust_limit);

  /* Must start with an alloc instruction */
  next_pc = fetch_instruction (pc, &it, &instr);
  if (pc < lim_pc && next_pc 
      && it == M && ((instr & 0x1ee0000003fLL) == 0x02c00000000LL))
    {
      /* alloc */
      int sor = (int) ((instr & 0x00078000000LL) >> 27);
      int sol = (int) ((instr & 0x00007f00000LL) >> 20);
      int sof = (int) ((instr & 0x000000fe000LL) >> 13);
      /* Okay, so sor, sol, and sof aren't used right now; but perhaps
         we could compare against the size given to us via the cfm as
	 either a sanity check or possibly to see if the frame has been
	 changed by a later alloc instruction... */
      int rN = (int) ((instr & 0x00000001fc0LL) >> 6);
      cfm_reg = rN;
      last_prologue_pc = next_pc;
      pc = next_pc;
    }
  else
    {
      pc = lim_pc;	/* Frameless: We're done early.  */
      if (trust_limit)
	last_prologue_pc = lim_pc;
    }

  /* Loop, looking for prologue instructions, keeping track of
     where preserved registers were spilled. */
  while (pc < lim_pc)
    {
      next_pc = fetch_instruction (pc, &it, &instr);
      if (next_pc == 0)
	break;

      if ((it == B && ((instr & 0x1e1f800003f) != 0x04000000000))
          || ((instr & 0x3fLL) != 0LL))
	{
	  /* Exit loop upon hitting a non-nop branch instruction 
	     or a predicated instruction. */
	  break;
	}
      else if (it == I && ((instr & 0x1eff8000000LL) == 0x00188000000LL))
        {
	  /* Move from BR */
	  int b2 = (int) ((instr & 0x0000000e000LL) >> 13);
	  int rN = (int) ((instr & 0x00000001fc0LL) >> 6);
	  int qp = (int) (instr & 0x0000000003f);

	  if (qp == 0 && b2 == 0 && rN >= 32 && ret_reg == 0)
	    {
	      ret_reg = rN;
	      last_prologue_pc = next_pc;
	    }
	}
      else if ((it == I || it == M) 
          && ((instr & 0x1ee00000000LL) == 0x10800000000LL))
	{
	  /* adds rN = imm14, rM   (or mov rN, rM  when imm14 is 0) */
	  int imm = (int) ((((instr & 0x01000000000LL) ? -1 : 0) << 13) 
	                   | ((instr & 0x001f8000000LL) >> 20)
		           | ((instr & 0x000000fe000LL) >> 13));
	  int rM = (int) ((instr & 0x00007f00000LL) >> 20);
	  int rN = (int) ((instr & 0x00000001fc0LL) >> 6);
	  int qp = (int) (instr & 0x0000000003fLL);

	  if (qp == 0 && rN >= 32 && imm == 0 && rM == 12 && fp_reg == 0)
	    {
	      /* mov rN, r12 */
	      fp_reg = rN;
	      last_prologue_pc = next_pc;
	    }
	  else if (qp == 0 && rN == 12 && rM == 12)
	    {
	      /* adds r12, -mem_stack_frame_size, r12 */
	      mem_stack_frame_size -= imm;
	      last_prologue_pc = next_pc;
	    }
	  else if (qp == 0 && rN == 2 
	        && ((rM == fp_reg && fp_reg != 0) || rM == 12))
	    {
	      /* adds r2, spilloffset, rFramePointer 
	           or
		 adds r2, spilloffset, r12

	         Get ready for stf.spill or st8.spill instructions.
		 The address to start spilling at is loaded into r2. 
		 FIXME:  Why r2?  That's what gcc currently uses; it
		 could well be different for other compilers.  */

	      /* Hmm... whether or not this will work will depend on
	         where the pc is.  If it's still early in the prologue
		 this'll be wrong.  FIXME */
	      spill_addr  = (frame ? frame->frame : 0)
	                  + (rM == 12 ? 0 : mem_stack_frame_size) 
			  + imm;
	      spill_reg   = rN;
	      last_prologue_pc = next_pc;
	    }
	}
      else if (it == M 
            && (   ((instr & 0x1efc0000000LL) == 0x0eec0000000LL)
                || ((instr & 0x1ffc8000000LL) == 0x0cec0000000LL) ))
	{
	  /* stf.spill [rN] = fM, imm9
	     or
	     stf.spill [rN] = fM  */

	  int imm = imm9(instr);
	  int rN = (int) ((instr & 0x00007f00000LL) >> 20);
	  int fM = (int) ((instr & 0x000000fe000LL) >> 13);
	  int qp = (int) (instr & 0x0000000003fLL);
	  if (qp == 0 && rN == spill_reg && spill_addr != 0
	      && ((2 <= fM && fM <= 5) || (16 <= fM && fM <= 31)))
	    {
	      if (do_fsr_stuff)
	        frame->saved_regs[IA64_FR0_REGNUM + fM] = spill_addr;

              if ((instr & 0x1efc0000000) == 0x0eec0000000)
		spill_addr += imm;
	      else
		spill_addr = 0;		/* last one; must be done */
	      last_prologue_pc = next_pc;
	    }
	}
      else if ((it == M && ((instr & 0x1eff8000000LL) == 0x02110000000LL))
            || (it == I && ((instr & 0x1eff8000000LL) == 0x00050000000LL)) )
	{
	  /* mov.m rN = arM   
	       or 
	     mov.i rN = arM */

	  int arM = (int) ((instr & 0x00007f00000LL) >> 20);
	  int rN  = (int) ((instr & 0x00000001fc0LL) >> 6);
	  int qp  = (int) (instr & 0x0000000003fLL);
	  if (qp == 0 && isScratch (rN) && arM == 36 /* ar.unat */)
	    {
	      /* We have something like "mov.m r3 = ar.unat".  Remember the
		 r3 (or whatever) and watch for a store of this register... */
	      unat_save_reg = rN;
	      last_prologue_pc = next_pc;
	    }
	}
      else if (it == I && ((instr & 0x1eff8000000LL) == 0x00198000000LL))
	{
	  /* mov rN = pr */
	  int rN  = (int) ((instr & 0x00000001fc0LL) >> 6);
	  int qp  = (int) (instr & 0x0000000003fLL);
	  if (qp == 0 && isScratch (rN))
	    {
	      pr_save_reg = rN;
	      last_prologue_pc = next_pc;
	    }
	}
      else if (it == M 
            && (   ((instr & 0x1ffc8000000LL) == 0x08cc0000000LL)
	        || ((instr & 0x1efc0000000LL) == 0x0acc0000000LL)))
	{
	  /* st8 [rN] = rM 
	      or
	     st8 [rN] = rM, imm9 */
	  int rN = (int) ((instr & 0x00007f00000LL) >> 20);
	  int rM = (int) ((instr & 0x000000fe000LL) >> 13);
	  int qp = (int) (instr & 0x0000000003fLL);
	  if (qp == 0 && rN == spill_reg && spill_addr != 0
	      && (rM == unat_save_reg || rM == pr_save_reg))
	    {
	      /* We've found a spill of either the UNAT register or the PR
	         register.  (Well, not exactly; what we've actually found is
		 a spill of the register that UNAT or PR was moved to).
		 Record that fact and move on... */
	      if (rM == unat_save_reg)
		{
		  /* Track UNAT register */
		  if (do_fsr_stuff)
		    frame->saved_regs[IA64_UNAT_REGNUM] = spill_addr;
		  unat_save_reg = 0;
		}
	      else
	        {
		  /* Track PR register */
		  if (do_fsr_stuff)
		    frame->saved_regs[IA64_PR_REGNUM] = spill_addr;
		  pr_save_reg = 0;
		}
	      if ((instr & 0x1efc0000000LL) == 0x0acc0000000LL)
		/* st8 [rN] = rM, imm9 */
		spill_addr += imm9(instr);
	      else
		spill_addr = 0;		/* must be done spilling */
	      last_prologue_pc = next_pc;
	    }
	  else if (qp == 0 && 32 <= rM && rM < 40 && !instores[rM-32])
	    {
	      /* Allow up to one store of each input register. */
	      instores[rM-32] = 1;
	      last_prologue_pc = next_pc;
	    }
	}
      else if (it == M && ((instr & 0x1ff08000000LL) == 0x08c00000000LL))
	{
	  /* One of
	       st1 [rN] = rM
	       st2 [rN] = rM
	       st4 [rN] = rM
	       st8 [rN] = rM
	     Note that the st8 case is handled in the clause above.
	     
	     Advance over stores of input registers. One store per input
	     register is permitted. */
	  int rM = (int) ((instr & 0x000000fe000LL) >> 13);
	  int qp = (int) (instr & 0x0000000003fLL);
	  if (qp == 0 && 32 <= rM && rM < 40 && !instores[rM-32])
	    {
	      instores[rM-32] = 1;
	      last_prologue_pc = next_pc;
	    }
	}
      else if (it == M && ((instr & 0x1ff88000000LL) == 0x0cc80000000LL))
        {
	  /* Either
	       stfs [rN] = fM
	     or
	       stfd [rN] = fM

	     Advance over stores of floating point input registers.  Again
	     one store per register is permitted */
	  int fM = (int) ((instr & 0x000000fe000LL) >> 13);
	  int qp = (int) (instr & 0x0000000003fLL);
	  if (qp == 0 && 8 <= fM && fM < 16 && !infpstores[fM - 8])
	    {
	      infpstores[fM-8] = 1;
	      last_prologue_pc = next_pc;
	    }
	}
      else if (it == M
            && (   ((instr & 0x1ffc8000000LL) == 0x08ec0000000LL)
	        || ((instr & 0x1efc0000000LL) == 0x0aec0000000LL)))
	{
	  /* st8.spill [rN] = rM
	       or
	     st8.spill [rN] = rM, imm9 */
	  int rN = (int) ((instr & 0x00007f00000LL) >> 20);
	  int rM = (int) ((instr & 0x000000fe000LL) >> 13);
	  int qp = (int) (instr & 0x0000000003fLL);
	  if (qp == 0 && rN == spill_reg && 4 <= rM && rM <= 7)
	    {
	      /* We've found a spill of one of the preserved general purpose
	         regs.  Record the spill address and advance the spill
		 register if appropriate. */
	      if (do_fsr_stuff)
		frame->saved_regs[IA64_GR0_REGNUM + rM] = spill_addr;
	      if ((instr & 0x1efc0000000LL) == 0x0aec0000000LL)
	        /* st8.spill [rN] = rM, imm9 */
		spill_addr += imm9(instr);
	      else
		spill_addr = 0;		/* Done spilling */
	      last_prologue_pc = next_pc;
	    }
	}

      pc = next_pc;
    }

  if (do_fsr_stuff) {
    int i;
    CORE_ADDR addr;
    int sor, rrb_gr;
    
    /* Extract the size of the rotating portion of the stack
       frame and the register rename base from the current
       frame marker. */
    sor = ((frame->extra_info->cfm >> 14) & 0xf) * 8;
    rrb_gr = (frame->extra_info->cfm >> 18) & 0x7f;

    for (i = 0, addr = frame->extra_info->bsp;
	 i < frame->extra_info->sof;
	 i++, addr += 8)
      {
	if (IS_NaT_COLLECTION_ADDR (addr))
	  {
	    addr += 8;
	  }
	if (i < sor)
	  frame->saved_regs[IA64_GR32_REGNUM + ((i + (sor - rrb_gr)) % sor)] 
	    = addr;
	else
	  frame->saved_regs[IA64_GR32_REGNUM + i] = addr;

	if (i+32 == cfm_reg)
	  frame->saved_regs[IA64_CFM_REGNUM] = addr;
	if (i+32 == ret_reg)
	  frame->saved_regs[IA64_VRAP_REGNUM] = addr;
	if (i+32 == fp_reg)
	  frame->saved_regs[IA64_VFP_REGNUM] = addr;
      }
  }

  if (frame && frame->extra_info) {
    frame->extra_info->after_prologue = last_prologue_pc;
    frame->extra_info->mem_stack_frame_size = mem_stack_frame_size;
    frame->extra_info->fp_reg = fp_reg;
  }

  return last_prologue_pc;
}

CORE_ADDR
ia64_skip_prologue (CORE_ADDR pc)
{
  return examine_prologue (pc, pc+1024, 0);
}

void
ia64_frame_init_saved_regs (struct frame_info *frame)
{
  if (frame->saved_regs)
    return;

  if (frame->signal_handler_caller && SIGCONTEXT_REGISTER_ADDRESS)
    {
      int regno;

      frame_saved_regs_zalloc (frame);

      frame->saved_regs[IA64_VRAP_REGNUM] = 
	SIGCONTEXT_REGISTER_ADDRESS (frame->frame, IA64_IP_REGNUM);
      frame->saved_regs[IA64_CFM_REGNUM] = 
	SIGCONTEXT_REGISTER_ADDRESS (frame->frame, IA64_CFM_REGNUM);
      frame->saved_regs[IA64_PSR_REGNUM] = 
	SIGCONTEXT_REGISTER_ADDRESS (frame->frame, IA64_PSR_REGNUM);
#if 0
      frame->saved_regs[IA64_BSP_REGNUM] = 
	SIGCONTEXT_REGISTER_ADDRESS (frame->frame, IA64_BSP_REGNUM);
#endif
      frame->saved_regs[IA64_RNAT_REGNUM] = 
	SIGCONTEXT_REGISTER_ADDRESS (frame->frame, IA64_RNAT_REGNUM);
      frame->saved_regs[IA64_CCV_REGNUM] = 
	SIGCONTEXT_REGISTER_ADDRESS (frame->frame, IA64_CCV_REGNUM);
      frame->saved_regs[IA64_UNAT_REGNUM] = 
	SIGCONTEXT_REGISTER_ADDRESS (frame->frame, IA64_UNAT_REGNUM);
      frame->saved_regs[IA64_FPSR_REGNUM] = 
	SIGCONTEXT_REGISTER_ADDRESS (frame->frame, IA64_FPSR_REGNUM);
      frame->saved_regs[IA64_PFS_REGNUM] = 
	SIGCONTEXT_REGISTER_ADDRESS (frame->frame, IA64_PFS_REGNUM);
      frame->saved_regs[IA64_LC_REGNUM] = 
	SIGCONTEXT_REGISTER_ADDRESS (frame->frame, IA64_LC_REGNUM);
      for (regno = IA64_GR1_REGNUM; regno <= IA64_GR31_REGNUM; regno++)
	if (regno != sp_regnum)
	  frame->saved_regs[regno] =
	    SIGCONTEXT_REGISTER_ADDRESS (frame->frame, regno);
      for (regno = IA64_BR0_REGNUM; regno <= IA64_BR7_REGNUM; regno++)
	frame->saved_regs[regno] =
	  SIGCONTEXT_REGISTER_ADDRESS (frame->frame, regno);
      for (regno = IA64_FR2_REGNUM; regno <= IA64_BR7_REGNUM; regno++)
	frame->saved_regs[regno] =
	  SIGCONTEXT_REGISTER_ADDRESS (frame->frame, regno);
    }
  else
    {
      CORE_ADDR func_start;

      func_start = get_pc_function_start (frame->pc);
      examine_prologue (func_start, frame->pc, frame);
    }
}

void
ia64_get_saved_register (char *raw_buffer, 
                         int *optimized, 
			 CORE_ADDR *addrp,
			 struct frame_info *frame,
			 int regnum,
			 enum lval_type *lval)
{
  int is_dummy_frame;

  if (!target_has_registers)
    error ("No registers.");

  if (optimized != NULL)
    *optimized = 0;

  if (addrp != NULL)
    *addrp = 0;

  if (lval != NULL)
    *lval = not_lval;

  is_dummy_frame = PC_IN_CALL_DUMMY (frame->pc, frame->frame, frame->frame);

  if (regnum == SP_REGNUM && frame->next)
    {
      /* Handle SP values for all frames but the topmost. */
      store_address (raw_buffer, REGISTER_RAW_SIZE (regnum), frame->frame);
    }
  else if (regnum == IA64_BSP_REGNUM)
    {
      store_address (raw_buffer, REGISTER_RAW_SIZE (regnum), 
                     frame->extra_info->bsp);
    }
  else if (regnum == IA64_VFP_REGNUM)
    {
      /* If the function in question uses an automatic register (r32-r127)
         for the frame pointer, it'll be found by ia64_find_saved_register()
	 above.  If the function lacks one of these frame pointers, we can
	 still provide a value since we know the size of the frame */
      CORE_ADDR vfp = frame->frame + frame->extra_info->mem_stack_frame_size;
      store_address (raw_buffer, REGISTER_RAW_SIZE (IA64_VFP_REGNUM), vfp);
    }
  else if (IA64_PR0_REGNUM <= regnum && regnum <= IA64_PR63_REGNUM)
    {
      char *pr_raw_buffer = alloca (MAX_REGISTER_RAW_SIZE);
      int  pr_optim;
      enum lval_type pr_lval;
      CORE_ADDR pr_addr;
      int prN_val;
      ia64_get_saved_register (pr_raw_buffer, &pr_optim, &pr_addr,
                               frame, IA64_PR_REGNUM, &pr_lval);
      if (IA64_PR16_REGNUM <= regnum && regnum <= IA64_PR63_REGNUM)
	{
	  /* Fetch predicate register rename base from current frame
	     marker for this frame. */
	  int rrb_pr = (frame->extra_info->cfm >> 32) & 0x3f;

	  /* Adjust the register number to account for register rotation. */
	  regnum = IA64_PR16_REGNUM 
	         + ((regnum - IA64_PR16_REGNUM) + rrb_pr) % 48;
	}
      prN_val = extract_bit_field ((unsigned char *) pr_raw_buffer,
                                   regnum - IA64_PR0_REGNUM, 1);
      store_unsigned_integer (raw_buffer, REGISTER_RAW_SIZE (regnum), prN_val);
    }
  else if (IA64_NAT0_REGNUM <= regnum && regnum <= IA64_NAT31_REGNUM)
    {
      char *unat_raw_buffer = alloca (MAX_REGISTER_RAW_SIZE);
      int  unat_optim;
      enum lval_type unat_lval;
      CORE_ADDR unat_addr;
      int unatN_val;
      ia64_get_saved_register (unat_raw_buffer, &unat_optim, &unat_addr,
                               frame, IA64_UNAT_REGNUM, &unat_lval);
      unatN_val = extract_bit_field ((unsigned char *) unat_raw_buffer,
                                   regnum - IA64_NAT0_REGNUM, 1);
      store_unsigned_integer (raw_buffer, REGISTER_RAW_SIZE (regnum), 
                              unatN_val);
    }
  else if (IA64_NAT32_REGNUM <= regnum && regnum <= IA64_NAT127_REGNUM)
    {
      int natval = 0;
      /* Find address of general register corresponding to nat bit we're
         interested in. */
      CORE_ADDR gr_addr = 0;

      if (!is_dummy_frame)
	{
	  FRAME_INIT_SAVED_REGS (frame);
	  gr_addr = frame->saved_regs[ regnum - IA64_NAT0_REGNUM 
	                                      + IA64_GR0_REGNUM];
	}
      if (gr_addr)
	{
	  /* Compute address of nat collection bits */
	  CORE_ADDR nat_addr = gr_addr | 0x1f8;
	  CORE_ADDR bsp = read_register (IA64_BSP_REGNUM);
	  CORE_ADDR nat_collection;
	  int nat_bit;
	  /* If our nat collection address is bigger than bsp, we have to get
	     the nat collection from rnat.  Otherwise, we fetch the nat
	     collection from the computed address. */
	  if (nat_addr >= bsp)
	    nat_collection = read_register (IA64_RNAT_REGNUM);
	  else
	    nat_collection = read_memory_integer (nat_addr, 8);
	  nat_bit = (gr_addr >> 3) & 0x3f;
	  natval = (nat_collection >> nat_bit) & 1;
	}
      store_unsigned_integer (raw_buffer, REGISTER_RAW_SIZE (regnum), natval);
    }
  else if (regnum == IA64_IP_REGNUM)
    {
      CORE_ADDR pc;
      if (frame->next)
        {
	  /* FIXME: Set *addrp, *lval when possible. */
	  pc = ia64_frame_saved_pc (frame->next);
        }
      else
        {
	  pc = read_pc ();
	}
      store_address (raw_buffer, REGISTER_RAW_SIZE (IA64_IP_REGNUM), pc);
    }
  else if (IA64_GR32_REGNUM <= regnum && regnum <= IA64_GR127_REGNUM)
    {
      CORE_ADDR addr = 0;
      if (!is_dummy_frame)
	{
	  FRAME_INIT_SAVED_REGS (frame);
	  addr = frame->saved_regs[regnum];
	}

      if (addr != 0)
	{
	  if (lval != NULL)
	    *lval = lval_memory;
	  if (addrp != NULL)
	    *addrp = addr;
	  read_memory (addr, raw_buffer, REGISTER_RAW_SIZE (regnum));
	}
      else
        {
	  /* r32 - r127 must be fetchable via memory.  If they aren't,
	     then the register is unavailable */
	  memset (raw_buffer, 0, REGISTER_RAW_SIZE (regnum));
        }
    }
  else
    {
      if (IA64_FR32_REGNUM <= regnum && regnum <= IA64_FR127_REGNUM)
	{
	  /* Fetch floating point register rename base from current
	     frame marker for this frame. */
	  int rrb_fr = (frame->extra_info->cfm >> 25) & 0x7f;

	  /* Adjust the floating point register number to account for
	     register rotation. */
	  regnum = IA64_FR32_REGNUM
	         + ((regnum - IA64_FR32_REGNUM) + rrb_fr) % 96;
	}

      generic_get_saved_register (raw_buffer, optimized, addrp, frame,
                                  regnum, lval);
    }
}

/* Should we use EXTRACT_STRUCT_VALUE_ADDRESS instead of
   EXTRACT_RETURN_VALUE?  GCC_P is true if compiled with gcc
   and TYPE is the type (which is known to be struct, union or array).  */
int
ia64_use_struct_convention (int gcc_p, struct type *type)
{
  struct type *float_elt_type;

  /* HFAs are structures (or arrays) consisting entirely of floating
     point values of the same length.  Up to 8 of these are returned
     in registers.  Don't use the struct convention when this is the
     case. */
  float_elt_type = is_float_or_hfa_type (type);
  if (float_elt_type != NULL
      && TYPE_LENGTH (type) / TYPE_LENGTH (float_elt_type) <= 8)
    return 0;

  /* Other structs of length 32 or less are returned in r8-r11.
     Don't use the struct convention for those either. */
  return TYPE_LENGTH (type) > 32;
}

void
ia64_extract_return_value (struct type *type, char *regbuf, char *valbuf)
{
  struct type *float_elt_type;

  float_elt_type = is_float_or_hfa_type (type);
  if (float_elt_type != NULL)
    {
      int offset = 0;
      int regnum = IA64_FR8_REGNUM;
      int n = TYPE_LENGTH (type) / TYPE_LENGTH (float_elt_type);

      while (n-- > 0)
	{
	  ia64_register_convert_to_virtual (regnum, float_elt_type,
	    &regbuf[REGISTER_BYTE (regnum)], valbuf + offset);
	  offset += TYPE_LENGTH (float_elt_type);
	  regnum++;
	}
    }
  else
    memcpy (valbuf, &regbuf[REGISTER_BYTE (IA64_GR8_REGNUM)],
	    TYPE_LENGTH (type));
}

/* FIXME: Turn this into a stack of some sort.  Unfortunately, something
   like this is necessary though since the IA-64 calling conventions specify
   that r8 is not preserved. */
static CORE_ADDR struct_return_address;

CORE_ADDR
ia64_extract_struct_value_address (char *regbuf)
{
  /* FIXME: See above. */
  return struct_return_address;
}

void
ia64_store_struct_return (CORE_ADDR addr, CORE_ADDR sp)
{
  /* FIXME: See above. */
  /* Note that most of the work was done in ia64_push_arguments() */
  struct_return_address = addr;
}

int
ia64_frameless_function_invocation (struct frame_info *frame)
{
  FRAME_INIT_SAVED_REGS (frame);
  return (frame->extra_info->mem_stack_frame_size == 0);
}

CORE_ADDR
ia64_saved_pc_after_call (struct frame_info *frame)
{
  return read_register (IA64_BR0_REGNUM);
}

CORE_ADDR
ia64_frame_args_address (struct frame_info *frame)
{
  /* frame->frame points at the SP for this frame; But we want the start
     of the frame, not the end.  Calling frame chain will get his for us. */
  return ia64_frame_chain (frame);
}

CORE_ADDR
ia64_frame_locals_address (struct frame_info *frame)
{
  /* frame->frame points at the SP for this frame; But we want the start
     of the frame, not the end.  Calling frame chain will get his for us. */
  return ia64_frame_chain (frame);
}

void
ia64_init_extra_frame_info (int fromleaf, struct frame_info *frame)
{
  CORE_ADDR bsp, cfm;
  int next_frame_is_call_dummy = ((frame->next != NULL)
    && PC_IN_CALL_DUMMY (frame->next->pc, frame->next->frame,
                                          frame->next->frame));

  frame->extra_info = (struct frame_extra_info *)
    frame_obstack_alloc (sizeof (struct frame_extra_info));

  if (frame->next == 0)
    {
      bsp = read_register (IA64_BSP_REGNUM);
      cfm = read_register (IA64_CFM_REGNUM);

    }
  else if (frame->next->signal_handler_caller)
    {
      bsp = read_sigcontext_register (frame->next, IA64_BSP_REGNUM);
      cfm = read_sigcontext_register (frame->next, IA64_CFM_REGNUM);
    }
  else if (next_frame_is_call_dummy)
    {
      bsp = generic_read_register_dummy (frame->next->pc, frame->next->frame,
                                         IA64_BSP_REGNUM);
      cfm = generic_read_register_dummy (frame->next->pc, frame->next->frame,
                                         IA64_CFM_REGNUM);
    }
  else
    {
      struct frame_info *frn = frame->next;

      FRAME_INIT_SAVED_REGS (frn);

      if (frn->saved_regs[IA64_CFM_REGNUM] != 0)
	cfm = read_memory_integer (frn->saved_regs[IA64_CFM_REGNUM], 8);
      else if (frn->next && frn->next->signal_handler_caller)
	cfm = read_sigcontext_register (frn->next, IA64_PFS_REGNUM);
      else if (frn->next
               && PC_IN_CALL_DUMMY (frn->next->pc, frn->next->frame,
	                                           frn->next->frame))
	cfm = generic_read_register_dummy (frn->next->pc, frn->next->frame,
	                                   IA64_PFS_REGNUM);
      else
	cfm = read_register (IA64_PFS_REGNUM);

      bsp = frn->extra_info->bsp;
    }
  frame->extra_info->cfm = cfm;
  frame->extra_info->sof = cfm & 0x7f;
  frame->extra_info->sol = (cfm >> 7) & 0x7f;
  if (frame->next == 0 
      || frame->next->signal_handler_caller 
      || next_frame_is_call_dummy)
    frame->extra_info->bsp = rse_address_add (bsp, -frame->extra_info->sof);
  else
    frame->extra_info->bsp = rse_address_add (bsp, -frame->extra_info->sol);

  frame->extra_info->after_prologue = 0;
  frame->extra_info->mem_stack_frame_size = -1;		/* Not yet determined */
  frame->extra_info->fp_reg = 0;
}

static int
is_float_or_hfa_type_recurse (struct type *t, struct type **etp)
{
  switch (TYPE_CODE (t))
    {
    case TYPE_CODE_FLT:
      if (*etp)
	return TYPE_LENGTH (*etp) == TYPE_LENGTH (t);
      else
	{
	  *etp = t;
	  return 1;
	}
      break;
    case TYPE_CODE_ARRAY:
      return
	is_float_or_hfa_type_recurse (check_typedef (TYPE_TARGET_TYPE (t)),
				      etp);
      break;
    case TYPE_CODE_STRUCT:
      {
	int i;

	for (i = 0; i < TYPE_NFIELDS (t); i++)
	  if (!is_float_or_hfa_type_recurse
	      (check_typedef (TYPE_FIELD_TYPE (t, i)), etp))
	    return 0;
	return 1;
      }
      break;
    default:
      return 0;
      break;
    }
}

/* Determine if the given type is one of the floating point types or
   and HFA (which is a struct, array, or combination thereof whose
   bottom-most elements are all of the same floating point type.) */

static struct type *
is_float_or_hfa_type (struct type *t)
{
  struct type *et = 0;

  return is_float_or_hfa_type_recurse (t, &et) ? et : 0;
}


/* Return 1 if the alignment of T is such that the next even slot
   should be used.  Return 0, if the next available slot should
   be used.  (See section 8.5.1 of the IA-64 Software Conventions
   and Runtime manual.)  */

static int
slot_alignment_is_next_even (struct type *t)
{
  switch (TYPE_CODE (t))
    {
    case TYPE_CODE_INT:
    case TYPE_CODE_FLT:
      if (TYPE_LENGTH (t) > 8)
	return 1;
      else
	return 0;
    case TYPE_CODE_ARRAY:
      return
	slot_alignment_is_next_even (check_typedef (TYPE_TARGET_TYPE (t)));
    case TYPE_CODE_STRUCT:
      {
	int i;

	for (i = 0; i < TYPE_NFIELDS (t); i++)
	  if (slot_alignment_is_next_even
	      (check_typedef (TYPE_FIELD_TYPE (t, i))))
	    return 1;
	return 0;
      }
    default:
      return 0;
    }
}

/* Attempt to find (and return) the global pointer for the given
   function.

   This is a rather nasty bit of code searchs for the .dynamic section
   in the objfile corresponding to the pc of the function we're trying
   to call.  Once it finds the addresses at which the .dynamic section
   lives in the child process, it scans the Elf64_Dyn entries for a
   DT_PLTGOT tag.  If it finds one of these, the corresponding
   d_un.d_ptr value is the global pointer.  */

static CORE_ADDR
generic_elf_find_global_pointer (CORE_ADDR faddr)
{
  struct obj_section *faddr_sect;
     
  faddr_sect = find_pc_section (faddr);
  if (faddr_sect != NULL)
    {
      struct obj_section *osect;

      ALL_OBJFILE_OSECTIONS (faddr_sect->objfile, osect)
	{
	  if (strcmp (osect->the_bfd_section->name, ".dynamic") == 0)
	    break;
	}

      if (osect < faddr_sect->objfile->sections_end)
	{
	  CORE_ADDR addr;

	  addr = osect->addr;
	  while (addr < osect->endaddr)
	    {
	      int status;
	      LONGEST tag;
	      char buf[8];

	      status = target_read_memory (addr, buf, sizeof (buf));
	      if (status != 0)
		break;
	      tag = extract_signed_integer (buf, sizeof (buf));

	      if (tag == DT_PLTGOT)
		{
		  CORE_ADDR global_pointer;

		  status = target_read_memory (addr + 8, buf, sizeof (buf));
		  if (status != 0)
		    break;
		  global_pointer = extract_address (buf, sizeof (buf));

		  /* The payoff... */
		  return global_pointer;
		}

	      if (tag == DT_NULL)
		break;

	      addr += 16;
	    }
	}
    }
  return 0;
}

/* Given a function's address, attempt to find (and return) the
   corresponding (canonical) function descriptor.  Return 0 if
   not found. */
static CORE_ADDR
find_extant_func_descr (CORE_ADDR faddr)
{
  struct obj_section *faddr_sect;

  /* Return early if faddr is already a function descriptor */
  faddr_sect = find_pc_section (faddr);
  if (faddr_sect && strcmp (faddr_sect->the_bfd_section->name, ".opd") == 0)
    return faddr;

  if (faddr_sect != NULL)
    {
      struct obj_section *osect;
      ALL_OBJFILE_OSECTIONS (faddr_sect->objfile, osect)
	{
	  if (strcmp (osect->the_bfd_section->name, ".opd") == 0)
	    break;
	}

      if (osect < faddr_sect->objfile->sections_end)
	{
	  CORE_ADDR addr;

	  addr = osect->addr;
	  while (addr < osect->endaddr)
	    {
	      int status;
	      LONGEST faddr2;
	      char buf[8];

	      status = target_read_memory (addr, buf, sizeof (buf));
	      if (status != 0)
		break;
	      faddr2 = extract_signed_integer (buf, sizeof (buf));

	      if (faddr == faddr2)
		return addr;

	      addr += 16;
	    }
	}
    }
  return 0;
}

/* Attempt to find a function descriptor corresponding to the
   given address.  If none is found, construct one on the
   stack using the address at fdaptr */

static CORE_ADDR
find_func_descr (CORE_ADDR faddr, CORE_ADDR *fdaptr)
{
  CORE_ADDR fdesc;

  fdesc = find_extant_func_descr (faddr);

  if (fdesc == 0)
    {
      CORE_ADDR global_pointer;
      char buf[16];

      fdesc = *fdaptr;
      *fdaptr += 16;

      global_pointer = FIND_GLOBAL_POINTER (faddr);

      if (global_pointer == 0)
	global_pointer = read_register (IA64_GR1_REGNUM);

      store_address (buf, 8, faddr);
      store_address (buf + 8, 8, global_pointer);

      write_memory (fdesc, buf, 16);
    }

  return fdesc; 
}

CORE_ADDR
ia64_push_arguments (int nargs, struct value **args, CORE_ADDR sp,
		    int struct_return, CORE_ADDR struct_addr)
{
  int argno;
  struct value *arg;
  struct type *type;
  int len, argoffset;
  int nslots, rseslots, memslots, slotnum, nfuncargs;
  int floatreg;
  CORE_ADDR bsp, cfm, pfs, new_bsp, funcdescaddr;

  nslots = 0;
  nfuncargs = 0;
  /* Count the number of slots needed for the arguments */
  for (argno = 0; argno < nargs; argno++)
    {
      arg = args[argno];
      type = check_typedef (VALUE_TYPE (arg));
      len = TYPE_LENGTH (type);

      if ((nslots & 1) && slot_alignment_is_next_even (type))
	nslots++;

      if (TYPE_CODE (type) == TYPE_CODE_FUNC)
	nfuncargs++;

      nslots += (len + 7) / 8;
    }

  /* Divvy up the slots between the RSE and the memory stack */
  rseslots = (nslots > 8) ? 8 : nslots;
  memslots = nslots - rseslots;

  /* Allocate a new RSE frame */
  cfm = read_register (IA64_CFM_REGNUM);

  bsp = read_register (IA64_BSP_REGNUM);
  bsp = rse_address_add (bsp, cfm & 0x7f);
  new_bsp = rse_address_add (bsp, rseslots);
  write_register (IA64_BSP_REGNUM, new_bsp);

  pfs = read_register (IA64_PFS_REGNUM);
  pfs &= 0xc000000000000000LL;
  pfs |= (cfm & 0xffffffffffffLL);
  write_register (IA64_PFS_REGNUM, pfs);

  cfm &= 0xc000000000000000LL;
  cfm |= rseslots;
  write_register (IA64_CFM_REGNUM, cfm);
  
  /* We will attempt to find function descriptors in the .opd segment,
     but if we can't we'll construct them ourselves.  That being the
     case, we'll need to reserve space on the stack for them. */
  funcdescaddr = sp - nfuncargs * 16;
  funcdescaddr &= ~0xfLL;

  /* Adjust the stack pointer to it's new value.  The calling conventions
     require us to have 16 bytes of scratch, plus whatever space is
     necessary for the memory slots and our function descriptors */
  sp = sp - 16 - (memslots + nfuncargs) * 8;
  sp &= ~0xfLL;				/* Maintain 16 byte alignment */

  /* Place the arguments where they belong.  The arguments will be
     either placed in the RSE backing store or on the memory stack.
     In addition, floating point arguments or HFAs are placed in
     floating point registers. */
  slotnum = 0;
  floatreg = IA64_FR8_REGNUM;
  for (argno = 0; argno < nargs; argno++)
    {
      struct type *float_elt_type;

      arg = args[argno];
      type = check_typedef (VALUE_TYPE (arg));
      len = TYPE_LENGTH (type);

      /* Special handling for function parameters */
      if (len == 8 
          && TYPE_CODE (type) == TYPE_CODE_PTR 
	  && TYPE_CODE (TYPE_TARGET_TYPE (type)) == TYPE_CODE_FUNC)
	{
	  char val_buf[8];

	  store_address (val_buf, 8,
	    find_func_descr (extract_address (VALUE_CONTENTS (arg), 8),
	                     &funcdescaddr));
	  if (slotnum < rseslots)
	    write_memory (rse_address_add (bsp, slotnum), val_buf, 8);
	  else
	    write_memory (sp + 16 + 8 * (slotnum - rseslots), val_buf, 8);
	  slotnum++;
	  continue;
	}

      /* Normal slots */

      /* Skip odd slot if necessary...  */
      if ((slotnum & 1) && slot_alignment_is_next_even (type))
	slotnum++;

      argoffset = 0;
      while (len > 0)
	{
	  char val_buf[8];

	  memset (val_buf, 0, 8);
	  memcpy (val_buf, VALUE_CONTENTS (arg) + argoffset, (len > 8) ? 8 : len);

	  if (slotnum < rseslots)
	    write_memory (rse_address_add (bsp, slotnum), val_buf, 8);
	  else
	    write_memory (sp + 16 + 8 * (slotnum - rseslots), val_buf, 8);

	  argoffset += 8;
	  len -= 8;
	  slotnum++;
	}

      /* Handle floating point types (including HFAs) */
      float_elt_type = is_float_or_hfa_type (type);
      if (float_elt_type != NULL)
	{
	  argoffset = 0;
	  len = TYPE_LENGTH (type);
	  while (len > 0 && floatreg < IA64_FR16_REGNUM)
	    {
	      ia64_register_convert_to_raw (
		float_elt_type,
		floatreg,
	        VALUE_CONTENTS (arg) + argoffset,
		&registers[REGISTER_BYTE (floatreg)]);
	      floatreg++;
	      argoffset += TYPE_LENGTH (float_elt_type);
	      len -= TYPE_LENGTH (float_elt_type);
	    }
	}
    }

  /* Store the struct return value in r8 if necessary. */
  if (struct_return)
    {
      store_address (&registers[REGISTER_BYTE (IA64_GR8_REGNUM)],
                     REGISTER_RAW_SIZE (IA64_GR8_REGNUM),
		     struct_addr);
    }

  /* Sync gdb's idea of what the registers are with the target. */
  target_store_registers (-1);

  /* FIXME: This doesn't belong here!  Instead, SAVE_DUMMY_FRAME_TOS needs
     to be defined to call generic_save_dummy_frame_tos().  But at the
     time of this writing, SAVE_DUMMY_FRAME_TOS wasn't gdbarch'd, so
     I chose to put this call here instead of using the old mechanisms. 
     Once SAVE_DUMMY_FRAME_TOS is gdbarch'd, all we need to do is add the
     line

	set_gdbarch_save_dummy_frame_tos (gdbarch, generic_save_dummy_frame_tos);

     to ia64_gdbarch_init() and remove the line below. */
  generic_save_dummy_frame_tos (sp);

  return sp;
}

CORE_ADDR
ia64_push_return_address (CORE_ADDR pc, CORE_ADDR sp)
{
  CORE_ADDR global_pointer = FIND_GLOBAL_POINTER (pc);

  if (global_pointer != 0)
    write_register (IA64_GR1_REGNUM, global_pointer);

  write_register (IA64_BR0_REGNUM, CALL_DUMMY_ADDRESS ());
  return sp;
}

void
ia64_store_return_value (struct type *type, char *valbuf)
{
  if (TYPE_CODE (type) == TYPE_CODE_FLT)
    {
      ia64_register_convert_to_raw (type, IA64_FR8_REGNUM, valbuf,
				  &registers[REGISTER_BYTE (IA64_FR8_REGNUM)]);
      target_store_registers (IA64_FR8_REGNUM);
    }
  else
    write_register_bytes (REGISTER_BYTE (IA64_GR8_REGNUM),
			  valbuf, TYPE_LENGTH (type));
}

void
ia64_pop_frame (void)
{
  generic_pop_current_frame (ia64_pop_frame_regular);
}

static void
ia64_pop_frame_regular (struct frame_info *frame)
{
  int regno;
  CORE_ADDR bsp, cfm, pfs;

  FRAME_INIT_SAVED_REGS (frame);

  for (regno = 0; regno < ia64_num_regs; regno++)
    {
      if (frame->saved_regs[regno]
	  && (!(IA64_GR32_REGNUM <= regno && regno <= IA64_GR127_REGNUM))
	  && regno != pc_regnum
	  && regno != sp_regnum
	  && regno != IA64_PFS_REGNUM
	  && regno != IA64_CFM_REGNUM
	  && regno != IA64_BSP_REGNUM
	  && regno != IA64_BSPSTORE_REGNUM)
	{
	  write_register (regno, 
			  read_memory_integer (frame->saved_regs[regno],
					       REGISTER_RAW_SIZE (regno)));
	}
    }

  write_register (sp_regnum, FRAME_CHAIN (frame));
  write_pc (FRAME_SAVED_PC (frame));

  cfm = read_register (IA64_CFM_REGNUM);

  if (frame->saved_regs[IA64_PFS_REGNUM])
    {
      pfs = read_memory_integer (frame->saved_regs[IA64_PFS_REGNUM],
				 REGISTER_RAW_SIZE (IA64_PFS_REGNUM));
    }
  else
    pfs = read_register (IA64_PFS_REGNUM);

  /* Compute the new bsp by *adding* the difference between the
     size of the frame and the size of the locals (both wrt the
     frame that we're going back to).  This seems kind of strange,
     especially since it seems like we ought to be subtracting the
     size of the locals... and we should; but the Linux kernel
     wants bsp to be set at the end of all used registers.  It's
     likely that this code will need to be revised to accomodate
     other operating systems. */
  bsp = rse_address_add (frame->extra_info->bsp,
                         (pfs & 0x7f) - ((pfs >> 7) & 0x7f));
  write_register (IA64_BSP_REGNUM, bsp);

  /* FIXME: What becomes of the epilog count in the PFS? */
  cfm = (cfm & ~0xffffffffffffLL) | (pfs & 0xffffffffffffLL);
  write_register (IA64_CFM_REGNUM, cfm);

  flush_cached_frames ();
}

static void
ia64_remote_translate_xfer_address (CORE_ADDR memaddr, int nr_bytes,
				    CORE_ADDR *targ_addr, int *targ_len)
{
  *targ_addr = memaddr;
  *targ_len  = nr_bytes;
}

static void
process_note_abi_tag_sections (bfd *abfd, asection *sect, void *obj)
{
  int *os_ident_ptr = obj;
  const char *name;
  unsigned int sectsize;

  name = bfd_get_section_name (abfd, sect);
  sectsize = bfd_section_size (abfd, sect);
  if (strcmp (name, ".note.ABI-tag") == 0 && sectsize > 0)
    {
      unsigned int name_length, data_length, note_type;
      char *note = alloca (sectsize);

      bfd_get_section_contents (abfd, sect, note,
                                (file_ptr) 0, (bfd_size_type) sectsize);

      name_length = bfd_h_get_32 (abfd, note);
      data_length = bfd_h_get_32 (abfd, note + 4);
      note_type   = bfd_h_get_32 (abfd, note + 8);

      if (name_length == 4 && data_length == 16 && note_type == 1
          && strcmp (note + 12, "GNU") == 0)
	{
	  int os_number = bfd_h_get_32 (abfd, note + 16);

	  /* The case numbers are from abi-tags in glibc */
	  switch (os_number)
	    {
	    case 0 :
	      *os_ident_ptr = ELFOSABI_LINUX;
	      break;
	    case 1 :
	      *os_ident_ptr = ELFOSABI_HURD;
	      break;
	    case 2 :
	      *os_ident_ptr = ELFOSABI_SOLARIS;
	      break;
	    default :
	      internal_error (__FILE__, __LINE__,
			      "process_note_abi_sections: unknown OS number %d", os_number);
	      break;
	    }
	}
    }
}

static struct gdbarch *
ia64_gdbarch_init (struct gdbarch_info info, struct gdbarch_list *arches)
{
  struct gdbarch *gdbarch;
  struct gdbarch_tdep *tdep;
  int os_ident;

  if (info.abfd != NULL
      && bfd_get_flavour (info.abfd) == bfd_target_elf_flavour)
    {
      os_ident = elf_elfheader (info.abfd)->e_ident[EI_OSABI];

      /* If os_ident is 0, it is not necessarily the case that we're
         on a SYSV system.  (ELFOSABI_NONE is defined to be 0.)
         GNU/Linux uses a note section to record OS/ABI info, but
         leaves e_ident[EI_OSABI] zero.  So we have to check for note
         sections too. */
      if (os_ident == 0)
	{
	  bfd_map_over_sections (info.abfd,
	                         process_note_abi_tag_sections,
				 &os_ident);
	}
    }
  else
    os_ident = -1;

  for (arches = gdbarch_list_lookup_by_info (arches, &info);
       arches != NULL;
       arches = gdbarch_list_lookup_by_info (arches->next, &info))
    {
      tdep = gdbarch_tdep (arches->gdbarch);
      if (tdep &&tdep->os_ident == os_ident)
	return arches->gdbarch;
    }

  tdep = xmalloc (sizeof (struct gdbarch_tdep));
  gdbarch = gdbarch_alloc (&info, tdep);
  tdep->os_ident = os_ident;


  /* Set the method of obtaining the sigcontext addresses at which
     registers are saved.  The method of checking to see if
     native_find_global_pointer is nonzero to indicate that we're
     on AIX is kind of hokey, but I can't think of a better way
     to do it.  */
  if (os_ident == ELFOSABI_LINUX)
    tdep->sigcontext_register_address = ia64_linux_sigcontext_register_address;
  else if (native_find_global_pointer != 0)
    tdep->sigcontext_register_address = ia64_aix_sigcontext_register_address;
  else
    tdep->sigcontext_register_address = 0;

  /* We know that GNU/Linux won't have to resort to the
     native_find_global_pointer hackery.  But that's the only one we
     know about so far, so if native_find_global_pointer is set to
     something non-zero, then use it.  Otherwise fall back to using
     generic_elf_find_global_pointer.  This arrangement should (in
     theory) allow us to cross debug GNU/Linux binaries from an AIX
     machine.  */
  if (os_ident == ELFOSABI_LINUX)
    tdep->find_global_pointer = generic_elf_find_global_pointer;
  else if (native_find_global_pointer != 0)
    tdep->find_global_pointer = native_find_global_pointer;
  else
    tdep->find_global_pointer = generic_elf_find_global_pointer;

  set_gdbarch_short_bit (gdbarch, 16);
  set_gdbarch_int_bit (gdbarch, 32);
  set_gdbarch_long_bit (gdbarch, 64);
  set_gdbarch_long_long_bit (gdbarch, 64);
  set_gdbarch_float_bit (gdbarch, 32);
  set_gdbarch_double_bit (gdbarch, 64);
  set_gdbarch_long_double_bit (gdbarch, 64);
  set_gdbarch_ptr_bit (gdbarch, 64);

  set_gdbarch_num_regs (gdbarch, ia64_num_regs);
  set_gdbarch_sp_regnum (gdbarch, sp_regnum);
  set_gdbarch_fp_regnum (gdbarch, fp_regnum);
  set_gdbarch_pc_regnum (gdbarch, pc_regnum);
  set_gdbarch_fp0_regnum (gdbarch, IA64_FR0_REGNUM);

  set_gdbarch_register_name (gdbarch, ia64_register_name);
  set_gdbarch_register_size (gdbarch, 8);
  set_gdbarch_register_bytes (gdbarch, ia64_num_regs * 8 + 128*8);
  set_gdbarch_register_byte (gdbarch, ia64_register_byte);
  set_gdbarch_register_raw_size (gdbarch, ia64_register_raw_size);
  set_gdbarch_max_register_raw_size (gdbarch, 16);
  set_gdbarch_register_virtual_size (gdbarch, ia64_register_virtual_size);
  set_gdbarch_max_register_virtual_size (gdbarch, 16);
  set_gdbarch_register_virtual_type (gdbarch, ia64_register_virtual_type);

  set_gdbarch_skip_prologue (gdbarch, ia64_skip_prologue);

  set_gdbarch_frame_num_args (gdbarch, frame_num_args_unknown);
  set_gdbarch_frameless_function_invocation (gdbarch, ia64_frameless_function_invocation);

  set_gdbarch_saved_pc_after_call (gdbarch, ia64_saved_pc_after_call);

  set_gdbarch_frame_chain (gdbarch, ia64_frame_chain);
  set_gdbarch_frame_chain_valid (gdbarch, generic_func_frame_chain_valid);
  set_gdbarch_frame_saved_pc (gdbarch, ia64_frame_saved_pc);

  set_gdbarch_frame_init_saved_regs (gdbarch, ia64_frame_init_saved_regs);
  set_gdbarch_get_saved_register (gdbarch, ia64_get_saved_register);

  set_gdbarch_register_convertible (gdbarch, ia64_register_convertible);
  set_gdbarch_register_convert_to_virtual (gdbarch, ia64_register_convert_to_virtual);
  set_gdbarch_register_convert_to_raw (gdbarch, ia64_register_convert_to_raw);

  set_gdbarch_use_struct_convention (gdbarch, ia64_use_struct_convention);
  set_gdbarch_extract_return_value (gdbarch, ia64_extract_return_value);

  set_gdbarch_store_struct_return (gdbarch, ia64_store_struct_return);
  set_gdbarch_store_return_value (gdbarch, ia64_store_return_value);
  set_gdbarch_extract_struct_value_address (gdbarch, ia64_extract_struct_value_address);

  set_gdbarch_memory_insert_breakpoint (gdbarch, ia64_memory_insert_breakpoint);
  set_gdbarch_memory_remove_breakpoint (gdbarch, ia64_memory_remove_breakpoint);
  set_gdbarch_breakpoint_from_pc (gdbarch, ia64_breakpoint_from_pc);
  set_gdbarch_read_pc (gdbarch, ia64_read_pc);
  set_gdbarch_write_pc (gdbarch, ia64_write_pc);

  /* Settings for calling functions in the inferior.  */
  set_gdbarch_use_generic_dummy_frames (gdbarch, 1);
  set_gdbarch_call_dummy_length (gdbarch, 0);
  set_gdbarch_push_arguments (gdbarch, ia64_push_arguments);
  set_gdbarch_push_return_address (gdbarch, ia64_push_return_address);
  set_gdbarch_pop_frame (gdbarch, ia64_pop_frame);

  set_gdbarch_call_dummy_p (gdbarch, 1);
  set_gdbarch_call_dummy_words (gdbarch, ia64_call_dummy_words);
  set_gdbarch_sizeof_call_dummy_words (gdbarch, sizeof (ia64_call_dummy_words));
  set_gdbarch_call_dummy_breakpoint_offset_p (gdbarch, 1);
  set_gdbarch_init_extra_frame_info (gdbarch, ia64_init_extra_frame_info);
  set_gdbarch_frame_args_address (gdbarch, ia64_frame_args_address);
  set_gdbarch_frame_locals_address (gdbarch, ia64_frame_locals_address);

  /* We won't necessarily have a frame pointer and even if we do,
     it winds up being extraordinarly messy when attempting to find
     the frame chain.  So for the purposes of creating frames (which
     is all read_fp() is used for), simply use the stack pointer value
     instead.  */
  set_gdbarch_read_fp (gdbarch, generic_target_read_sp);
  set_gdbarch_write_fp (gdbarch, generic_target_write_sp);

  /* Settings that should be unnecessary.  */
  set_gdbarch_inner_than (gdbarch, core_addr_lessthan);

  set_gdbarch_read_sp (gdbarch, generic_target_read_sp);
  set_gdbarch_write_sp (gdbarch, generic_target_write_sp);

  set_gdbarch_call_dummy_location (gdbarch, AT_ENTRY_POINT);
  set_gdbarch_call_dummy_address (gdbarch, entry_point_address);
  set_gdbarch_call_dummy_breakpoint_offset (gdbarch, 0);
  set_gdbarch_call_dummy_start_offset (gdbarch, 0);
  set_gdbarch_pc_in_call_dummy (gdbarch, generic_pc_in_call_dummy);
  set_gdbarch_call_dummy_stack_adjust_p (gdbarch, 0);
  set_gdbarch_push_dummy_frame (gdbarch, generic_push_dummy_frame);
  set_gdbarch_fix_call_dummy (gdbarch, generic_fix_call_dummy);

  set_gdbarch_decr_pc_after_break (gdbarch, 0);
  set_gdbarch_function_start_offset (gdbarch, 0);

  set_gdbarch_remote_translate_xfer_address (
    gdbarch, ia64_remote_translate_xfer_address);

  return gdbarch;
}

void
_initialize_ia64_tdep (void)
{
  register_gdbarch_init (bfd_arch_ia64, ia64_gdbarch_init);

  tm_print_insn = print_insn_ia64;
  tm_print_insn_info.bytes_per_line = SLOT_MULTIPLIER;
}
