	.text
        .size   function,1
        .type   function,%function
function:
	.byte	0x0
        .data
        .type   object,%object
        .size   object,1
object:
	.byte	0x0
        .type   tls_object,%tls_object
        .size   tls_object,1
tls_object:
	.byte	0x0
        .type   notype,%notype
        .size   notype,1
notype:
	.byte	0x0
