#objdump: -dw
#name: x86-64 merom

.*: +file format .*

Disassembly of section .text:

0+000 <foo>:
   0:	0f 38 01 01[ 	]+phaddw \(%rcx\),%mm0
   4:	0f 38 01 c1[ 	]+phaddw %mm1,%mm0
   8:	66 0f 38 01 01[ 	]+phaddw \(%rcx\),%xmm0
   d:	66 0f 38 01 c1[ 	]+phaddw %xmm1,%xmm0
  12:	0f 38 02 01[ 	]+phaddd \(%rcx\),%mm0
  16:	0f 38 02 c1[ 	]+phaddd %mm1,%mm0
  1a:	66 0f 38 02 01[ 	]+phaddd \(%rcx\),%xmm0
  1f:	66 0f 38 02 c1[ 	]+phaddd %xmm1,%xmm0
  24:	0f 38 03 01[ 	]+phaddsw \(%rcx\),%mm0
  28:	0f 38 03 c1[ 	]+phaddsw %mm1,%mm0
  2c:	66 0f 38 03 01[ 	]+phaddsw \(%rcx\),%xmm0
  31:	66 0f 38 03 c1[ 	]+phaddsw %xmm1,%xmm0
  36:	0f 38 05 01[ 	]+phsubw \(%rcx\),%mm0
  3a:	0f 38 05 c1[ 	]+phsubw %mm1,%mm0
  3e:	66 0f 38 05 01[ 	]+phsubw \(%rcx\),%xmm0
  43:	66 0f 38 05 c1[ 	]+phsubw %xmm1,%xmm0
  48:	0f 38 06 01[ 	]+phsubd \(%rcx\),%mm0
  4c:	0f 38 06 c1[ 	]+phsubd %mm1,%mm0
  50:	66 0f 38 06 01[ 	]+phsubd \(%rcx\),%xmm0
  55:	66 0f 38 06 c1[ 	]+phsubd %xmm1,%xmm0
  5a:	0f 38 07 01[ 	]+phsubsw \(%rcx\),%mm0
  5e:	0f 38 07 c1[ 	]+phsubsw %mm1,%mm0
  62:	66 0f 38 07 01[ 	]+phsubsw \(%rcx\),%xmm0
  67:	66 0f 38 07 c1[ 	]+phsubsw %xmm1,%xmm0
  6c:	0f 38 04 01[ 	]+pmaddubsw \(%rcx\),%mm0
  70:	0f 38 04 c1[ 	]+pmaddubsw %mm1,%mm0
  74:	66 0f 38 04 01[ 	]+pmaddubsw \(%rcx\),%xmm0
  79:	66 0f 38 04 c1[ 	]+pmaddubsw %xmm1,%xmm0
  7e:	0f 38 0b 01[ 	]+pmulhrsw \(%rcx\),%mm0
  82:	0f 38 0b c1[ 	]+pmulhrsw %mm1,%mm0
  86:	66 0f 38 0b 01[ 	]+pmulhrsw \(%rcx\),%xmm0
  8b:	66 0f 38 0b c1[ 	]+pmulhrsw %xmm1,%xmm0
  90:	0f 38 00 01[ 	]+pshufb \(%rcx\),%mm0
  94:	0f 38 00 c1[ 	]+pshufb %mm1,%mm0
  98:	66 0f 38 00 01[ 	]+pshufb \(%rcx\),%xmm0
  9d:	66 0f 38 00 c1[ 	]+pshufb %xmm1,%xmm0
  a2:	0f 38 08 01[ 	]+psignb \(%rcx\),%mm0
  a6:	0f 38 08 c1[ 	]+psignb %mm1,%mm0
  aa:	66 0f 38 08 01[ 	]+psignb \(%rcx\),%xmm0
  af:	66 0f 38 08 c1[ 	]+psignb %xmm1,%xmm0
  b4:	0f 38 09 01[ 	]+psignw \(%rcx\),%mm0
  b8:	0f 38 09 c1[ 	]+psignw %mm1,%mm0
  bc:	66 0f 38 09 01[ 	]+psignw \(%rcx\),%xmm0
  c1:	66 0f 38 09 c1[ 	]+psignw %xmm1,%xmm0
  c6:	0f 38 0a 01[ 	]+psignd \(%rcx\),%mm0
  ca:	0f 38 0a c1[ 	]+psignd %mm1,%mm0
  ce:	66 0f 38 0a 01[ 	]+psignd \(%rcx\),%xmm0
  d3:	66 0f 38 0a c1[ 	]+psignd %xmm1,%xmm0
  d8:	0f 3a 0f 01 02[ 	]+palignr \$0x2,\(%rcx\),%mm0
  dd:	0f 3a 0f c1 02[ 	]+palignr \$0x2,%mm1,%mm0
  e2:	66 0f 3a 0f 01 02[ 	]+palignr \$0x2,\(%rcx\),%xmm0
  e8:	66 0f 3a 0f c1 02[ 	]+palignr \$0x2,%xmm1,%xmm0
  ee:	0f 38 1c 01[ 	]+pabsb  \(%rcx\),%mm0
  f2:	0f 38 1c c1[ 	]+pabsb  %mm1,%mm0
  f6:	66 0f 38 1c 01[ 	]+pabsb  \(%rcx\),%xmm0
  fb:	66 0f 38 1c c1[ 	]+pabsb  %xmm1,%xmm0
 100:	0f 38 1d 01[ 	]+pabsw  \(%rcx\),%mm0
 104:	0f 38 1d c1[ 	]+pabsw  %mm1,%mm0
 108:	66 0f 38 1d 01[ 	]+pabsw  \(%rcx\),%xmm0
 10d:	66 0f 38 1d c1[ 	]+pabsw  %xmm1,%xmm0
 112:	0f 38 1e 01[ 	]+pabsd  \(%rcx\),%mm0
 116:	0f 38 1e c1[ 	]+pabsd  %mm1,%mm0
 11a:	66 0f 38 1e 01[ 	]+pabsd  \(%rcx\),%xmm0
 11f:	66 0f 38 1e c1[ 	]+pabsd  %xmm1,%xmm0
#pass
