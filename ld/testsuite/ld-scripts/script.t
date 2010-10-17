SECTIONS
{
  .text 0x100 : { 
    text_start = .;
    *(.text)
    *(.pr)
    text_end = .;
  }
  . = 0x1000;
  .data : {
    data_start = .;
    *(.data)
    *(.rw)
    data_end = .;
  }
}
