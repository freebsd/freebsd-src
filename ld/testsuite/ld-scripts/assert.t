SECTIONS
{
  .empty : {
  here = !.;
  ASSERT (!., "dot is not zero");
  ASSERT (here, "here is zero");
  }
  ASSERT (!SIZEOF(.empty), "Empty is not empty")
  /DISCARD/ : { *(.reginfo) }
}
