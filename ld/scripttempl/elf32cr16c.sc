# Linker Script for National Semiconductor's CR16C-ELF32.

test -z "$ENTRY" && ENTRY=_start
cat <<EOF

/* Example Linker Script for linking NS CR16C or CR16CPlus
   elf32 files, which were compiled with either the near data
   model or the default data model.  */

/* Force the entry point to be entered in the output file as
   an undefined symbol. This is needed in case the entry point
   (which is not called explicitly) is in an archive (which is
   the usual case).  */

EXTERN(${ENTRY})

ENTRY(${ENTRY})

MEMORY
{
  near_rom  : ORIGIN = 0x4,     LENGTH = 512K - 4
  near_ram  : ORIGIN = 512K,    LENGTH = 512K - 64K
  rom  	    : ORIGIN = 1M,      LENGTH = 3M
  ram 	    : ORIGIN = 4M,      LENGTH = 10M
}

SECTIONS
{
/* The heap is located in near memory, to suit both the near and
   default data models.  The heap and stack are aligned to the bus
   width, as a speed optimization for accessing  data located
   there. The alignment to 4 bytes is compatible for both the CR16C
   bus width (2 bytes) and CR16CPlus bus width (4 bytes).  */

  .text          : { __TEXT_START = .;   *(.text)                                        __TEXT_END = .; } > rom	
  .rdata         : { __RDATA_START = .;  *(.rdata_4) *(.rdata_2) *(.rdata_1)             __RDATA_END = .; } > near_rom
  .ctor ALIGN(4) : { __CTOR_LIST = .;    *(.ctors)                                       __CTOR_END = .; } > near_rom
  .dtor ALIGN(4) : { __DTOR_LIST = .;    *(.dtors)                                       __DTOR_END = .; } > near_rom
  .data          : { __DATA_START = .;   *(.data_4) *(.data_2) *(.data_1) *(.data)       __DATA_END = .; } > ram AT > rom
  .bss (NOLOAD)  : { __BSS_START = .;    *(.bss_4) *(.bss_2) *(.bss_1) *(.bss) *(COMMON) __BSS_END = .; } > ram
  .nrdata        : { __NRDATA_START = .; *(.nrdat_4) *(.nrdat_2) *(.nrdat_1)             __NRDATA_END =  .; } > near_rom
  .ndata         : { __NDATA_START = .;  *(.ndata_4) *(.ndata_2) *(.ndata_1)             __NDATA_END = .; } > near_ram AT > rom
  .nbss (NOLOAD) : { __NBSS_START = .;   *(.nbss_4) *(.nbss_2) *(.nbss_1) *(.ncommon)    __NBSS_END = .; } > near_ram
  .heap          : { . = ALIGN(4); __HEAP_START = .; . += 0x2000;                        __HEAP_MAX = .; } > near_ram
  .stack         : { . = ALIGN(4); . += 0x6000; __STACK_START = .; } > ram
  .istack        : { . = ALIGN(2); . += 0x100; __ISTACK_START = .; } > ram
}

__DATA_IMAGE_START = LOADADDR(.data);
__NDATA_IMAGE_START = LOADADDR(.ndata);

EOF
