#objdump: -dw
#name: x86-64 CBW/CWD & Co

.*: +file format .*

Disassembly of section .text:

0+000 <_cbw>:
   0:	66 98                	cbtw   
   2:	98                   	cwtl   
   3:	48 98                	cltq   
   5:	66 40 98             	rex cbtw   
   8:	40 98                	rex cwtl   
   a:	66                   	data16
   b:	48 98                	cltq   

0+00d <_cwd>:
   d:	66 99                	cwtd   
   f:	99                   	cltd   
  10:	48 99                	cqto   
  12:	66 40 99             	rex cwtd   
  15:	40 99                	rex cltd   
  17:	66                   	data16
  18:	48 99                	cqto   
#pass
