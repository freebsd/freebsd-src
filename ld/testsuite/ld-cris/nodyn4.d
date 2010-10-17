#source: expdyn1.s
#source: expdref1.s --pic
#source: comref1.s --pic
#as: --no-underscore
#ld: -m crislinux
#readelf: -l

# Like expdyn4.d, but no --export-dynamic.  Got a BFD_ASSERT at one time.
# Check that we get the expected sections.

#...
There are 2 program headers, .*
#...
  LOAD [0-9a-fx ]+ R E 0x2000
  LOAD [0-9a-fx ]+ RW  0x2000
#...
   00     \.text[ ]*
   01     \.data \.got \.bss[ ]*
#pass
