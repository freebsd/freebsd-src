#source: x86-64-cbw.s
#objdump: -dwMintel
#name: x86-64 CBW/CWD & Co (Intel disassembly)

.*: +file format .*

Disassembly of section .text:

0+000 <_cbw>:
   0:	66 98                	cbw    
   2:	98                   	cwde   
   3:	48 98                	cdqe   
   5:	66 40 98             	rex cbw    
   8:	40 98                	rex cwde   
   a:	66                   	data16
   b:	48 98                	cdqe   

0+00d <_cwd>:
   d:	66 99                	cwd    
   f:	99                   	cdq    
  10:	48 99                	cqo    
  12:	66 40 99             	rex cwd    
  15:	40 99                	rex cdq    
  17:	66                   	data16
  18:	48 99                	cqo    
#pass
