#objdump: -d
#name: MRI moveml
#as: -M

.*: +file format .*

Disassembly of section \.text:

0+000 <\.text>:
   0:	4cdf 07fc      	moveml %sp@\+,%d2-%a2
   4:	4cdf 07fc      	moveml %sp@\+,%d2-%a2
   8:	48f9 07fc 0000 	moveml %d2-%a2,0 <\.text>
   e:	0000 
  10:	48f9 07fc 0000 	moveml %d2-%a2,0 <\.text>
  16:	0000 
  18:	4cf9 07fc 0000 	moveml 0 <\.text>,%d2-%a2
  1e:	0000 
  20:	4cf9 07fc 0000 	moveml 0 <\.text>,%d2-%a2
  26:	0000 
  28:	4cf9 07fc 0001 	moveml 16000 <.*>,%d2-%a2
  2e:	6000 
  30:	4cf9 07fc 0001 	moveml 16000 <.*>,%d2-%a2
  36:	6000 
  38:	48f9 07fc 0001 	moveml %d2-%a2,16000 <.*>
  3e:	6000 
  40:	48f9 07fc 0001 	moveml %d2-%a2,16000 <.*>
  46:	6000 
