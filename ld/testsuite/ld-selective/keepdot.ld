SECTIONS
{
  .myinit : { KEEP (*(.myinit)) }
  .mytext : { *(.mytext*) *(.text*) }
}
