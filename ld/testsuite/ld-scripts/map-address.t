SECTIONS
{
  . = 0x10001;
  foo = .;
  . += 0x200;
  bar = .;
  . = ALIGN (4);
  frob = .;
}
