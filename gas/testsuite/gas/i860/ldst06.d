#as:
#objdump: -dr
#name: i860 ldst06 (st.b)

.*: +file format .*

Disassembly of section \.text:

00000000 <\.text>:
   0:	00 00 00 0c 	st.b	%r0,0\(%r0\)
   4:	01 f8 20 0c 	st.b	%r31,1\(%r1\)
   8:	02 f0 40 0c 	st.b	%r30,2\(%sp\)
   c:	01 ea 60 0c 	st.b	%r29,513\(%fp\)
  10:	04 e4 80 0c 	st.b	%r28,1028\(%r4\)
  14:	fa df a1 0c 	st.b	%r27,4090\(%r5\)
  18:	fe d7 c3 0c 	st.b	%r26,8190\(%r6\)
  1c:	01 c8 e8 0c 	st.b	%r25,16385\(%r7\)
  20:	07 cd ef 0c 	st.b	%r25,32007\(%r7\)
  24:	ff cf ef 0c 	st.b	%r25,32767\(%r7\)
  28:	00 c8 f0 0c 	st.b	%r25,-32768\(%r7\)
  2c:	01 c8 f0 0c 	st.b	%r25,-32767\(%r7\)
  30:	01 c0 18 0d 	st.b	%r24,-16383\(%r8\)
  34:	5b b8 3c 0d 	st.b	%r23,-8101\(%r9\)
  38:	05 b0 5e 0d 	st.b	%r22,-4091\(%r10\)
  3c:	01 ac 7f 0d 	st.b	%r21,-1023\(%r11\)
  40:	03 a6 9f 0d 	st.b	%r20,-509\(%r12\)
  44:	e9 9f bf 0d 	st.b	%r19,-23\(%r13\)
  48:	ff 97 df 0d 	st.b	%r18,-1\(%r14\)
