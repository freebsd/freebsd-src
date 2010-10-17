SECTIONS
{
  .data : {
    tmpdir/weak-undef.o(.data)
  }
  /DISCARD/ : {
    *(*)
  }
}
