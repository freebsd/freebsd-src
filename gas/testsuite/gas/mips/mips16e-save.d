#objdump: -dr -mmips:isa32 -mmips:16
#as: -march=mips32 -mips16 -32
#name: mips16e save/restore

.*: +file format .*mips.*

Disassembly of section .text:
00000000 <func>:
   0:[ 	]+6481[ 	]+save[ 	]+8
   2:[ 	]+64c2[ 	]+save[ 	]+16,ra
   4:[ 	]+64a3[ 	]+save[ 	]+24,s0
   6:[ 	]+6494[ 	]+save[ 	]+32,s1
   8:[ 	]+64b5[ 	]+save[ 	]+40,s0-s1
   a:[ 	]+64e6[ 	]+save[ 	]+48,ra,s0
   c:[ 	]+64d7[ 	]+save[ 	]+56,ra,s1
   e:[ 	]+64f8[ 	]+save[ 	]+64,ra,s0-s1
  10:[ 	]+64f9[ 	]+save[ 	]+72,ra,s0-s1
  12:[ 	]+64fa[ 	]+save[ 	]+80,ra,s0-s1
  14:[ 	]+64fb[ 	]+save[ 	]+88,ra,s0-s1
  16:[ 	]+64f0[ 	]+save[ 	]+128,ra,s0-s1
  18:[ 	]+f010 6481[ 	]+save[ 	]+136
  1c:[ 	]+f010 64c2[ 	]+save[ 	]+144,ra
  20:[ 	]+f010 64b3[ 	]+save[ 	]+152,s0-s1
  24:[ 	]+f100 6488[ 	]+save[ 	]+64,s2
  28:[ 	]+f600 6489[ 	]+save[ 	]+72,s2-s7
  2c:[ 	]+f700 648a[ 	]+save[ 	]+80,s2-s8
  30:[ 	]+f700 64bb[ 	]+save[ 	]+88,s0-s8
  34:[ 	]+f001 6488[ 	]+save[ 	]+64,a3
  38:[ 	]+f012 6480[ 	]+save[ 	]+128,a2-a3
  3c:[ 	]+f02b 6480[ 	]+save[ 	]+256,a0-a3
  40:[ 	]+f024 6480[ 	]+save[ 	]+a0,256
  44:[ 	]+f018 6480[ 	]+save[ 	]+a0-a1,128
  48:[ 	]+f00e 6488[ 	]+save[ 	]+a0-a3,64
  4c:[ 	]+f015 6480[ 	]+save[ 	]+a0,128,a3
  50:[ 	]+f017 6480[ 	]+save[ 	]+a0,128,a1-a3
  54:[ 	]+f01a 6480[ 	]+save[ 	]+a0-a1,128,a2-a3
  58:[ 	]+f01d 6480[ 	]+save[ 	]+a0-a2,128,a3
  5c:[ 	]+f71a 64f0[ 	]+save[ 	]+a0-a1,128,ra,s0-s8,a2-a3
  60:[ 	]+6470[ 	]+restore[ 	]+128,ra,s0-s1
  62:[ 	]+f010 6441[ 	]+restore[ 	]+136,ra
  66:[ 	]+f100 6408[ 	]+restore[ 	]+64,s2
  6a:[ 	]+f71a 6470[ 	]+restore[ 	]+a0-a1,128,ra,s0-s8,a2-a3
  6e:[ 	]+6500[ 	]+nop
