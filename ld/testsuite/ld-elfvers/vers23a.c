__asm__(".symver _old_bar,bar@VERS.0");

void
_old_bar (void) 
{
}

void
foo (void) 
{
}
