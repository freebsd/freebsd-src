#as: --underscore --em=criself --march=v32
#objdump: -dr

# Test that v32 flags are properly recognized and emitted at disassembly.

.*:     file format elf32-us-cris

Disassembly of section \.text:

00000000 <x>:
   0:	b105                	setf c
   2:	f105                	clearf c
   4:	f205                	clearf v
   6:	b205                	setf v
   8:	b405                	setf z
   a:	f405                	clearf z
   c:	f805                	clearf n
   e:	b805                	setf n
  10:	b015                	ax 
  12:	f015                	clearf x
  14:	b025                	ei 
  16:	f025                	di 
  18:	f045                	clearf u
  1a:	b045                	setf u
  1c:	b085                	setf p
  1e:	f085                	clearf p
