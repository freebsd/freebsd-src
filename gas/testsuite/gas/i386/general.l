.*: Assembler messages:
.*:7: Warning:.*
.*:9: Warning:.*
.*:11: Warning:.*
.*:13: Warning:.*
.*:15: Warning:.*
.*:21: Warning:.*
.*:23: Warning:.*
.*:25: Warning:.*
.*:27: Warning:.*
.*:29: Warning:.*
.*:39: Warning:.*
.*:41: Warning:.*
.*:48: Warning:.*
.*:51: Warning:.*
.*:124: Warning:.*
.*:125: Warning:.*
.*:126: Warning:.*
.*:127: Warning:.*
.*:128: Warning:.*
.*:129: Warning:.*
.*:130: Warning:.*
.*:131: Warning:.*
.*:132: Warning:.*
.*:133: Warning:.*
.*:134: Warning:.*
.*:135: Warning:.*
.*:136: Warning:.*
.*:137: Warning:.*
.*:138: Warning:.*
.*:139: Warning:.*
.*:140: Warning:.*
.*:141: Warning:.*
.*:142: Warning:.*
.*:143: Warning:.*
.*:144: Warning:.*
.*:178: Warning:.*
   1                       	.psize 0
   2                       	.text
   3                       	#test jumps and calls
   4 0000 EBFE              	1:	jmp	1b
   5 0002 E9((FC|F9)FFFF FF|000000 00)       		jmp	xxx
   6 0007 FF250000 0000     		jmp	\*xxx
   7 000d FF250000 0000     		jmp	xxx\(,1\)
.*Warning:.*
   8 0013 FFE7              		jmp	\*%edi
   9 0015 FFE7              		jmp	%edi
.*Warning:.*
  10 0017 FF27              		jmp	\*\(%edi\)
  11 0019 FF27              		jmp	\(%edi\)
.*Warning:.*
  12 001b FF2CBD00 000000   		ljmp	\*xxx\(,%edi,4\)
  13 0022 FF2CBD00 000000   		ljmp	xxx\(,%edi,4\)
.*Warning:.*
  14 0029 FF2D0000 0000     		ljmp	\*xxx
  15 002f FF2D0000 0000     		ljmp	xxx\(,1\)
.*Warning:.*
  16 0035 EA000000 003412   		ljmp	\$0x1234,\$xxx
  17                       	
  18 003c E8BFFFFF FF       		call	1b
  19 0041 E8((FC|BA)FFFF FF|000000 00)       		call	xxx
  20 0046 FF150000 0000     		call	\*xxx
  21 004c FF150000 0000     		call	xxx\(,1\)
.*Warning:.*
  22 0052 FFD7              		call	\*%edi
  23 0054 FFD7              		call	%edi
.*Warning:.*
  24 0056 FF17              		call	\*\(%edi\)
  25 0058 FF17              		call	\(%edi\)
.*Warning:.*
  26 005a FF1CBD00 000000   		lcall	\*xxx\(,%edi,4\)
  27 0061 FF1CBD00 000000   		lcall	xxx\(,%edi,4\)
.*Warning:.*
  28 0068 FF1D0000 0000     		lcall	\*xxx
  29 006e FF1D0000 0000     		lcall	xxx\(,1\)
.*Warning:.*
  30 0074 9A000000 003412   		lcall	\$0x1234,\$xxx
  31                       	
  32                       	# test various segment reg insns
  33 007b 1E                		push	%ds
  34 007c 1E                		pushl	%ds
  35 007d 1F                		pop	%ds
  36 007e 1F                		popl	%ds
  37 007f 8CD8              		mov	%ds,%eax
  38 0081 8CD8              		movl	%ds,%eax
  39 0083 8CD8              		movl	%ds,%ax
.*Warning:.*
  40 0085 8ED8              		mov	%eax,%ds
  41 0087 8ED8              		movl	%ax,%ds
.*Warning:.*
  42 0089 8ED8              		movl	%eax,%ds
  43                       	
  44 008b 661E              		pushw	%ds
  45 008d 661F              		popw	%ds
  46 008f 668CD8            		mov	%ds,%ax
  47 0092 668CD8            		movw	%ds,%ax
  48 0095 668CD8            		movw	%ds,%eax
.*Warning:.*
  49 0098 8ED8              		mov	%ax,%ds
  50 009a 8ED8              		movw	%ax,%ds
  51 009c 8ED8              		movw	%eax,%ds
