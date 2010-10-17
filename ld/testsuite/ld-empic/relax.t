OUTPUT_FORMAT("ecoff-bigmips")
SECTIONS
{
  .foo 0x30 : {
    tmpdir/relax3.o(.text)
    tmpdir/relax1.o(.text)
  }
  .text  0x20000 : {
     _ftext = . ;
    *(.init)
     eprol  =  .;
    tmpdir/relax4.o(.text)
    *(.text)
    *(.fini)
     etext  =  .;
     _etext  =  .;
  }
  .rdata  . : {
    *(.rdata)
  }
   _fdata = .;
  .data  . : {
    *(.data)
    CONSTRUCTORS
  }
   _gp = . + 0x8000;
  .lit8  . : {
    *(.lit8)
  }
  .lit4  . : {
    *(.lit4)
  }
  .sdata  . : {
    *(.sdata)
  }
   edata  =  .;
   _edata  =  .;
   _fbss = .;
  .sbss  . : {
    *(.sbss)
    *(.scommon)
  }
  .bss  . : {
    *(.bss)
    *(COMMON)
  }
   end = .;
   _end = .;
}
