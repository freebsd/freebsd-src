#include "db-int.h"
#include <stdio.h>
#include <fcntl.h>
#include <assert.h>
#include <stdlib.h>

int
main(void)
{
	HASHINFO info;
	DB *db;
	DBT key, value, returned;
	int *data;
	int n, i;

	info.bsize = 512;
	info.cachesize = 500;
	info.lorder = 0;
	info.ffactor = 4;
	info.nelem = 0;
	info.hash = NULL;

	db = dbopen("big2.db", O_RDWR|O_CREAT|O_TRUNC|O_BINARY, 0664, DB_HASH, &info);
	data = malloc(800 * sizeof(int));
	for (n = 0; n < 800; n++)
		data[n] = 0xDEADBEEF;
	key.size = sizeof(int);
	key.data = &n;
	value.size = 800 * sizeof(int);
	value.data = (void *)data;

	for (n = 0; n < 200000; n++) {
		returned.data = NULL;
		if (n == 4627)
			printf("");
		if (n % 50 == 0)
			printf("put n = %d\n", n);
		if (db->put(db, &key, &value, 0) != 0)
			printf("put error, n = %d\n", n);
		if (db->get(db, &key, &returned, 0) != 0)
			printf("Immediate get error, n = %d\n", n);
		assert (returned.size == 3200);
		for (i = 0; i < 800; i++)
			if (((int *)returned.data)[i] != 0xDEADBEEF)
				printf("ERRORRRRRR!!!\n");

	}

	for (n = 0; n < 200000; n++) {
		if (n % 50 == 0)
			printf("seq n = %d\n", n);
		if ((db->seq(db, &key, &returned, 0)) != 0)
			printf("Seq error, n = %d\n", n);

		assert(returned.size == 3200);

		for (i = 0; i < 800; i++)
			if (((int *)returned.data)[i] != 0xDEADBEEF)
				printf("ERRORRRRRR!!! seq %d\n", n);
	}

	for (n = 0; n < 2000; n++) {
		if (n % 50 == 0)
			printf("get n = %d\n", n);
		if (db->get(db, &key, &returned, 0) != 0)
			printf("Late get error, n = %d\n", n);
		assert(returned.size == 1200);
		for (i = 0; i < 300; i++)
			if (((int *)returned.data)[i] != 0xDEADBEEF)
				printf("ERRORRRRRR!!!, get %d\n", n);
	}
   	db->close(db);
	free(value.data);
	return(0);
}
