#as:
#objdump: -dr
#name: i860 ldst04 (st.l)

.*: +file format .*

Disassembly of section \.text:

00000000 <\.text>:
   0:	01 00 00 1c 	st.l	%r0,0\(%r0\)
   4:	7d f8 20 1c 	st.l	%r31,124\(%r1\)
   8:	01 f1 40 1c 	st.l	%r30,256\(%sp\)
   c:	01 ea 60 1c 	st.l	%r29,512\(%fp\)
  10:	01 e4 80 1c 	st.l	%r28,1024\(%r4\)
  14:	01 d8 a2 1c 	st.l	%r27,4096\(%r5\)
  18:	01 d0 c4 1c 	st.l	%r26,8192\(%r6\)
  1c:	01 c8 e8 1c 	st.l	%r25,16384\(%r7\)
  20:	01 c0 18 1d 	st.l	%r24,-16384\(%r8\)
  24:	01 b8 3c 1d 	st.l	%r23,-8192\(%r9\)
  28:	01 b0 5e 1d 	st.l	%r22,-4096\(%r10\)
  2c:	01 ac 7f 1d 	st.l	%r21,-1024\(%r11\)
  30:	05 a6 9f 1d 	st.l	%r20,-508\(%r12\)
  34:	09 9f bf 1d 	st.l	%r19,-248\(%r13\)
  38:	fd 97 df 1d 	st.l	%r18,-4\(%r14\)
