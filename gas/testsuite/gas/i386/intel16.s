 .intel_syntax noprefix
 .code16
 .text

 movsx	eax,word ptr [0]
 movsx	eax,byte ptr [0]
 movsx	ax,byte ptr [0]
 movzx	eax,word ptr [0]
 movzx	eax,byte ptr [0]
 movzx	ax,byte ptr [0]
 .p2align 4,0
