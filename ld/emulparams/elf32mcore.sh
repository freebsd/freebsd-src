SCRIPT_NAME=elf
OUTPUT_FORMAT="elf32-mcore-little"
BIG_OUTPUT_FORMAT="elf32-mcore-big"
LITTLE_OUTPUT_FORMAT="elf32-mcore-little"
PAGE_SIZE=0x1000
TARGET_PAGE_SIZE=0x400
MAXPAGESIZE="CONSTANT (MAXPAGESIZE)"
TEXT_START_ADDR=0
NONPAGED_TEXT_START_ADDR=0
ARCH=mcore
EMBEDDED=yes

# There is a problem with the NOP value - it must work for both
# big endian and little endian systems.  Unfortunately there is
# no symmetrical mcore opcode that functions as a noop.  The
# chosen solution is to use "tst r0, r14".  This is a symetrical
# value, and apart from the corruption of the C bit, it has no other
# side effects.  Since the carry bit is never tested without being
# explicitly set first, and since the NOP code is only used as a
# fill value between independantly viable peices of code, it should
# not matter.
NOP=0x0e0e0e0e

OTHER_BSS_SYMBOLS="__bss_start__ = . ;"
OTHER_BSS_END_SYMBOLS="__bss_end__ = . ;"

# This sets the stack to the top of the simulator memory (2^19 bytes).
STACK_ADDR=0x80000

TEMPLATE_NAME=elf32
GENERATE_SHLIB_SCRIPT=yes

# This code gets inserted into the generic elf32.sc linker script
# and allows us to define our own command line switches.
PARSE_AND_LIST_PROLOGUE='
#define OPTION_BASE_FILE		300
'

PARSE_AND_LIST_LONGOPTS='
  {"base-file", required_argument, NULL, OPTION_BASE_FILE},
'

PARSE_AND_LIST_OPTIONS='
  fprintf (file, _("  --base_file <basefile>\n"));
  fprintf (file, _("\t\t\tGenerate a base file for relocatable DLLs\n"));
'

PARSE_AND_LIST_ARGS_CASES='
    case OPTION_BASE_FILE:
      link_info.base_file = fopen (optarg, FOPEN_WB);
      if (link_info.base_file == NULL)
	{
	  /* xgettext:c-format */
	  fprintf (stderr, _("%s: Cannot open base file %s\n"),
		   program_name, optarg);
	  xexit (1);
	}
      break;
'