.*Warning:.*
  52                       	
  53                       	# test various pushes
  54 009e 6A0A              		pushl	\$10
  55 00a0 666A0A            		pushw	\$10
  56 00a3 6A0A              		push	\$10
  57 00a5 68E80300 00       		pushl	\$1000
  58 00aa 6668E803          		pushw	\$1000
  59 00ae 68E80300 00       		push	\$1000
  60 00b3 FF35D200 0000     		pushl	1f
  61 00b9 66FF35D2 000000   		pushw	1f
  62 00c0 FF35D200 0000     		push	1f
  63 00c6 FFB30C00 0000     		push	\(1f-.\)\(%ebx\)
  64 00cc FF350600 0000     		push	1f-.
  65                       	# these, and others like them should have no operand size prefix
  66 00d2 0F00D1            	1:	lldt	%cx
  67 00d5 0F01F0            		lmsw	%ax
  68                       	
  69                       	# Just to make sure these don't become illegal due to over-enthusiastic
  70                       	# register checking
  71 00d8 660FBEF8          		movsbw	%al,%di
  72 00dc 0FBEC8            		movsbl	%al,%ecx
  73 00df 0FBFC8            		movswl	%ax,%ecx
  74 00e2 660FB6F8          		movzbw	%al,%di
  75 00e6 0FB6C8            		movzbl	%al,%ecx
  76 00e9 0FB7C8            		movzwl	%ax,%ecx
  77                       	
  78 00ec EC                		in	%dx,%al
  79 00ed 66ED              		in	%dx,%ax
  80 00ef ED                		in	%dx,%eax
  81 00f0 EC                		in	\(%dx\),%al
  82 00f1 66ED              		in	\(%dx\),%ax
  83 00f3 ED                		in	\(%dx\),%eax
  84 00f4 EC                		inb	%dx,%al
  85 00f5 66ED              		inw	%dx,%ax
  86 00f7 ED                		inl	%dx,%eax
  87 00f8 EC                		inb	%dx
  88 00f9 66ED              		inw	%dx
  89 00fb ED                		inl	%dx
  90 00fc E4FF              		inb	\$255
  91 00fe 66E502            		inw	\$2
  92 0101 E504              		inl	\$4
  93 0103 66E50D            		in	\$13, %ax
  94 0106 EE                		out	%al,%dx
  95 0107 66EF              		out	%ax,%dx
  96 0109 EF                		out	%eax,%dx
  97 010a EE                		out	%al,\(%dx\)
  98 010b 66EF              		out	%ax,\(%dx\)
  99 010d EF                		out	%eax,\(%dx\)
 100 010e EE                		outb	%al,%dx
 101 010f 66EF              		outw	%ax,%dx
 102 0111 EF                		outl	%eax,%dx
 103 0112 EE                		outb	%dx
 104 0113 66EF              		outw	%dx
 105 0115 EF                		outl	%dx
 106 0116 E6FF              		outb	\$255
 107 0118 66E702            		outw	\$2
 108 011b E704              		outl	\$4
 109 011d 66E70D            		out	%ax, \$13
 110                       	# These are used in AIX.
 111 0120 66ED              		inw	\(%dx\)
 112 0122 66EF              		outw	\(%dx\)
 113                       	
 114 0124 A4                		movsb
 115 0125 66A7              		cmpsw
 116 0127 AF                		scasl
 117 0128 D7                		xlatb
 118 0129 2EA5              		movsl	%cs:\(%esi\),%es:\(%edi\)
 119 012b 0F9303            		setae	\(%ebx\)
 120 012e 0F9303            		setaeb	\(%ebx\)
 121 0131 0F93C0            		setae	%al
 122                       	
 123                       	#these should give warnings
 124 0134 0C01              		orb	\$1,%ax
.*Warning:.*
 125 0136 0C01              		orb	\$1,%eax
.*Warning:.*
 126 0138 80CB01            		orb	\$1,%bx
.*Warning:.*
 127 013b 80CB01            		orb	\$1,%ebx
.*Warning:.*
 128 013e D9C1              		fldl	%st\(1\)
.*Warning:.*
 129 0140 DDD2              		fstl	%st\(2\)
.*Warning:.*
 130 0142 DDDB              		fstpl	%st\(3\)
.*Warning:.*
 131 0144 D8D4              		fcoml	%st\(4\)
.*Warning:.*
 132 0146 D8DD              		fcompl	%st\(5\)
.*Warning:.*
 133 0148 DEC1              		faddp	%st\(1\),%st
.*Warning:.*
 134 014a DECA              		fmulp	%st\(2\),%st
.*Warning:.*
 135 014c DEE3              		fsubp	%st\(3\),%st
.*Warning:.*
 136 014e DEEC              		fsubrp	%st\(4\),%st
.*Warning:.*
 137 0150 DEF5              		fdivp	%st\(5\),%st
