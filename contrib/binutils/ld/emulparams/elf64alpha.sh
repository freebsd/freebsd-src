ENTRY=_start
SCRIPT_NAME=elf
ELFSIZE=64
TEMPLATE_NAME=elf32
OUTPUT_FORMAT="elf64-alpha"
TEXT_START_ADDR="0x120000000"
MAXPAGESIZE=0x10000
COMMONPAGESIZE=0x2000
NONPAGED_TEXT_START_ADDR="0x120000000"
ARCH=alpha
MACHINE=
GENERATE_SHLIB_SCRIPT=yes
DATA_PLT=
# Note that the number is always big-endian, thus we have to 
# reverse the digit string.
NOP=0x0000fe2f1f04ff47		# unop; nop

OTHER_READONLY_SECTIONS="
  .reginfo      ${RELOCATING-0} : { *(.reginfo) }"

# This code gets inserted into the generic elf32.sc linker script
# and allows us to define our own command line switches.
PARSE_AND_LIST_PROLOGUE='
#define OPTION_TASO            300
/* Set the start address as in the Tru64 ld */
#define ALPHA_TEXT_START_32BIT 0x12000000

static int elf64alpha_32bit = 0;

struct ld_emulation_xfer_struct ld_elf64alpha_emulation;
static void gld_elf64alpha_finish PARAMS ((void));
'

PARSE_AND_LIST_LONGOPTS='
  {"taso", no_argument, NULL, OPTION_TASO},
'

PARSE_AND_LIST_OPTIONS='
  fprintf (file, _("  -taso\t\t\tLoad executable in the lower 31-bit addressable\n"));
  fprintf (file, _("\t\t\t  virtual address range\n"));
'

PARSE_AND_LIST_ARGS_CASES='
    case EOF:
      if (elf64alpha_32bit && !link_info.shared && !link_info.relocateable)
	{
	  lang_section_start (".interp",
			      exp_binop ('\''+'\'',
					 exp_intop (ALPHA_TEXT_START_32BIT),
					 exp_nameop (SIZEOF_HEADERS, NULL)));
	  ld_elf64alpha_emulation.finish = gld_elf64alpha_finish;
	}
      return 0;

    case OPTION_TASO:
      elf64alpha_32bit = 1;
      break;
'

PARSE_AND_LIST_EPILOGUE='
#include "elf/internal.h"
#include "elf/alpha.h"
#include "elf-bfd.h"

static void
gld_elf64alpha_finish()
{
  elf_elfheader (output_bfd)->e_flags |= EF_ALPHA_32BIT;
}
'
