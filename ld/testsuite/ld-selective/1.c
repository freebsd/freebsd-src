/* _start should be the only thing left after GC.  */

void _start() __asm__("_start");
void _start()
{
}

void dropme1()
{
}

int dropme2[102] = { 0 };
