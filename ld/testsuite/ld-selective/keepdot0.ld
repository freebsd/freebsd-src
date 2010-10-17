SECTIONS
{
  . = 0x0;
  .myinit : { KEEP (*(.myinit)) }
  .mytext : { *(.mytext*) *(.text*) }
}
