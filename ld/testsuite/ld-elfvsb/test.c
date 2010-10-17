#pragma weak main_hidden_data
extern int main_hidden_data;
asm (".hidden main_hidden_data");
 
#pragma weak main_hidden_func
extern int main_hidden_func ();
asm (".hidden main_hidden_func");

int
_start (void)
{
  int ret = 0;

  if (&main_hidden_data != 0)
    ret = 1;
  if (main_hidden_func != 0)
    ret = 1;

  return ret;
}

int
__start (void)
{
  return _start ();
}
