#as: -Av9
#objdump: -dr
#name: sparc64 asi

.*: +file format .*sparc.*

Disassembly of section .text:

0+ <.text>:
   0:	c4 80 40 00 	lda  \[ %g1 \] \(0\), %g2
   4:	c4 80 5f e0 	lda  \[ %g1 \] \(255\), %g2
   8:	c4 80 42 00 	lda  \[ %g1 \] #ASI_AIUP, %g2
   c:	c4 80 42 20 	lda  \[ %g1 \] #ASI_AIUS, %g2
  10:	c4 80 43 00 	lda  \[ %g1 \] #ASI_AIUP_L, %g2
  14:	c4 80 43 20 	lda  \[ %g1 \] #ASI_AIUS_L, %g2
  18:	c4 80 50 00 	lda  \[ %g1 \] #ASI_P, %g2
  1c:	c4 80 50 20 	lda  \[ %g1 \] #ASI_S, %g2
  20:	c4 80 50 40 	lda  \[ %g1 \] #ASI_PNF, %g2
  24:	c4 80 50 60 	lda  \[ %g1 \] #ASI_SNF, %g2
  28:	c4 80 51 00 	lda  \[ %g1 \] #ASI_P_L, %g2
  2c:	c4 80 51 20 	lda  \[ %g1 \] #ASI_S_L, %g2
  30:	c4 80 51 40 	lda  \[ %g1 \] #ASI_PNF_L, %g2
  34:	c4 80 51 60 	lda  \[ %g1 \] #ASI_SNF_L, %g2
  38:	c4 80 42 00 	lda  \[ %g1 \] #ASI_AIUP, %g2
  3c:	c4 80 42 20 	lda  \[ %g1 \] #ASI_AIUS, %g2
  40:	c4 80 43 00 	lda  \[ %g1 \] #ASI_AIUP_L, %g2
  44:	c4 80 43 20 	lda  \[ %g1 \] #ASI_AIUS_L, %g2
  48:	c4 80 50 00 	lda  \[ %g1 \] #ASI_P, %g2
  4c:	c4 80 50 20 	lda  \[ %g1 \] #ASI_S, %g2
  50:	c4 80 50 40 	lda  \[ %g1 \] #ASI_PNF, %g2
  54:	c4 80 50 60 	lda  \[ %g1 \] #ASI_SNF, %g2
  58:	c4 80 51 00 	lda  \[ %g1 \] #ASI_P_L, %g2
  5c:	c4 80 51 20 	lda  \[ %g1 \] #ASI_S_L, %g2
  60:	c4 80 51 40 	lda  \[ %g1 \] #ASI_PNF_L, %g2
  64:	c4 80 51 60 	lda  \[ %g1 \] #ASI_SNF_L, %g2
