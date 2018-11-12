#include <string.h>
#include <sqlite3.h>

int
main(void)
{
	return strcmp(sqlite3_errstr(SQLITE_OK), "not an error");
}
