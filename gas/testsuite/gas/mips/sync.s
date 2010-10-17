	.text
foo:
	.ent foo
	sync
	sync.p
	sync.l
	.end foo

# Force at least 8 (non-delay-slot) zero bytes, to make 'objdump' print ...
	.space  8
