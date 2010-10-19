__attribute__ ((section (".nocrossrefs")))
static void
foo ()
{
}

void (*dummy) () = foo;
