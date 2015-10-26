
#include "config.h"
#include "stdlib.h"
#include "sntptest.h"

#include "fileHandlingTest.h" //required because of the h.in thingy

#include <string.h>
#include <unistd.h>

/*
enum DirectoryType {
	INPUT_DIR = 0,
	OUTPUT_DIR = 1
};
*/
//extern const char srcdir[];

const char *
CreatePath(const char* filename, enum DirectoryType argument) {
	const char srcdir[] = SRCDIR_DEF;//"@abs_srcdir@/data/";
	char * path = emalloc (sizeof (char) * (strlen(srcdir) + 256));

	//char cwd[1024];

	strcpy(path, srcdir);
	strcat(path, filename);

	return path;
}


int
GetFileSize(FILE *file) {
	fseek(file, 0L, SEEK_END);
	int length = ftell(file);
	fseek(file, 0L, SEEK_SET);

	return length;
}


bool
CompareFileContent(FILE* expected, FILE* actual) {
	int currentLine = 1;

	char actualLine[1024];
	char expectedLine[1024];
	size_t lenAct = sizeof actualLine;
	size_t lenExp = sizeof expectedLine;
	
	while (  ( (fgets(actualLine, lenAct, actual)) != NULL)
	      && ( (fgets(expectedLine, lenExp, expected)) != NULL )
	      ) {

	
		if( strcmp(actualLine,expectedLine) !=0 ){
			printf("Comparision failed on line %d",currentLine);
			return FALSE;
		}

		currentLine++;
	}

	return TRUE;
}


void
ClearFile(const char * filename) {
	if (!truncate(filename, 0))
		exit(1);
}

