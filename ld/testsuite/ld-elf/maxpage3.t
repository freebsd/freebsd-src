SECTIONS
{
  .text : {*(.text)}
  . = ALIGN(CONSTANT (MAXPAGESIZE));
  .data : {*(.data)}
  /DISCARD/ : {*(*)}
}
