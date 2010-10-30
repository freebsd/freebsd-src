ENTRY(RAM)

MEMORY
{
  ram (rwx) : ORIGIN = 0, LENGTH = 0x1000000
}

SECTIONS
{
.text : { } >ram
}
RAM = ADDR(ram);
