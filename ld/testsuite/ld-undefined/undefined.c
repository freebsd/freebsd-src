/* This file is used to test the linker's reporting of undefined
   symbols.  */

extern int this_function_is_not_defined ();

int
function ()
{
  return this_function_is_not_defined ();
}
