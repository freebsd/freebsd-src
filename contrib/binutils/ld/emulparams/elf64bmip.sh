. ${srcdir}/emulparams/elf32bmipn32.sh
OUTPUT_FORMAT="elf64-bigmips"
BIG_OUTPUT_FORMAT="elf64-bigmips"
LITTLE_OUTPUT_FORMAT="elf64-littlemips"
ELFSIZE=64

# IRIX6 defines these symbols.  0x40 is the size of the ELF header.
EXECUTABLE_SYMBOLS="
  __dso_displacement = 0;
  __elf_header = ${TEXT_START_ADDR};
  __program_header_table = ${TEXT_START_ADDR} + 0x40;
"
