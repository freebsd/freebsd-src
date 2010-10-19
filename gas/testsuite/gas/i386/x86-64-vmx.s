# VMX Instructions

	.text
foo:
	vmcall
	vmlaunch
	vmresume
	vmxoff
	vmclear (%rax)
	vmptrld (%rax)
	vmptrst (%rax)
	vmxon (%rax)
	vmread %rax,%rbx
	vmreadq %rax,%rbx
	vmread %rax,(%rbx)
	vmreadq %rax,(%rbx)
	vmwrite %rax,%rbx
	vmwriteq %rax,%rbx
	vmwrite (%rax),%rbx
	vmwriteq (%rax),%rbx
	.p2align	4,0
