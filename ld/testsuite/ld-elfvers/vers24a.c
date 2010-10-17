/* Test whether .symver x, x@foo
   causes relocations against x within the same shared library
   to become dynamic relocations against x@foo.  */
int x = 12;
__asm__ (".symver x, x@VERS.0");
