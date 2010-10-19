#objdump: -dw
#name: i386 padlock

.*: +file format .*

Disassembly of section .text:

0+000 <foo>:
   0:[	 ]*0f a7 c0 [	 ]*xstore-rng 
   3:[	 ]*f3 0f a7 c0 [	 ]*repz xstore-rng 
   7:[	 ]*f3 0f a7 c8 [	 ]*repz xcrypt-ecb 
   b:[	 ]*f3 0f a7 c8 [	 ]*repz xcrypt-ecb 
   f:[	 ]*f3 0f a7 d0 [	 ]*repz xcrypt-cbc 
  13:[	 ]*f3 0f a7 d0 [	 ]*repz xcrypt-cbc 
  17:[	 ]*f3 0f a7 e0 [	 ]*repz xcrypt-cfb 
  1b:[	 ]*f3 0f a7 e0 [	 ]*repz xcrypt-cfb 
  1f:[	 ]*f3 0f a7 e8 [	 ]*repz xcrypt-ofb 
  23:[	 ]*f3 0f a7 e8 [	 ]*repz xcrypt-ofb 
  27:[	 ]*0f a7 c0 [	 ]*xstore-rng 
  2a:[	 ]*f3 0f a7 c0 [	 ]*repz xstore-rng 
  2e:[	 ]*f3 0f a6 c0 [	 ]*repz montmul 
  32:[	 ]*f3 0f a6 c0 [	 ]*repz montmul 
  36:[	 ]*f3 0f a6 c8 [	 ]*repz xsha1 
  3a:[	 ]*f3 0f a6 c8 [	 ]*repz xsha1 
  3e:[	 ]*f3 0f a6 d0 [	 ]*repz xsha256 
  42:[	 ]*f3 0f a6 d0 [	 ]*repz xsha256 
#pass
