.text
	lea		symbol(%eax), %rax
	lea		symbol(%r8d), %rax
	addr32 lea	symbol(%rip), %rax
	addr32 lea	symbol, %rax
	addr32 mov	0x600898,%al
	addr32 mov	0x600898,%ax
	addr32 mov	0x600898,%eax
	addr32 mov	0x600898,%rax
	addr32 mov	%al,0x600898
	addr32 mov	%ax,0x600898
	addr32 mov	%eax,0x600898
	addr32 mov	%rax,0x600898
