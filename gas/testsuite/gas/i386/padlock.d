#objdump: -dw
#name: i386 padlock

.*: +file format .*

Disassembly of section .text:

00000000 <foo>:
   0:[	 ]*0f a7 c0 [	 ]*xstorerng 
   3:[	 ]*f3 0f a7 c0 [	 ]*repz xstorerng 
   7:[	 ]*f3 0f a7 c8 [	 ]*repz xcryptecb 
   b:[	 ]*f3 0f a7 c8 [	 ]*repz xcryptecb 
   f:[	 ]*f3 0f a7 d0 [	 ]*repz xcryptcbc 
  13:[	 ]*f3 0f a7 d0 [	 ]*repz xcryptcbc 
  17:[	 ]*f3 0f a7 e0 [	 ]*repz xcryptcfb 
  1b:[	 ]*f3 0f a7 e0 [	 ]*repz xcryptcfb 
  1f:[	 ]*f3 0f a7 e8 [	 ]*repz xcryptofb 
  23:[	 ]*f3 0f a7 e8 [	 ]*repz xcryptofb 
  27:[	 ]*0f a7 c0 [	 ]*xstorerng 
  2a:[	 ]*f3 0f a7 c0 [	 ]*repz xstorerng 
[ 	]*\.\.\.
