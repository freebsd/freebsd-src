int global_a = 2;

void 
exewrite (void)
{
  global_a = 1;
}

extern void dllwrite (void);

int _stdcall
testexe_main (void* p1, void *p2, char* p3, int p4)
{
  dllwrite ();
  return 0;
}
