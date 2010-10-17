SECTIONS {
  .text : {
     *(.text)
     LONG(0x12345678);
  }
}
