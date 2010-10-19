#notarget: cris*-*-linux-gnu
#as: --em=criself --march=v32
#ld: -m criself --oformat binary --defsym ext1=0x4000 --defsym ext2=0x6000
#objdump: -s -b binary

# Test that pcrel relocs work with --oformat binary.
# Source code and "-m criself" doesn't work with *-linux-gnu.

.*:     file format binary

Contents of section \.data:
 0000 7f5d0020 0000bfbe fa7f0000 b0057f3d  .*
 0010 f23f0000 bfbeec5f 0000b005           .*
