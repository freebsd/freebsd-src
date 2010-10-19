SECTIONS
{
  .text : {*(SORT_BY_ALIGNMENT(SORT_BY_ALIGNMENT(.text*)))}
  /DISCARD/ : { *(.*) }
}