.*Warning:.*
 138 0152 DEFE              		fdivrp	%st\(6\),%st
.*Warning:.*
 139 0154 DEC1              		fadd
.*Warning:.*
 140 0156 DEE1              		fsub
.*Warning:.*
 141 0158 DEC9              		fmul
.*Warning:.*
 142 015a DEF1              		fdiv
.*Warning:.*
 143 015c DEE9              		fsubr
.*Warning:.*
 144 015e DEF9              		fdivr
.*Warning:.*
 145                       	#these should all be legal
 146 0160 0FA31556 341200   		btl	%edx, 0x123456
 147 0167 0FA3D0            		btl	%edx, %eax
 148 016a 0C01              		orb	\$1,%al
 149 016c 80CB01            		orb	\$1,%bl
 150 016f A1110000 00       		movl	17,%eax
 151 0174 A1110000 00       		mov	17,%eax
 152 0179 66ED              		inw	%dx,%ax
 153 017b ED                		inl	%dx,%eax
 154 017c 66ED              		inw	\(%dx\),%ax
 155 017e ED                		inl	\(%dx\),%eax
 156 017f EC                		in	\(%dx\),%al
 157 0180 66ED              		in	\(%dx\),%ax
 158 0182 ED                		in	\(%dx\),%eax
 159 0183 0FB61437          		movzbl	\(%edi,%esi\),%edx
 160 0187 0FB6451C          		movzbl	28\(%ebp\),%eax
 161 018b 0FB6C0            		movzbl	%al,%eax
 162 018e 0FB6F1            		movzbl	%cl,%esi
 163 0191 26D7              		xlat	%es:\(%ebx\)
 164 0193 D7                		xlat
 165 0194 D7                		xlatb
 166 0195 DDD8              	1:	fstp	%st\(0\)
 167 0197 E2FC              		loop	1b
 168 0199 F6F1              		divb    %cl 
 169 019b 66F7F1            		divw    %cx 
 170 019e F7F1              		divl    %ecx
 171 01a0 F6F1              		div	%cl
 172 01a2 66F7F1            		div	%cx
 173 01a5 F7F1              		div	%ecx
 174 01a7 F6F1              		div	%cl,%al
 175 01a9 66F7F1            		div	%cx,%ax
 176 01ac F7F1              		div	%ecx,%eax
 177 01ae 8EDE              		mov	%si,%ds
 178 01b0 8EDE              		movl	%si,%ds		# warning here
.*Warning:.*
 179 01b2 1E                		pushl	%ds
 180 01b3 1E                		push	%ds
 181 01b4 A0000000 00       		mov	0,%al
 182 01b9 66A10000 0100     		mov	0x10000,%ax
 183 01bf 89C3              		mov	%eax,%ebx
 184 01c1 9C                		pushf
 185 01c2 9C                		pushfl
 186 01c3 669C              		pushfw
 187 01c5 9D                		popf
 188 01c6 9D                		popfl
 189 01c7 669D              		popfw
 190 01c9 89341D00 000000   		mov	%esi,\(,%ebx,1\)
 191 01d0 80250000 00007F   		andb	\$~0x80,foo
 192                       	
 193 01d7 6683E0FE          		and	\$0xfffe,%ax
 194 01db 662500FF          		and	\$0xff00,%ax
 195 01df 25FEFF00 00       		and	\$0xfffe,%eax
 196 01e4 2500FF00 00       		and	\$0xff00,%eax
 197 01e9 83E0FE            		and	\$0xfffffffe,%eax
 198                       	
 199                       	.code16
 200 01ec 83E0FE            		and	\$0xfffe,%ax
 201 01ef 2500FF            		and	\$0xff00,%ax
 202 01f2 6625FEFF 0000     		and	\$0xfffe,%eax
 203 01f8 662500FF 0000     		and	\$0xff00,%eax
 204 01fe 6683E0FE          		and	\$0xfffffffe,%eax
 205                       	
 206                       	#check 16-bit code auto address prefix
 207                       	.code16gcc
 208 0202 67668D95 00FFFFFF 		leal	-256\(%ebp\),%edx
 209 020a 6788857F FFFFFF   		mov	%al,-129\(%ebp\)
 210 0211 67886580          		mov	%ah,-128\(%ebp\)
 211 0215 67668D9D 20F9FFFF 		leal	-1760\(%ebp\),%ebx
 212 021d 67668984 248C0000 		movl	%eax,140\(%esp\)
 212      00
 213                       	
 214                       		# Force a good alignment.
 215 0226 00000000 00000000 		.p2align	4,0
 215      0000
