SECTIONS
{
  .text : {*(SORT_BY_NAME(.text*))}
  /DISCARD/ : { *(.*) }
}
