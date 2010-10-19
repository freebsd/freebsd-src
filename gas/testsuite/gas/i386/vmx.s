# VMX Instructions

	.text
foo:
	vmcall
	vmlaunch
	vmresume
	vmxoff
	vmclear (%eax)
	vmptrld (%eax)
	vmptrst (%eax)
	vmxon (%eax)
	vmread %eax,%ebx
	vmreadl %eax,%ebx
	vmread %eax,(%ebx)
	vmreadl %eax,(%ebx)
	vmwrite %eax,%ebx
	vmwritel %eax,%ebx
	vmwrite (%eax),%ebx
	vmwritel (%eax),%ebx
	.p2align	4,0
