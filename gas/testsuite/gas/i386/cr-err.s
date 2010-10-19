.text

_start:
	movl	(%cr0), %eax
	movl	%eax, (%cr7)
	movl	(%cr8), %eax
	movl	%eax, (%cr15)
	movl	(%db0), %eax
	movl	%eax, (%db7)
	movl	(%dr0), %eax
	movl	%eax, (%dr7)
	movl	(%tr0), %eax
	movl	%eax, (%tr7)

.att_syntax noprefix
	movl	(cr0), eax
	movl	eax, (cr7)
	movl	(cr8), eax
	movl	eax, (cr15)
	movl	(db0), eax
	movl	eax, (db7)
	movl	(dr0), eax
	movl	eax, (dr7)
	movl	(tr0), eax
	movl	eax, (tr7)

.intel_syntax noprefix
	mov	eax, [cr0]
	mov	[cr7], eax
	mov	eax, [cr8]
	mov	[cr15], eax
	mov	eax, [dr0]
	mov	[dr7], eax
	mov	eax, [tr0]
	mov	[tr7], eax
