SECTIONS
{
  .text : {*(.text .text.*)}
  /DISCARD/ : { *(.*) }
}
