extern void exewrite (void);
__attribute((dllimport)) int  global_a;

void 
dllwrite (void)
{
  global_a = 3;
  exewrite ();
}

int _stdcall testdll_main(int p1, unsigned long p2, void* p3)
{
	return 1;
}
