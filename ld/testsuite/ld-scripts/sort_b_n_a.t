SECTIONS
{
  .text : {*(SORT_BY_NAME(SORT_BY_ALIGNMENT(.text*)))}
  /DISCARD/ : { *(.*) }
}
