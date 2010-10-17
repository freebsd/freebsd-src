#objdump: -d -r
#name: c54x extended addressing

.*: +file format .*c54x.*

Disassembly of section .text:

00000000 <.text>:
       0:	f062.*
       1:	0000.*
.*1: RELEXTMS7.*
       2:	f040.*
       3:	0000.*
.*3: RELEXT16.*
       4:	f4e2.*

00000005 <start>:
       5:	f881.*
       6:	0080.*
.*5: ARELEXT.*
       7:	fa81.*
       8:	0080.*
.*7: ARELEXT.*
       9:	f495.*
       a:	f495.*
       b:	f4e6.*
       c:	f6e6.*
       d:	f495.*
       e:	f495.*
       f:	f4e7.*
      10:	f7e7.*
      11:	f495.*
      12:	f495.*
      13:	f981.*
      14:	0080.*
.*13: ARELEXT.*
      15:	fb81.*
      16:	0080.*
.*15: ARELEXT.*
      17:	f495.*
      18:	f495.*
      19:	f4e4.*
      1a:	f6e4.*
      1b:	f495.*
      1c:	f495.*
      1d:	f4e5.*
      1e:	f6e5.*
      1f:	f495.*
      20:	f495.*
	...

00010080 <end>:
   10080:	f881.*
   10081:	0080.*
.*10080: ARELEXT.*
