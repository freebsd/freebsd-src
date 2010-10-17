#source: x.s
#source: y.s
#ld: -m mmo -Ttext 0xa00 -T $srcdir/$subdir/zeroeh.ld
#objdump: -s

# The word at address 201c, for the linkonce-excluded section, must be zero.

.*:     file format mmo

Contents of section \.text:
 0a00 00000a08 00000a10 00000001 00000002  .*
 0a10 00000003                             .*
Contents of section \.eh_frame:
 2000 00000002 00000a08 00000008 00000007  .*
 2010 00000a10 00000004 00006066 00000000  .*
 2020 00000004                             .*
