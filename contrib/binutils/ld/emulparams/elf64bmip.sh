# This is an ELF platform.
SCRIPT_NAME=elf

# Handle both big- and little-ended 32-bit MIPS objects.
ARCH=mips
OUTPUT_FORMAT="elf64-bigmips"
BIG_OUTPUT_FORMAT="elf64-bigmips"
LITTLE_OUTPUT_FORMAT="elf64-littlemips"

# Note that the elf32 template is used for 64-bit emulations as well 
# as 32-bit emulations.
ELFSIZE=64
TEMPLATE_NAME=elf32

TEXT_START_ADDR=0x10000000
MAXPAGESIZE=0x100000
ENTRY=__start

# GOT-related settings.  
OTHER_GOT_SYMBOLS='
  _gp = ALIGN(16) + 0x7ff0;
'
OTHER_GOT_SECTIONS='
  .lit8 : { *(.lit8) }
  .lit4 : { *(.lit4) }
  .srdata : { *(.srdata) }
'

# Magic symbols.
TEXT_START_SYMBOLS='_ftext = . ;'
DATA_START_SYMBOLS='_fdata = . ;'
OTHER_BSS_SYMBOLS='_fbss = .;'
# IRIX6 defines these symbols.  0x40 is the size of the ELF header.
EXECUTABLE_SYMBOLS="
  __dso_displacement = 0;
  __elf_header = ${TEXT_START_ADDR};
  __program_header_table = ${TEXT_START_ADDR} + 0x40;
"

# There are often dynamic relocations against the .rodata section.
# Setting DT_TEXTREL in the .dynamic section does not convince the
# IRIX6 linker to permit relocations against the text segment.
# Following the IRIX linker, we simply put .rodata in the data
# segment.
WRITABLE_RODATA=


OTHER_RELOCATING_SECTIONS='
  .MIPS.events.text :
    {
       *(.MIPS.events.text)
       *(.MIPS.events.gnu.linkonce.t*)
    }
  .MIPS.content.text : 
    {
       *(.MIPS.content.text)
       *(.MIPS.content.gnu.linkonce.t*)
    }
  .MIPS.events.data : 
    {
       *(.MIPS.events.data)
       *(.MIPS.events.gnu.linkonce.d*)
    }
  .MIPS.content.data : 
    {
       *(.MIPS.content.data)
       *(.MIPS.content.gnu.linkonce.d*)
    }
  .MIPS.events.rodata : 
    {
       *(.MIPS.events.rodata)
       *(.MIPS.events.gnu.linkonce.r*)
    }
  .MIPS.content.rodata : 
    {
       *(.MIPS.content.rodata)
       *(.MIPS.content.gnu.linkonce.r*)
    }
'
