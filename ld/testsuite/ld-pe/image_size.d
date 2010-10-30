#name: PE-COFF SizeOfImage
#ld: -T image_size.t
#objdump: -p
#target: i*86-*-mingw32

.*:     file format .*
#...
SizeOfImage		00004000
#...
