.text
.intel_syntax noprefix
# REX prefix and addressing modes.
add edx,ecx
add edx,r9d
add r10d,ecx
add rdx,rcx
add r10,r9
add r8d,eax
add r8w,ax
add r8,rax
add eax,0x44332211
add rax,0xfffffffff4332211
add ax,0x4433
add rax,0x44332211
add dl,cl
add bh,dh
add dil,sil
add r15b,sil
add dil,r14b
add r15b,r14b
PUSH RAX
PUSH R8
POP R9
ADD AL,0x11
ADD AH,0x11
ADD SPL,0x11
ADD R8B,0x11
ADD R12B,0x11
MOV RAX,CR0
MOV R8,CR0
MOV RAX,CR8
MOV CR8,RAX
REP MOVSQ #[RSI],[RDI]
REP MOVSW #[RSI,[RDI]
REP MOVSQ #[RSI],[RDI]
MOV AL, 0x11
MOV AH, 0x11
MOV SPL, 0x11
MOV R12B, 0x11
MOV EAX,0x11223344
MOV R8D,0x11223344
MOV RAX,0x1122334455667788
MOV R8,0x1122334455667788
add eax,[rax]
ADD EAX,[R8]
ADD R8D,[R8]
ADD RAX,[R8]
ADD EAX,[0x22222222+RIP]
ADD EAX,[RBP+0x00]
ADD EAX,[0x22222222]
ADD EAX,[R13+0]
ADD EAX,[RAX+RAX*4]
ADD EAX,[R8+RAX*4]
ADD R8D,[R8+RAX*4]
ADD EAX,[R8+R8*4]
ADD [RCX+R8*4],R8D
ADD EDX,[RAX+RAX*8]
ADD EDX,[RAX+RCX*8]
ADD EDX,[RAX+RDX*8]
ADD EDX,[RAX+RBX*8]
ADD EDX,[RAX]
ADD EDX,[RAX+RBP*8]
ADD EDX,[RAX+RSI*8]
ADD EDX,[RAX+RDI*8]
ADD EDX,[RAX+R8*8]
ADD EDX,[RAX+R9*8]
ADD EDX,[RAX+R10*8]
ADD EDX,[RAX+R11*8]
ADD EDX,[RAX+R12*8]
ADD EDX,[RAX+R13*8]
ADD EDX,[RAX+R14*8]
ADD EDX,[RAX+R15*8]
ADD ECX,0x11
ADD DWORD PTR [RAX],0x11
ADD QWORD PTR [RAX],0x11
ADD DWORD PTR [R8],0x11
ADD DWORD PTR [RCX+RAX*4],0x11
ADD DWORD PTR [R9+RAX*4],0x11
ADD DWORD PTR [RCX+R8*4],0x11
ADD DWORD PTR [0x22222222+RIP],0x33
ADD QWORD PTR [RIP+0x22222222],0x33
ADD DWORD PTR [RIP+0x22222222],0x33333333
ADD QWORD PTR [RIP+0x22222222],0x33333333
ADD DWORD PTR [RAX*8+0x22222222],0x33
ADD DWORD PTR [RAX+0x22222222],0x33
ADD DWORD PTR [RAX+0x22222222],0x33
ADD DWORD PTR [R8+RBP*8],0x33
ADD DWORD PTR [0x22222222],0x33		
#new instructions
MOVABS AL,[0x8877665544332211]
MOVABS EAX,[0x8877665544332211]
MOVABS [0x8877665544332211],AL
MOVABS [0x8877665544332211],EAX
MOVABS RAX,[0x8877665544332211]
MOVABS [0x8877665544332211],RAX
cqo
cdqe
movsx rax, eax
movsx rax, ax
movsx rax, al
bar:
.att_syntax
#testcase for symbol references.

#immediates - various sizes:

mov $symbol, %al
mov $symbol, %ax
mov $symbol, %eax
mov $symbol, %rax

#addressing modes:

#absolute 64bit addressing
movabs symbol, %eax

#absolute 32bit addressing
mov symbol, %eax

#arithmetic
mov symbol(%rax), %eax

#RIP relative
mov symbol(%rip), %eax

.intel_syntax noprefix

#immediates - various sizes:
mov al, flat:symbol
mov ax, flat:symbol
mov eax, flat:symbol
mov rax, flat:symbol

#parts aren't supported by the parser, yet (and not at all for symbol refs)
#mov eax, high part symbol
#mov eax, low part symbol

#addressing modes

#absolute 64bit addressing
movabs eax, [symbol]

#absolute 32bit addressing
mov eax, [symbol]

#arithmetic
mov eax, [rax+symbol]

#RIP relative
mov eax, [rip+symbol]

foo:
.att_syntax
#absolute 64bit addressing
mov 0x8877665544332211,%al
mov 0x8877665544332211,%ax
mov 0x8877665544332211,%eax
mov 0x8877665544332211,%rax
mov %al,0x8877665544332211
mov %ax,0x8877665544332211
mov %eax,0x8877665544332211
mov %rax,0x8877665544332211
movb 0x8877665544332211,%al
movw 0x8877665544332211,%ax
movl 0x8877665544332211,%eax
movq 0x8877665544332211,%rax
movb %al,0x8877665544332211
movw %ax,0x8877665544332211
movl %eax,0x8877665544332211
movq %rax,0x8877665544332211

#absolute signed 32bit addressing
mov 0xffffffffff332211,%al
mov 0xffffffffff332211,%ax
mov 0xffffffffff332211,%eax
mov 0xffffffffff332211,%rax
mov %al,0xffffffffff332211
mov %ax,0xffffffffff332211
mov %eax,0xffffffffff332211
mov %rax,0xffffffffff332211
movb 0xffffffffff332211,%al
movw 0xffffffffff332211,%ax
movl 0xffffffffff332211,%eax
movq 0xffffffffff332211,%rax
movb %al,0xffffffffff332211
movw %ax,0xffffffffff332211
movl %eax,0xffffffffff332211
movq %rax,0xffffffffff332211

cmpxchg16b (%rax)

.intel_syntax noprefix
cmpxchg16b oword ptr [rax]

# Get a good alignment.
 .p2align	4,0
