#as:
#objdump: -dr
#name: i860 ldst05 (st.s)

.*: +file format .*

Disassembly of section \.text:

00000000 <\.text>:
   0:	00 00 00 1c 	st.s	%r0,0\(%r0\)
   4:	7a f8 20 1c 	st.s	%r31,122\(%r1\)
   8:	02 f1 40 1c 	st.s	%r30,258\(%sp\)
   c:	00 ea 60 1c 	st.s	%r29,512\(%fp\)
  10:	04 e4 80 1c 	st.s	%r28,1028\(%r4\)
  14:	fa df a1 1c 	st.s	%r27,4090\(%r5\)
  18:	fe d7 c3 1c 	st.s	%r26,8190\(%r6\)
  1c:	00 c8 e8 1c 	st.s	%r25,16384\(%r7\)
  20:	00 c0 18 1d 	st.s	%r24,-16384\(%r8\)
  24:	00 b8 3c 1d 	st.s	%r23,-8192\(%r9\)
  28:	00 b0 5e 1d 	st.s	%r22,-4096\(%r10\)
  2c:	00 ac 7f 1d 	st.s	%r21,-1024\(%r11\)
  30:	04 a6 9f 1d 	st.s	%r20,-508\(%r12\)
  34:	0e 9f bf 1d 	st.s	%r19,-242\(%r13\)
  38:	fe 97 df 1d 	st.s	%r18,-2\(%r14\)
