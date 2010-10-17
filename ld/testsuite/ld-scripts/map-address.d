#...
Linker script and memory map
#...
 *0x0*010001 *\. = 0x10001
 *0x0*010001 *foo = \.
 *0x0*010201 *\. = \(\. \+ 0x200\)
 *0x0*010201 *bar = \.
 *0x0*010204 *\. = ALIGN \(0x4\)
 *0x0*010204 *frob = \.
#pass
