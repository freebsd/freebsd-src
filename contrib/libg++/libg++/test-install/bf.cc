#include <stream.h>
#include <String.h>

// gcc-2.3.2 is buggy, and can't deal with the following.
// Take the wimpy way out for now until 2.3.3 is released.
#if 0
String s1 = String("Hello ");
#else
String s1("Hello ");
#endif
String s2(" world!\n");

int f()
{
	cout << s1 + s2;
        return cout.good();
}

