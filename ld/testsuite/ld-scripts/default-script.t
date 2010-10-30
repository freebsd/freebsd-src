_START = DEFINED(_START) ? _START : 0x9000000;
SECTIONS
{
  . = _START;
  .text : {*(.text)}
  /DISCARD/ : {*(*)}
}
