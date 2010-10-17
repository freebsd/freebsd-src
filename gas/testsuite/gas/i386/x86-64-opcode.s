.text
	#				  Prefixes
	#			        O16 A32 OV REX  OPCODE				 ; NOTES

	# CALL
	CALLq *(%r8)		      # --  --  -- 41   FF 10				 ; REX to access upper reg.
	CALLq *(%rax)		      # --  --  -- --   FF 10
	CALLq *(%r8)		      # --  --  -- 41   FF 10				 ; REX to access upper reg.
	CALLq *(%rax)		      # --  --  -- --   FF 10

	# RET
	lret			      # --  --  -- --   CB
	retq			      # --  --  -- --   C3

	# IRET
	IRET			      # --  --  -- --   CF				 ; 32-bit operand size
	IRETW			      # 66  --  -- --   CF				 ; O16 for 16-bit operand size
	IRETQ			      # --  --  -- 48   CF				 ; REX for 64-bit operand size

	# CMP

	# MOV
	MOVw %cs,(%r8)		      # 66  --  -- 41   8C 08				 ; REX to access upper reg. O16 for 16-bit operand size
	MOVw %cs,(%rax)		      # 66  --  -- --   8C 08				 ; O16 for 16-bit operand size
	MOVw %ss,(%r8)		      # 66  --  -- 41   8C 10				 ; REX to access upper reg. O16 for 16-bit operand size
	MOVw %ss,(%rax)		      # 66  --  -- --   8C 10				 ; O16 for 16-bit operand size
	MOVw %fs,(%r8)		      # 66  --  -- 41   8C 20				 ; REX to access upper reg. O16 for 16-bit operand size
	MOVw %fs,(%rax)		      # 66  --  -- --   8C 20				 ; O16 for 16-bit operand size
	MOVl %cs,(%r8)		      # --  --  -- 41   8C 08				 ; REX to access upper reg.
	MOVl %cs,(%rax)		      # --  --  -- --   8C 08
	MOVl %ss,(%r8)		      # --  --  -- 41   8C 10				 ; REX to access upper reg.
	MOVl %ss,(%rax)		      # --  --  -- --   8C 10
	MOVl %fs,(%r8)		      # --  --  -- 41   8C 20				 ; REX to access upper reg.
	MOVl %fs,(%rax)		      # --  --  -- --   8C 20
	MOVl (%r8),%ss		      # --  --  -- 41   8E 10				 ; REX to access upper reg.
	MOVl (%rax),%ss		      # --  --  -- --   8E 10
	MOVl (%r8),%fs		      # --  --  -- 41   8E 20				 ; REX to access upper reg.
	MOVl (%rax),%fs		      # --  --  -- --   8E 20
	MOVb $0,(%r8)		      # --  --  -- 41   C6 00 00			 ; REX to access upper reg.
	MOVb $0,(%rax)		      # --  --  -- --   C6 00 00
	MOVw $0x7000,(%r8)	      # 66  --  -- 41   C7 00 00 70			 ; REX to access upper reg. O16 for 16-bit operand size
	MOVw $0x7000,(%rax)	      # 66  --  -- --   C7 00 00 70			 ; O16 for 16-bit operand size
	MOVl $0x70000000,(%r8)	      # --  --  -- 41   C7 00 00 00 00 70		 ; REX to access upper reg.
	MOVl $0x70000000,(%rax)	      # --  --  -- --   C7 00 00 00 00 70
	MOVb $0,(%r8)		      # --  --  -- 41   C6 00 00			 ; REX to access upper reg.
	MOVb $0,(%rax)		      # --  --  -- --   C6 00 00
	MOVw $0x7000,(%r8)	      # 66  --  -- --   41 C7 00 00 70			 ; O16 for 16-bit operand size
	MOVw $0x7000,(%rax)	      # 66  --  -- --   C7 00 00 70			 ; O16 for 16-bit operand size
	MOVl $0x70000000,(%rax)	      # --  --  -- --   C7 00 00 00 00 70
	MOVb $0,(%r8)		      # --  --  -- 41   C6 00 00			 ; REX to access upper reg.
	MOVb $0,(%rax)		      # --  --  -- --   C6 00 00
	MOVw $0x7000,(%r8)	      # 66  --  -- 41   C7 00 00 70			 ; REX to access upper reg. O16 for 16-bit operand size
	MOVw $0x7000,(%rax)	      # 66  --  -- --   C7 00 00 70			 ; O16 for 16-bit operand size
	MOVl $0x70000000,(%r8)	      # --  --  -- 41   C7 00 00 00 00 70		 ; REX to access upper reg.
	MOVl $0x70000000,(%rax)	      # --  --  -- --   C7 00 00 00 00 70
	MOVq $0x70000000,(%r8)	      # --  --  -- 49   C7 00 00 00 00 70		 ; REX for 64-bit operand size. REX to access upper reg.
	MOVq $0x70000000,(%rax)	      # --  --  -- 48   C7 00 00 00 00 70		 ; REX for 64-bit operand size

	# MOVNTI
	MOVNTI %eax,(%r8)	      # --  --  -- 41   0f c3 00			 ; REX to access upper reg.
	MOVNTI %eax,(%rax)	      # --  --  -- --   0f c3 00
	MOVNTI %rax,(%r8)	      # --  --  -- 49   0F C3 00			 ; REX to access upper reg. REX for 64-bit operand size
	MOVNTI %rax,(%rax)	      # --  --  -- 48   0F C3 00			 ; REX for 64-bit operand size. REX to access upper reg.
	MOVNTI %r8,(%r8)	      # --  --  -- 4D   0F C3 00			 ; REX to access upper reg. REX for 64-bit operand size
	MOVNTI %r8,(%rax)	      # --  --  -- 4C   0F C3 00			 ; REX to access upper reg. REX for 64-bit operand size

	# Conditionals

	# LOOP


	# Jcc
				      #	 66  --	 -- --	 77 FD				 ; A16 override: (Addr64) = ZEXT(Addr16)
				      #	 66  --	 -- --	 0F 87 F9 FF FF FF		 ; A16 override: (Addr64) = ZEXT(Addr16)

	# J*CXZ
				      #	 66  67	 -- --	 E3 FC				 ; ECX used as counter. A16 override: (Addr64) = ZEXT(Addr16)
				      #	 66  --	 -- --	 E3 FD				 ; A16 override: (Addr64) = ZEXT(Addr16)



	# Integer

	# IDIV

	IDIVb (%r8)		      #	 --  --	 -- 41	 F6 38				 ; Sign extended result. REX to access upper reg.
	IDIVb (%rax)		      #	 --  --	 -- --	 F6 38				 ; Sign extended result
	IDIVw (%r8)		      #	 66  --	 -- 41	 F7 38				 ; Sign extended result. REX to access upper reg. O16 for 16-bit
	IDIVw (%rax)		      #	 66  --	 -- --	 F7 38				 ; Sign extended result. O16 for 16-bit operand size
	IDIVl (%r8)		      #	 --  --	 -- 41	 F7 38				 ; Sign extended result. REX to access upper reg
	IDIVl (%rax)		      #	 --  --	 -- --	 F7 38				 ; Sign extended result
	IDIVq (%r8)		      #	 --  --	 -- 49	 F7 38				 ; Sign extended result. REX for 64-bit operand size. REX to access u
	IDIVq (%rax)		      #	 --  --	 -- 48	 F7 38				 ; Sign extended result. REX for 64-bit operand size

	# IMUL
	IMULb (%r8)		      #	 --  --	 -- 41	 F6 28				 ; Sign extended result. REX to access upper reg
	IMULb (%rax)		      #	 --  --	 -- --	 F6 28				 ; Sign extended result
	IMULw (%r8)		      #	 66  --	 -- 41	 F7 28				 ; Sign extended result. O16 for 16-bit operand size. REX to access
	IMULw (%rax)		      #	 66  --	 -- --	 F7 28				 ; Sign extended result. O16 for 16-bit operand size
	IMULl (%r8)		      #	 --  --	 -- 41	 F7 28				 ; Sign extended result. REX to access upper reg
	IMULl (%rax)		      #	 --  --	 -- --	 F7 28				 ; Sign extended result
	IMULq (%r8)		      #	 --  --	 -- 49	 F7 28				 ; Sign extended result. REX for 64-bit operand size. REX to access u
	IMULq (%rax)		      #	 --  --	 -- 48	 F7 28				 ; Sign extended result. REX for 64-bit operand size



	# SIMD/SSE

	# ADDPD
	ADDPD  (%r8),%xmm0	      #	 --  --	 66 41	 0F 58 00			 ; REX to access upper reg. OVR 128bit MMinstr.
	ADDPD  (%rax),%xmm0	      #	 --  --	 66 --	 0F 58 00			 ; OVR 128bit MMinstr.
	ADDPD  (%r8),%xmm15	      #	 --  --	 66 45	 0F 58 38			 ; REX to access upper XMM reg. REX to access upper reg. OVR 128bit MMinstr.
	ADDPD  (%rax),%xmm15	      #	 --  --	 66 44	 0F 58 38			 ; REX to access upper XMM reg. OVR 128bit MMinstr.
	ADDPD  (%r8),%xmm8	      #	 --  --	 66 45	 0F 58 00			 ; REX to access upper XMM reg. REX to access upper reg. OVR 128bit MMinstr.
	ADDPD  (%rax),%xmm8	      #	 --  --	 66 44	 0F 58 00			 ; REX to access upper XMM reg. OVR 128bit MMinstr.
	ADDPD  (%r8),%xmm7	      #	 --  --	 66 41	 0F 58 38			 ; REX to access upper reg. OVR 128bit MMinstr.
	ADDPD  (%rax),%xmm7	      #	 --  --	 66 --	 0F 58 38			 ; OVR 128bit MMinstr.
	ADDPD  %xmm0,%xmm0	      #	 --  --	 66 --	 0F 58 C0			 ; OVR 128bit MMinstr.
	ADDPD  %xmm15,%xmm15	      #	 --  --	 66 45	 0F 58 FF			 ; REX to access upper XMM reg. OVR 128bit MMinstr.
	ADDPD  %xmm15,%xmm8	      #	 --  --	 66 45	 0F 58 C7			 ; REX to access upper XMM reg. OVR 128bit MMinstr.

	# CMPPD

        # CVTSD2SI
	CVTSD2SIq (%r8),%rax	      #	 --  --	 F2 49	 0f 2d 00	                 ; OVR 128-bit media instruction override REX for 64-bit operand size                  REX to access upper reg.
	CVTSD2SIq (%rax),%rax	      #	 --  --	 F2 48	 0f 2d 00	                 ; OVR 128-bit media instruction override REX for 64-bit operand size
	CVTSD2SIq (%r8),%r8	      #	 --  --	 F2 4D	 0f 2d 00	                 ; OVR 128-bit media instruction override REX for 64-bit operand size                REX to access upper reg.
	CVTSD2SIq (%rax),%r8	      #	 --  --	 F2 4C	 0f 2d 00	                 ; OVR 128-bit media instruction override REX for 64-bit operand size                  REX to access upper reg.
	CVTSD2SIq %xmm0,%rax	      #	 --  --	 F2 48	 0f 2d c0	                 ; OVR 128-bit media instruction override REX for 64-bit operand size
	CVTSD2SIq %xmm15,%r8	      #	 --  --	 F2 4D	 0f 2d c7	                 ; OVR 128-bit media instruction override REX for 64-bit operand size                 REX to access upper XMM reg             REX to access upper reg.
	CVTSD2SIq %xmm15,%rax	      #	 --  --	 F2 49	 0f 2d c7	                 ; OVR 128-bit media instruction override REX for 64-bit operand size                  REX to access upper XMM reg
	CVTSD2SIq %xmm8,%r8	      #	 --  --	 F2 4D	 0f 2d c0	                 ; OVR 128-bit media instruction override REX for 64-bit operand size                  REX to access upper XMM reg             REX to access upper reg.
	CVTSD2SIq %xmm8,%rax	      #	 --  --	 F2 49	 0f 2d c0	                 ; OVR 128-bit media instruction override REX for 64-bit operand size                  REX to access upper XMM reg
	CVTSD2SIq %xmm7,%r8	      #	 --  --	 F2 4C	 0f 2d c7	                 ; OVR 128-bit media instruction override REX for 64-bit operand size                 REX to access upper reg.
	CVTSD2SIq %xmm7,%rax	      #	 --  --	 F2 48	 0f 2d c7	                 ; OVR 128-bit media instruction override REX for 64-bit operand size
	CVTSD2SIq %xmm0,%r8	      #	 --  --	 F2 4C	 0f 2d c0	                 ; OVR 128-bit media instruction override REX for 64-bit operand size                  REX to access upper reg.

        # CVTTSD2SI
	CVTTSD2SIq (%r8),%rax	      #	 --  --	 F2 49	 0f 2c 00	                 ; OVR 128-bit media instruction override REX for 64-bit operand size                REX to access upper reg.
	CVTTSD2SIq (%rax),%rax	      #	 --  --	 F2 48	 0f 2c 00	                 ; OVR 128-bit media instruction override REX for 64-bit operand size
	CVTTSD2SIq (%r8),%r8	      #	 --  --	 F2 4D	 0f 2c 00	                 ; OVR 128-bit media instruction override REX for 64-bit operand size                  REX to access upper reg.
	CVTTSD2SIq (%rax),%r8	      #	 --  --	 F2 4C	 0f 2c 00	                 ; OVR 128-bit media instruction override REX for 64-bit operand size                   REX to access upper reg.
	CVTTSD2SIq %xmm0,%rax	      #	 --  --	 F2 48	 0f 2c c0	                 ; OVR 128-bit media instruction override REX for 64-bit operand size
	CVTTSD2SIq %xmm15,%r8	      #	 --  --	 F2 4D	 0f 2c c7	                 ; OVR 128-bit media instruction override REX for 64-bit operand size                 REX to access upper XMM reg             REX to access upper reg.
	CVTTSD2SIq %xmm15,%rax	      #	 --  --	 F2 49	 0f 2c c7	                 ; OVR 128-bit media instruction override REX for 64-bit operand size                 REX to access upper XMM reg
	CVTTSD2SIq %xmm8,%r8	      #	 --  --	 F2 4D	 0f 2c c0	                 ; OVR 128-bit media instruction override REX for 64-bit operand size                 REX to access upper XMM reg             REX to access upper reg.
	CVTTSD2SIq %xmm8,%rax	      #	 --  --	 F2 49	 0f 2c c0	                 ; OVR 128-bit media instruction override REX for 64-bit operand size                 REX to access upper XMM reg
	CVTTSD2SIq %xmm7,%r8	      #	 --  --	 F2 4C	 0f 2c c7	                 ; OVR 128-bit media instruction override REX for 64-bit operand size                  REX to access upper reg.
	CVTTSD2SIq %xmm7,%rax	      #	 --  --	 F2 48	 0f 2c c7	                 ; OVR 128-bit media instruction override REX for 64-bit operand size
	CVTTSD2SIq %xmm0,%r8	      #	 --  --	 F2 4C	 0f 2c c0	                 ; OVR 128-bit media instruction override REX for 64-bit operand size                 REX to access upper reg.

        # CVTSS2SI
	CVTSS2SIq (%r8),%rax	      #	 --  --	 F3 49	 0f 2d 00	                 ; OVR 128-bit media instruction override Result is sign extended                         REX for 64-bit operand size                  REX to access upper reg.
	CVTSS2SIq (%rax),%rax	      #	 --  --	 F3 48	 0f 2d 00	                 ; OVR 128-bit media instruction override Result is sign extended                       REX for 64-bit operand size
	CVTSS2SIq (%r8),%r8	      #	 --  --	 F3 4D	 0f 2d 00	                 ; OVR 128-bit media instruction override Result is sign extended                        REX for 64-bit operand size                  REX to access upper reg.
	CVTSS2SIq (%rax),%r8	      #	 --  --	 F3 4C	 0f 2d 00	                 ; OVR 128-bit media instruction override Result is sign extended                         REX for 64-bit operand size                 REX to access upper reg.
	CVTSS2SIq %xmm0,%rax	      #	 --  --	 F3 48	 0f 2d c0	                 ; OVR 128-bit media instruction override Result is sign extended                       REX for 64-bit operand size
	CVTSS2SIq %xmm15,%r8	      #	 --  --	 F3 4D	 0f 2d c7	                 ; OVR 128-bit media instruction override Result is sign extended                       REX to access upper XMM reg            REX to access upper reg.
	CVTSS2SIq %xmm15,%rax	      #	 --  --	 F3 49	 0f 2d c7	                 ; OVR 128-bit media instruction override Result is sign extended                       REX for 64-bit operand size                 REX to access upper XMM reg
	CVTSS2SIq %xmm8,%r8	      #	 --  --	 F3 4D	 0f 2d c0	                 ; OVR 128-bit media instruction override Result is sign extended                          REX for 64-bit operand size                  REX to access upper XMM reg              REX to access upper reg.
	CVTSS2SIq %xmm8,%rax	      #	 --  --	 F3 49	 0f 2d c0	                 ; OVR 128-bit media instruction override Result is sign extended                          REX for 64-bit operand size
	CVTSS2SIq %xmm7,%r8	      #	 --  --	 F3 4C	 0f 2d c7	                 ; OVR 128-bit media instruction override Result is sign extended                       REX for 64-bit operand size                 REX to access upper reg.
	CVTSS2SIq %xmm7,%rax	      #	 --  --	 F3 48	 0f 2d c7	                 ; OVR 128-bit media instruction override Result is sign extended                          REX for 64-bit operand size
	CVTSS2SIq %xmm0,%r8	      #	 --  --	 F3 4C	 0f 2d c0	                 ; OVR 128-bit media instruction override Result is sign extended                       REX for 64-bit operand size                REX to access upper reg.

        # CVTTSS2SI
	CVTTSS2SIq (%r8),%rax	      #	 --  --	 F3 49	 0f 2c 00	                 ; OVR 128-bit media instruction override Result is sign extended                       REX for 64-bit operand size                  REX to access upper reg.
	CVTTSS2SIq (%rax),%rax	      #	 --  --	 F3 48	 0f 2c 00	                 ; OVR 128-bit media instruction override Result is sign extended                        REX for 64-bit operand size
	CVTTSS2SIq (%r8),%r8	      #	 --  --	 F3 4D	 0f 2c 00	                 ; OVR 128-bit media instruction override Result is sign extended                        REX for 64-bit operand size                   REX to access upper reg.
	CVTTSS2SIq (%rax),%r8	      #	 --  --	 F3 4C	 0f 2c 00	                 ; OVR 128-bit media instruction override Result is sign extended                       REX for 64-bit operand size                    REX to access upper reg.
	CVTTSS2SIq %xmm0,%rax	      #	 --  --	 F3 48	 0f 2c c0	                 ; OVR 128-bit media instruction override Result is sign extended                        REX for 64-bit operand size
	CVTTSS2SIq %xmm15,%r8	      #	 --  --	 F3 4D	 0f 2c c7	                 ; OVR 128-bit media instruction override Result is sign extended                       REX for 64-bit operand size                 REX to access upper XMM reg            REX to access upper reg.
	CVTTSS2SIq %xmm15,%rax	      #	 --  --	 F3 49	 0f 2c c7	                 ; OVR 128-bit media instruction override Result is sign extended                        REX for 64-bit operand size                 REX to access upper XMM reg
	CVTTSS2SIq %xmm8,%r8	      #	 --  --	 F3 4D	 0f 2c c0	                 ; OVR 128-bit media instruction override Result is sign extended                          REX for 64-bit operand size                  REX to access upper XMM reg               REX to access upper reg.
	CVTTSS2SIq %xmm8,%rax	      #	 --  --	 F3 49	 0f 2c c0	                 ; OVR 128-bit media instruction override Result is sign extended                        REX for 64-bit operand size
	CVTTSS2SIq %xmm7,%r8	      #	 --  --	 F3 4C	 0f 2c c7	                 ; OVR 128-bit media instruction override Result is sign extended                        REX for 64-bit operand size                 REX to access upper reg.
	CVTTSS2SIq %xmm7,%rax	      #	 --  --	 F3 48	 0f 2c c7	                 ; OVR 128-bit media instruction override Result is sign extended
	CVTTSS2SIq %xmm0,%r8	      #	 --  --	 F3 4C	 0f 2c c0	                 ; OVR 128-bit media instruction override Result is sign extended                          REX for 64-bit operand size                 REX to access upper reg.

        # CVTSI2SS
	CVTSI2SS  (%r8),%xmm0	      #	 --  --	 F3 41	 0f 2a 00	                 ; OVR 128-bit media instruction override REX to access upper reg.
	CVTSI2SS  (%rax),%xmm0	      #	 --  --	 F3 --	 0f 2a 00	 ; OVR 128-bit media instruction override
	CVTSI2SS  (%r8),%xmm15	      #	 --  --	 F3 45	 0f 2a 38	                 ; OVR 128-bit media instruction override REX to access upper XMM reg            REX to access upper reg.
	CVTSI2SS  (%rax),%xmm15	      #	 --  --	 F3 44	 0f 2a 38	                 ; OVR 128-bit media instruction override REX to access upper XMM reg
	CVTSI2SS  (%r8),%xmm8	      #	 --  --	 F3 45	 0f 2a 00	                 ; OVR 128-bit media instruction override REX to access upper XMM reg            REX to access upper reg.
	CVTSI2SS  (%rax),%xmm8	      #	 --  --	 F3 44	 0f 2a 00	                 ; OVR 128-bit media instruction override REX to access upper XMM reg
	CVTSI2SS  (%r8),%xmm7	      #	 --  --	 F3 41	 0f 2a 38	                 ; OVR 128-bit media instruction override REX to access upper reg.
	CVTSI2SS  (%rax),%xmm7	      #	 --  --	 F3 --	 0f 2a 38	                 ; OVR 128-bit media instruction override
	CVTSI2SS  %eax,%xmm0	      #	 --  --	 F3 --	 0f 2a c0	                 ; OVR 128-bit media instruction override
	CVTSI2SS  %eax,%xmm15	      #	 --  --	 F3 44	 0f 2a f8	                 ; OVR 128-bit media instruction override REX to access upper XMM reg
	CVTSI2SS  %eax,%xmm8	      #	 --  --	 F3 44	 0f 2a c0	                 ; OVR 128-bit media instruction override REX to access upper XMM reg
	CVTSI2SS  %eax,%xmm7	      #	 --  --	 F3 --	 0f 2a f8	                 ; OVR 128-bit media instruction override
	CVTSI2SS  (%r8),%xmm0	      #	 --  --	 F3 41	 0f 2a 00	                 ; OVR 128-bit media instruction override REX to access upper reg.
	CVTSI2SS  (%rax),%xmm0	      #	 --  --	 F3 --	 0f 2a 00	                 ; OVR 128-bit media instruction override
	CVTSI2SS  (%r8),%xmm15	      #	 --  --	 F3 45	 0f 2a 38	                 ; OVR 128-bit media instruction override REX to access upper XMM reg            REX to access upper reg.
	CVTSI2SS  (%rax),%xmm15	      #	 --  --	 F3 44	 0f 2a 38	                 ; OVR 128-bit media instruction override REX to access upper XMM reg
	CVTSI2SS  (%r8),%xmm8	      #	 --  --	 F3 45	 0f 2a 00	                 ; OVR 128-bit media instruction override REX to access upper XMM reg            REX to access upper reg.
	CVTSI2SS  (%rax),%xmm8	      #	 --  --	 F3 44	 0f 2a 00	                 ; OVR 128-bit media instruction override REX to access upper XMM reg
	CVTSI2SS  (%r8),%xmm7	      #	 --  --	 F3 41	 0f 2a 38	                 ; OVR 128-bit media instruction override REX to access upper reg.
	CVTSI2SS  (%rax),%xmm7	      #	 --  --	 F3 --	 0f 2a 38	                 ; OVR 128-bit media instruction override

        # CVTSI2SD
	CVTSI2SD  (%r8),%xmm0	      #	 --  --	 F2 41	 0F 2A 00			 ; REX to access upper reg. OVR 128bit MMinstr.
	CVTSI2SD  (%rax),%xmm0	      #	 --  --	 F2 --	 0F 2A 00			 ; OVR 128bit MMinstr.
	CVTSI2SD  (%r8),%xmm15	      #	 --  --	 F2 45	 0F 2A 38			 ; REX to access upper XMM reg. REX to access upper reg. OVR 128bit MMinstr.
	CVTSI2SD  (%rax),%xmm15	      #	 --  --	 F2 44	 0F 2A 38			 ; REX to access upper XMM reg. OVR 128bit MMinstr.
	CVTSI2SD  (%r8),%xmm8	      #	 --  --	 F2 45	 0F 2A 00			 ; REX to access upper XMM reg. REX to access upper reg. OVR 128bit MMinstr.
	CVTSI2SD  (%rax),%xmm8	      #	 --  --	 F2 44	 0F 2A 00			 ; REX to access upper XMM reg. OVR 128bit MMinstr.
	CVTSI2SD  (%r8),%xmm7	      #	 --  --	 F2 41	 0F 2A 38			 ; REX to access upper reg. OVR 128bit MMinstr.
	CVTSI2SD  (%rax),%xmm7	      #	 --  --	 F2 --	 0F 2A 38			 ; OVR 128bit MMinstr.
	CVTSI2SD  %eax,%xmm0	      #	 --  --	 F2 --	 0F 2A C0			 ; OVR 128bit MMinstr.
	CVTSI2SD  %eax,%xmm15	      #	 --  --	 F2 44	 0F 2A F8			 ; REX to access upper XMM reg. OVR 128bit MMinstr.
	CVTSI2SD  %eax,%xmm8	      #	 --  --	 F2 44	 0F 2A C0			 ; REX to access upper XMM reg. OVR 128bit MMinstr.
	CVTSI2SD  %eax,%xmm7	      #	 --  --	 F2 --	 0F 2A F8			 ; OVR 128bit MMinstr.
	CVTSI2SD  (%r8),%xmm0	      #	 --  --	 F2 41	 0F 2A 00			 ; REX to access upper reg. OVR 128bit MMinstr.
	CVTSI2SD  (%rax),%xmm0	      #	 --  --	 F2 --	 0F 2A 00			 ; OVR 128bit MMinstr.
	CVTSI2SD  (%r8),%xmm15	      #	 --  --	 F2 45	 0F 2A 38			 ; REX to access upper XMM reg. REX to access upper reg. OVR 128bit MMinstr.
	CVTSI2SD  (%rax),%xmm15	      #	 --  --	 F2 44	 0F 2A 38			 ; REX to access upper XMM reg. OVR 128bit MMinstr.
	CVTSI2SD  (%r8),%xmm8	      #	 --  --	 F2 45	 0F 2A 00			 ; REX to access upper XMM reg. REX to access upper reg. OVR 128bit MMinstr.
	CVTSI2SD  (%rax),%xmm8	      #	 --  --	 F2 44	 0F 2A 00			 ; REX to access upper XMM reg. OVR 128bit MMinstr.
	CVTSI2SD  (%r8),%xmm7	      #	 --  --	 F2 41	 0F 2A 38			 ; REX to access upper reg. OVR 128bit MMinstr.
	CVTSI2SD  (%rax),%xmm7	      #	 --  --	 F2 --	 0F 2A 38			 ; OVR 128bit MMinstr.

	# MOVD
	MOVD (%r8),%xmm0	      #	 --  --	 66 41	 0F 6E 00			 ; REX to access upper reg. Data128 = ZEXT(Data32). OVR 128bit MMinstr.
	MOVD (%rax),%xmm0	      #	 --  --	 66 --	 0F 6E 00			 ; Data128 = ZEXT(Data32). OVR 128bit MMinstr.
	MOVD (%r8),%xmm15	      #	 --  --	 66 45	 0F 6E 38			 ; REX to access upper XMM reg. REX to access upper reg. Data128 = ZEXT(Data32)
	MOVD (%rax),%xmm15	      #	 --  --	 66 44	 0F 6E 38			 ; REX to access upper XMM reg. OVR 128bit MMinstr.
	MOVD (%r8),%xmm8	      #	 --  --	 66 45	 0F 6E 00			 ; REX to access upper XMM reg. REX to access upper reg. Data128 = ZEXT(Data32)
	MOVD (%rax),%xmm8	      #	 --  --	 66 44	 0F 6E 00			 ; REX to access upper XMM reg. Data128 = ZEXT(Data32). OVR 128bit MMinstr.
	MOVD (%r8),%xmm7	      #	 --  --	 66 41	 0F 6E 38			 ; REX to access upper reg. Data128 = ZEXT(Data32). OVR 128bit MMinstr.
	MOVD (%rax),%xmm7	      #	 --  --	 66 --	 0F 6E 38			 ; Data128 = ZEXT(Data32). OVR 128bit MMinstr.
	MOVD %eax,%xmm0		      #	 --  --	 66 --	 0F 6E C0			 ; Data128 = ZEXT(Data32). OVR 128bit MMinstr.
	MOVD %eax,%xmm15	      #	 --  --	 66 44	 0F 6E F8			 ; REX to access upper XMM reg. Data128 = ZEXT(Data32). OVR 128bit MMinstr.
	MOVD %eax,%xmm8		      #	 --  --	 66 44	 0F 6E C0			 ; REX to access upper XMM reg. Data128 = ZEXT(Data32). OVR 128bit MMinstr.
	MOVD %eax,%xmm7		      #	 --  --	 66 --	 0F 6E F8			 ; Data128 = ZEXT(Data32). OVR 128bit MMinstr.
	MOVD %xmm0,(%r8)	      #	 --  --	 66 41	 0F 7E 00			 ; REX to access upper reg. OVR 128bit MMinstr.
	MOVD %xmm0,(%rax)	      #	 --  --	 66 --	 0F 7E 00			 ; OVR 128bit MMinstr.
	MOVD %xmm15,(%r8)	      #	 --  --	 66 45	 0F 7E 38			 ; REX to access upper XMM reg. REX to access upper reg. OVR 128bit MMinstr.
	MOVD %xmm15,(%rax)	      #	 --  --	 66 44	 0F 7E 38			 ; REX to access upper XMM reg. OVR 128bit MMinstr.
	MOVD %xmm8,(%r8)	      #	 --  --	 66 45	 0F 7E 00			 ; REX to access upper XMM reg. REX to access upper reg. OVR 128bit MMinstr.
	MOVD %xmm8,(%rax)	      #	 --  --	 66 44	 0F 7E 00			 ; REX to access upper XMM reg. OVR 128bit MMinstr.
	MOVD %xmm7,(%r8)	      #	 --  --	 66 41	 0F 7E 38			 ; REX to access upper reg. OVR 128bit MMinstr.
	MOVD %xmm7,(%rax)	      #	 --  --	 66 --	 0F 7E 38			 ; OVR 128bit MMinstr.
	MOVD %xmm0,%eax		      #	 --  --	 66 --	 0F 7E C0			 ; OVR 128bit MMinstr.
	MOVD %xmm15,%eax	      #	 --  --	 66 44	 0F 7E F8			 ; REX to access upper XMM reg. OVR 128bit MMinstr.
	MOVD %xmm8,%eax		      #	 --  --	 66 44	 0F 7E C0			 ; REX to access upper XMM reg. OVR 128bit MMinstr.
	MOVD %xmm7,%eax		      #	 --  --	 66 --	 0F 7E F8			 ; OVR 128bit MMinstr.
	MOVD %rax,%xmm0		      #	 --  --	 66 48	 0F 6E C0			 ; Data128 = ZEXT(Data64). OVR 128bit MMinstr. REX for 64-bit operand size.
	MOVD %r8,%xmm0		      #	 --  --	 66 49	 0F 6E C0			 ; REX to access upper reg. Data128 = ZEXT(Data64). OVR 128bit MMinstr. REX for 64-bit operand size.
	MOVD %r8,%xmm15 	      #	 --  --	 66 4D	 0F 6E F8			 ; REX to access upper reg. Data128 = ZEXT(Data64). OVR 128bit MMinstr. REX for 64-bit operand size.
	MOVD %xmm0,%rax		      #	 --  --	 66 48	 0F 7E C0			 ; OVR 128bit MMinstr. REX for 64-bit operand size.
	MOVD %xmm0,%r8		      #	 --  --	 66 49	 0F 7E C0			 ; OVR 128bit MMinstr. REX for 64-bit operand size.
	MOVD %xmm7,%r8		      #	 --  --	 66 49	 0F 7E F8			 ; OVR 128bit MMinstr. REX for 64-bit operand size.

	# MOVQ
	MOVQ (%r8),%xmm0	      #	 --  --	 F3 41	 0F 7E 00			 ; REX to access upper reg. Data128 = ZEXT(Data64). OVR 128bit MMinstr.
	MOVQ (%rax),%xmm0	      #	 --  --	 F3 --	 0F 7E 00			 ; Data128 = ZEXT(Data64). OVR 128bit MMinstr.
	MOVQ (%r8),%xmm15	      #	 --  --	 F3 45	 0F 7E 38			 ; REX to access upper XMM reg. REX to access upper reg. Data128 = ZEXT(Data64)
	MOVQ (%rax),%xmm15	      #	 --  --	 F3 44	 0F 7E 38			 ; REX to access upper XMM reg. Data128 = ZEXT(Data64). OVR 128bit MMinstr.
	MOVQ (%r8),%xmm8	      #	 --  --	 F3 45	 0F 7E 00			 ; REX to access upper XMM reg. REX to access upper reg. Data128 = ZEXT(Data64)
	MOVQ (%rax),%xmm8	      #	 --  --	 F3 44	 0F 7E 00			 ; REX to access upper XMM reg. Data128 = ZEXT(Data64). OVR 128bit MMinstr.
	MOVQ (%r8),%xmm7	      #	 --  --	 F3 41	 0F 7E 38			 ; REX to access upper reg. Data128 = ZEXT(Data64). OVR 128bit MMinstr.
	MOVQ (%rax),%xmm7	      #	 --  --	 F3 --	 0F 7E 38			 ; Data128 = ZEXT(Data64). OVR 128bit MMinstr.
	MOVQ %xmm0,%xmm0	      #	 --  --	 F3 --	 0F 7E C0			 ; OVR 128bit MMinstr.
	MOVQ %xmm15,%xmm15	      #	 --  --	 F3 45	 0F 7E FF			 ; REX to access upper XMM reg. OVR 128bit MMinstr.
	MOVQ %xmm15,%xmm8	      #	 --  --	 F3 45	 0F 7E C7			 ; REX to access upper XMM reg. OVR 128bit MMinstr.
	MOVQ %xmm15,%xmm7	      #	 --  --	 F3 41	 0F 7E FF			 ; REX to access upper XMM reg. OVR 128bit MMinstr.
	MOVQ %xmm15,%xmm0	      #	 --  --	 F3 41	 0F 7E C7			 ; REX to access upper XMM reg. OVR 128bit MMinstr.
	MOVQ %xmm8,%xmm15	      #	 --  --	 F3 45	 0F 7E F8			 ; REX to access upper XMM reg. OVR 128bit MMinstr.
	MOVQ %xmm8,%xmm8	      #	 --  --	 F3 45	 0F 7E C0			 ; REX to access upper XMM reg. OVR 128bit MMinstr.
	MOVQ %xmm8,%xmm7	      #	 --  --	 F3 41	 0F 7E F8			 ; REX to access upper XMM reg. OVR 128bit MMinstr.
	MOVQ %xmm8,%xmm0	      #	 --  --	 F3 41	 0F 7E C0			 ; REX to access upper XMM reg. OVR 128bit MMinstr.
	MOVQ %xmm7,%xmm15	      #	 --  --	 F3 44	 0F 7E FF			 ; REX to access upper XMM reg. OVR 128bit MMinstr.
	MOVQ %xmm7,%xmm8	      #	 --  --	 F3 44	 0F 7E C7			 ; REX to access upper XMM reg. OVR 128bit MMinstr.
	MOVQ %xmm7,%xmm7	      #	 --  --	 F3 --	 0F 7E FF			 ; OVR 128bit MMinstr.
	MOVQ %xmm7,%xmm0	      #	 --  --	 F3 --	 0F 7E C7			 ; OVR 128bit MMinstr.
	MOVQ %xmm0,%xmm15	      #	 --  --	 F3 44	 0F 7E F8			 ; REX to access upper XMM reg. OVR 128bit MMinstr.
	MOVQ %xmm0,%xmm8	      #	 --  --	 F3 44	 0F 7E C0			 ; REX to access upper XMM reg. OVR 128bit MMinstr.
	MOVQ %xmm0,%xmm7	      #	 --  --	 F3 --	 0F 7E F8			 ; OVR 128bit MMinstr.
	MOVQ %xmm0,(%r8)	      #	 --  --	 66 41	 0F D6 00			 ; REX to access upper reg. OVR 128bit MMinstr.
	MOVQ %xmm0,(%rax)	      #	 --  --	 66 --	 0F D6 00			 ; OVR 128bit MMinstr.
	MOVQ %xmm15,(%r8)	      #	 --  --	 66 45	 0F D6 38			 ; REX to access upper reg. OVR 128bit MMinstr.
	MOVQ %xmm15,(%rax)	      #	 --  --	 66 44	 0F D6 38			 ; REX to access upper XMM reg. OVR 128bit MMinstr.
	MOVQ %xmm8,(%r8)	      #	 --  --	 66 45	 0F D6 00			 ; REX to access upper XMM reg. REX to access upper reg. OVR 128bit MMinstr.
	MOVQ %xmm8,(%rax)	      #	 --  --	 66 44	 0F D6 00			 ; REX to access upper XMM reg. OVR 128bit MMinstr.
	MOVQ %xmm7,(%r8)	      #	 --  --	 66 41	 0F D6 38			 ; REX to access upper reg. OVR 128bit MMinstr.

	# 64-bit MMX

	# CVTPD2PI

	# MOVD
	MOVD (%r8),%mm0		      #	 --  --	 -- 41	 0F 6E 00			 ; REX to access upper reg. Data64 = ZEXT(Data32)
	MOVD (%rax),%mm0	      #	 --  --	 -- --	 0F 6E 00			 ; Data64 = ZEXT(Data32)
	MOVD (%r8),%mm7		      #	 --  --	 -- 41	 0F 6E 38			 ; REX to access upper reg. Data64 = ZEXT(Data32)
	MOVD (%rax),%mm7	      #	 --  --	 -- --	 0F 6E 38			 ; Data64 = ZEXT(Data32)
	MOVD %eax,%mm0		      #	 --  --	 -- --	 0F 6E C0			 ; Data64 = ZEXT(Data32)
	MOVD %eax,%mm7		      #	 --  --	 -- --	 0F 6E F8			 ; Data64 = ZEXT(Data32)
	MOVD %mm0,(%r8)		      #	 --  --	 -- 41	 0F 7E 00			 ; REX to access upper reg.
	MOVD %mm0,(%rax)	      #	 --  --	 -- --	 0F 7E 00
	MOVD %mm7,(%r8)		      #	 --  --	 -- 41	 0F 7E 38			 ; REX to access upper reg.
	MOVD %mm7,(%rax)	      #	 --  --	 -- --	 0F 7E 38
	MOVD %mm0,%eax		      #	 --  --	 -- --	 0F 7E C0
	MOVD %mm7,%eax		      #	 --  --	 -- --	 0F 7E F8

	# MOVQ
	MOVQ (%r8),%mm0		      #	 --  --	 -- 41	 0F 6F 00			 ; REX to access upper reg.
	MOVQ (%rax),%mm0	      #	 --  --	 -- --	 0F 6F 00
	MOVQ (%r8),%mm7		      #	 --  --	 -- 41	 0F 6F 38			 ; REX to access upper reg.
	MOVQ (%rax),%mm7	      #	 --  --	 -- --	 0F 6F 38
	MOVQ %mm0,(%r8)		      #	 --  --	 -- 41	 0F 7F 00			 ; REX to access upper reg.
	MOVQ %mm0,(%rax)	      #	 --  --	 -- --	 0F 7F 00
	MOVQ %mm7,(%r8)		      #	 --  --	 -- 41	 0F 7F 38			 ; REX to access upper reg.
	MOVQ %mm7,(%rax)	      #	 --  --	 -- --	 0F 7F 38

	# X87
	# FADDP


	# FDIV

	# Stack Operations

	# POP
	POPq (%r8)		      #	 --  --	 -- 41	 8F 00				 ; REX to access upper reg.
	POPq (%rax)		      #	 --  --	 -- --	 8F 00
	POPFQ			      #	 --  --	 -- --	 9D

	# PUSH
	PUSHq (%r8)		      #	 --  --	 -- 41	 FF 30				 ; REX to access upper reg.
	PUSHq (%rax)		      #	 --  --	 -- --	 FF 30
	PUSHFQ			      #	 --  --	 -- --	 9C





	# MMX/XMM/x87 State
	# FNSAVE
	# FRSTOR
	# FSAVE
	# FXRSTOR
	# FXSAVE
	# EMMS
	EMMS			      #	 --  --	 -- --	 0F 77
	# FEMMS
	FEMMS			      #	 --  --	 -- --	 0F 0E

	# LEA calculation

	# MISC System Instructions
	# CLFLUSH

	# INVD
	INVD			      #	 --  --	 -- --	 0F 08

	# INVLPG
	INVLPG (%r8)		      #	 --  --	 -- 41	 0F 01 38			 ; REX to access upper reg.
	INVLPG (%rax)		      #	 --  --	 -- --	 0F 01 38
	INVLPG (%r8)		      #	 --  --	 -- 41	 0F 01 38			 ; REX to access upper reg.
	INVLPG (%rax)		      #	 --  --	 -- --	 0F 01 38
	INVLPG (%r8)		      #	 --  --	 -- 41	 0F 01 38			 ; REX to access upper reg.
	INVLPG (%rax)		      #	 --  --	 -- --	 0F 01 38

	# LAR

	# LGDT

	# LIDT


	# LLDT

	# SGDT

	# SIDT

	# SLDT
#        SLDT (%eax)	              #  --  67	 -- --	 0F 00 00	                 ; A32 override: (Addr64) = ZEXT(Addr32 )
        SLDT %eax	              #  --  --	 -- --	 0F 00 C0

	# SWAPGS



	# IO

	# OUT
	OUT %al,$0		      #	 --  --	 -- --	 E6 00
	OUT %ax,$0		      #	 66  --	 -- --	 E7 00				 ; O16 for 16-bit operand size
	OUT %eax,$0		      #	 --  --	 -- --	 E7 00

	# IN

 .p2align 4,0
