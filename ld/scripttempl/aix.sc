# AIX linker script.
# AIX always uses shared libraries.  The section VMA appears to be
# unimportant.  The native linker aligns the sections on boundaries
# specified by the -H option.

cat <<EOF
OUTPUT_ARCH(${ARCH})
${RELOCATING+${LIB_SEARCH_DIRS}}
ENTRY(__start)
SECTIONS
{
  .pad 0 : { *(.pad) }

  . = 0x10000000;
  .text ${RELOCATING-0} : {
    ${RELOCATING+PROVIDE (_text = .);}
    *(.text)
    *(.pr)
    *(.ro)
    *(.db)
    *(.gl)
    *(.xo)
    *(.ti)
    *(.tb)
    ${RELOCATING+PROVIDE (_etext = .);}
  }
  . = ALIGN (0x10000000);
  .data . : {
    ${RELOCATING+PROVIDE (_data = .);}
    *(.data)
    *(.rw)
    *(.sv)
    *(.sv64)
    *(.sv3264)
    *(.ua)
    . = ALIGN(4);
    ${CONSTRUCTING+CONSTRUCTORS}
    *(.ds)
    *(.tc0)
    *(.tc)
    *(.td)
    ${RELOCATING+PROVIDE (_edata = .);}
  }
  .bss : {
    *(.tocbss)
    *(.bss)
    *(.bs)
    *(.uc)
    *(COMMON)
    ${RELOCATING+PROVIDE (_end = .);}
    ${RELOCATING+PROVIDE (end = .);}
  }

  .loader : {
    *(.loader)
  }
  
  .debug : {
    *(.debug)
  }
}
EOF
