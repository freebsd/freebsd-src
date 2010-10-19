#as: -mcpu=fr405
#objdump: -dr

.*:     file format elf32-frv(|fdpic)

Disassembly of section \.text:

00000000 <.*>:
.*:	81 18 41 45 	smu gr4,gr5
.*:	81 18 41 85 	smass gr4,gr5
.*:	81 18 41 c5 	smsss gr4,gr5
.*:	8d 18 40 85 	slass gr4,gr5,gr6
.*:	8b 18 01 04 	scutss gr4,gr5
.*:	8d 18 40 05 	addss gr4,gr5,gr6
.*:	8d 18 40 45 	subss gr4,gr5,gr6
