#as: -Av9
#objdump: -dr
#name: sparc64 membar

.*: +file format .*sparc.*

Disassembly of section .text:

0+ <.text>:
   0:	81 43 e0 00 	membar  0
   4:	81 43 e0 7f 	membar  #Sync|#MemIssue|#Lookaside|#StoreStore|#LoadStore|#StoreLoad|#LoadLoad
   8:	81 43 e0 7f 	membar  #Sync|#MemIssue|#Lookaside|#StoreStore|#LoadStore|#StoreLoad|#LoadLoad
   c:	81 43 e0 40 	membar  #Sync
  10:	81 43 e0 20 	membar  #MemIssue
  14:	81 43 e0 10 	membar  #Lookaside
  18:	81 43 e0 08 	membar  #StoreStore
  1c:	81 43 e0 04 	membar  #LoadStore
  20:	81 43 e0 02 	membar  #StoreLoad
  24:	81 43 e0 01 	membar  #LoadLoad
