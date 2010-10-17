#objdump: -dr
#name: @OC@

# Test the @OC@ insn.

.*: +file format elf32-.*arc

Disassembly of section .text:
00000000 <text_label> @IC+7@ffff80	@OC@ 00000000 <text_label>
00000004 <text_label\+4> @IC+7@ffff00	@OC@ 00000000 <text_label>
00000008 <text_label\+8> @IC+7@fffe80	@OC@ 00000000 <text_label>
0000000c <text_label\+c> @IC+7@fffe01	@OC@eq 00000000 <text_label>
00000010 <text_label\+10> @IC+7@fffd81	@OC@eq 00000000 <text_label>
00000014 <text_label\+14> @IC+7@fffd02	@OC@ne 00000000 <text_label>
00000018 <text_label\+18> @IC+7@fffc82	@OC@ne 00000000 <text_label>
0000001c <text_label\+1c> @IC+7@fffc03	@OC@p 00000000 <text_label>
00000020 <text_label\+20> @IC+7@fffb83	@OC@p 00000000 <text_label>
00000024 <text_label\+24> @IC+7@fffb04	@OC@n 00000000 <text_label>
00000028 <text_label\+28> @IC+7@fffa84	@OC@n 00000000 <text_label>
0000002c <text_label\+2c> @IC+7@fffa05	@OC@c 00000000 <text_label>
00000030 <text_label\+30> @IC+7@fff985	@OC@c 00000000 <text_label>
00000034 <text_label\+34> @IC+7@fff905	@OC@c 00000000 <text_label>
00000038 <text_label\+38> @IC+7@fff886	@OC@nc 00000000 <text_label>
0000003c <text_label\+3c> @IC+7@fff806	@OC@nc 00000000 <text_label>
00000040 <text_label\+40> @IC+7@fff786	@OC@nc 00000000 <text_label>
00000044 <text_label\+44> @IC+7@fff707	@OC@v 00000000 <text_label>
00000048 <text_label\+48> @IC+7@fff687	@OC@v 00000000 <text_label>
0000004c <text_label\+4c> @IC+7@fff608	@OC@nv 00000000 <text_label>
00000050 <text_label\+50> @IC+7@fff588	@OC@nv 00000000 <text_label>
00000054 <text_label\+54> @IC+7@fff509	@OC@gt 00000000 <text_label>
00000058 <text_label\+58> @IC+7@fff48a	@OC@ge 00000000 <text_label>
0000005c <text_label\+5c> @IC+7@fff40b	@OC@lt 00000000 <text_label>
00000060 <text_label\+60> @IC+7@fff38c	@OC@le 00000000 <text_label>
00000064 <text_label\+64> @IC+7@fff30d	@OC@hi 00000000 <text_label>
00000068 <text_label\+68> @IC+7@fff28e	@OC@ls 00000000 <text_label>
0000006c <text_label\+6c> @IC+7@fff20f	@OC@pnz 00000000 <text_label>
00000070 <text_label\+70> @IC+7@ffff80	@OC@ 00000070 <text_label\+70>
		RELOC: 00000070 R_ARC_B22_PCREL external_text_label
00000074 <text_label\+74> @IC+0@000000	@OC@ 00000078 <text_label\+78>
00000078 <text_label\+78> @IC+7@fff0a0	@OC@.d 00000000 <text_label>
0000007c <text_label\+7c> @IC+7@fff000	@OC@ 00000000 <text_label>
00000080 <text_label\+80> @IC+7@ffefc0	@OC@.jd 00000000 <text_label>
00000084 <text_label\+84> @IC+7@ffef21	@OC@eq.d 00000000 <text_label>
00000088 <text_label\+88> @IC+7@ffee82	@OC@ne 00000000 <text_label>
0000008c <text_label\+8c> @IC+7@ffee46	@OC@nc.jd 00000000 <text_label>
