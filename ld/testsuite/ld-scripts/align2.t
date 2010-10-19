SECTIONS
{
  .text : {*(.text)}
  . = ALIGN(data_align);
  .data : {*(.data)}
  /DISCARD/ : {*(*)}
}
