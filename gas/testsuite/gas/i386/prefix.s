.text ; foo: addr16 fstcw %es:(%si)
 fstsw;fstsw %ax;fstsw %eax
 addr16 fstsw %ax ;addr16 rep cmpsw %es:(%di),%ss:(%si)

# Get a good alignment.
 .p2align	4,0
