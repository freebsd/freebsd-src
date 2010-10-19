SECTIONS
{
  .text : {*(SORT_BY_ALIGNMENT(.text*))}
  /DISCARD/ : { *(.*) }
}
