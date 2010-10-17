# In microcomputer (MC) mode, the vectors are mapped into the on-chip ROM,
# otherwise in microprocessor (MP) mode the vectors are mapped to address 0
# on the external bus.  In MC mode, the on-chip ROM contains a bootloader program
# that loads the internal RAM from the serial port or external ROM.
#
# Common configurations:
# 1. MC mode, no external memory (serial boot).
# 2. MC mode, external RAM (serial boot).
# 3. MC mode, external ROM.
# 4. MC mode, external ROM, external RAM.
# 5. MP mode, external ROM.
# 6. MP mode, external ROM, external RAM.
# 7. MP mode, external RAM (dual-port with hosting CPU or external debugger).
#
# Config  TEXT     DATA/BSS
# 1.      INT_RAM  INT_RAM   (mcmode,onchip)
# 2.      EXT_RAM  EXT_RAM   (mcmode,extram)
# 3.      INT_RAM  INT_RAM   (mcmode,onchip)
# 4.      EXT_RAM  EXT_RAM   (mcmode,extram)
# 5.      EXT_ROM  INT_RAM   (mpmode,onchip,extrom)
# 6.      EXT_ROM  EXT_RAM   (mpmode,extram,extrom)
# 7.      EXT_RAM  EXT_RAM   (mpmode,extram)
#
# In MC mode, TEXT and DATA are copied into RAM by the bootloader. 
#
# In MP mode with external ROM, DATA needs to be copied into RAM at boot time.
#
# If there is external RAM it is better to use that and reserve the internal RAM 
# for data buffers.  However, the address of the external RAM needs to be specified.
#
# This emulation assumes config 7.

case $OUTPUT_ARCH in
  tic3x) OUTPUT_ARCHNAME="TMS320C3x" ;;
  tic4x) OUTPUT_ARCHNAME="TMS320C4x" ;;
esac

case $ONCHIP in
  yes) RAM=RAM;
       STACK_SIZE_DEFAULT=128;
       HEAP_SIZE_DEFAULT=0;
       ;;
  *)   RAM=EXT0;
       STACK_SIZE_DEFAULT=0x1000;
       HEAP_SIZE_DEFAULT=0x4000;
       ;;
esac

TEXT_MEMORY=$RAM;
DATA_MEMORY=$RAM;


MEMORY_DEF="
/* C30 memory space.  */
MEMORY
{
   EXT0  :  org = 0x0000000, len = 0x800000  /* External address bus.  */
   XBUS  :  org = 0x0800000, len = 0x002000  /* Expansion bus.         */
   IOBUS :  org = 0x0804000, len = 0x002000  /* I/O BUS.               */
   RAM0  :  org = 0x0809800, len = 0x000400  /* Internal RAM block 0.  */
   RAM1  :  org = 0x0809a00, len = 0x000400  /* Internal RAM block 1.  */
   RAM   :  org = 0x0809800, len = 0x000800  /* Internal RAM.          */
   EXT1  :  org = 0x080a000, len = 0x7f6000  /* External address bus.  */
}
"

test -z "$ENTRY" && ENTRY=_start

cat <<EOF
${RELOCATING+/* Linker script for $OUTPUT_ARCHNAME executable.  */}
${RELOCATING-/* Linker script for $OUTPUT_ARCHNAME object file (ld -r).  */}

OUTPUT_FORMAT("${OUTPUT_FORMAT}")
OUTPUT_ARCH("${OUTPUT_ARCH}")
${LIB_SEARCH_DIRS}
ENTRY(${ENTRY})

${RELOCATING+ __HEAP_SIZE = DEFINED(__HEAP_SIZE) ? __HEAP_SIZE : ${HEAP_SIZE_DEFAULT};}
${RELOCATING+ __STACK_SIZE  = DEFINED(__STACK_SIZE)  ? __STACK_SIZE  : ${STACK_SIZE_DEFAULT};}

${RELOCATING+${MEMORY_DEF}}

/* In the small memory model the .data and .bss sections must be contiguous
   when loaded and fit within the same page.   The DP register is loaded
   with the page address.  */

SECTIONS
{
  /* Reset, interrupt, and trap vectors.  */
  .vectors ${RELOCATING+ 0} : {
    *(.vectors)
  } ${RELOCATING+ > ${TEXT_MEMORY}}
  /* Constants.  */
  .const : {
    *(.const)
  } ${RELOCATING+ > ${TEXT_MEMORY}}
  /* Program code.  */
  .text : {
    ${RELOCATING+  __text =  .;}
    ${RELOCATING+ *(.init)}
    *(.text)
    ${CONSTRUCTING+ ___CTOR_LIST__ = .;}
    ${CONSTRUCTING+ LONG(___CTOR_END__ - ___CTOR_LIST__ - 2)}
    ${CONSTRUCTING+ *(.ctors)}
    ${CONSTRUCTING+ LONG(0);}
    ${CONSTRUCTING+ ___CTOR_END__  = .;}
    ${CONSTRUCTING+ ___DTOR_LIST__ = .;}
    ${CONSTRUCTING+ LONG(___DTOR_END__ - ___DTOR_LIST__ - 2)}
    ${CONSTRUCTING+ *(.dtors)}
    ${CONSTRUCTING+ LONG(0)}
    ${CONSTRUCTING+ ___DTOR_END__  = .;}
    ${RELOCATING+ *(.fini)}
    ${RELOCATING+  __etext =  .;}
  } ${RELOCATING+ > ${TEXT_MEMORY}}
  /* Global initialised variables.  */
  .data :
  { 				
    ${RELOCATING+  __data  =  .;}
    *(.data)
    ${RELOCATING+  __edata  = .;}
  } ${RELOCATING+ > ${DATA_MEMORY}}
  /* Global uninitialised variables.  */
  .bss : {
    ${RELOCATING+ __bss  =  .;}	
    *(.bss)
    *(COMMON)
    ${RELOCATING+  __end  =  .;}
  } ${RELOCATING+ > ${DATA_MEMORY}}
  /* Heap.  */
  .heap :
  { 					
    ${RELOCATING+ __heap  =  .;}		
    ${RELOCATING+ . += __HEAP_SIZE};
  } ${RELOCATING+ > ${DATA_MEMORY}}
  /* Stack (grows upward).  */
  .stack :
  { 				
    ${RELOCATING+ __stack  =  .;}		
    *(.stack)
    ${RELOCATING+ .  =  . + __STACK_SIZE};		
  } ${RELOCATING+ > ${DATA_MEMORY}}
  .stab 0 ${RELOCATING+(NOLOAD)} : 
  {
    [ .stab ]
  }
  .stabstr 0 ${RELOCATING+(NOLOAD)} :
  {
    [ .stabstr ]
  }
}
EOF
