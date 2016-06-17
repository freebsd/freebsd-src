// The code generates a test case which is used
// to detect whether Binutil supports hint
// instruction correctly or not.
#include <string.h>
#include <stdio.h>

int main()
{
	char str[32766];
	memset(str,'\n',32765);
	str[32765]=0;
	printf("%s(p7) hint @pause\n",str);

	return 0;
}
