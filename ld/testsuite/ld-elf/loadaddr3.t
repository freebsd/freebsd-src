
MEMORY
{
  rom (rx) : ORIGIN = 0x100, LENGTH = 0x100
  ram (rwx) : ORIGIN = 0x200, LENGTH = 0x100

}

SECTIONS
{
  .text : {*(.text .text.*)} >rom
  .data : {data_load = LOADADDR (.data);
	   data_start = ADDR (.data);
	   *(.data .data.*)} >ram AT>rom
  /DISCARD/ : { *(.*) }
}
