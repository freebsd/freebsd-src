#as: -64 -Av9
#objdump: -dr
#name: sparc64 prefetch

.*: +file format .*sparc.*

Disassembly of section .text:

0+ <.text>:
   0:	c1 68 40 00 	prefetch  \[ %g1 \], #n_reads
   4:	ff 68 40 00 	prefetch  \[ %g1 \], 31
   8:	c1 68 40 00 	prefetch  \[ %g1 \], #n_reads
   c:	c3 68 40 00 	prefetch  \[ %g1 \], #one_read
  10:	c5 68 40 00 	prefetch  \[ %g1 \], #n_writes
  14:	c7 68 40 00 	prefetch  \[ %g1 \], #one_write
  18:	c1 e8 42 00 	prefetcha  \[ %g1 \] #ASI_AIUP, #n_reads
  1c:	ff e8 60 00 	prefetcha  \[ %g1 \] %asi, 31
  20:	c1 e8 42 20 	prefetcha  \[ %g1 \] #ASI_AIUS, #n_reads
  24:	c3 e8 60 00 	prefetcha  \[ %g1 \] %asi, #one_read
