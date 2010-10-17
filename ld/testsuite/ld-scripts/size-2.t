PHDRS
{
  header PT_PHDR FILEHDR PHDRS ;
	 
  image PT_LOAD FLAGS (5);
  tls PT_TLS FLAGS (4);
  
}
SECTIONS
{
  .text 0x100 : { *(.text) } :image
  .tdata : { *(.tdata) } :image :tls
  .tbss : { *(.tbss) } :image : tls
  .map : {
    LONG (SIZEOF (.text))
    LONG (SIZEOF (.data))
    LONG (SIZEOF (.bss))
    LONG (SIZEOF (.tdata))
    LONG (SIZEOF (.tbss))
  } :image
}
