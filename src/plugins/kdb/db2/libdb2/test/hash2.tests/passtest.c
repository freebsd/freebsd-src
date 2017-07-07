#include "db-int.h"
#include <fcntl.h>
#include <stdio.h>
#include <string.h>

extern int hash_expansions;

int
main(void)
{
    FILE *keys, *vals;
    DB *db;
    DBT key, val;
    char *key_line, *val_line, *get_key, *get_val, *old, *key2;
    HASHINFO passwd;
    int n = 0, i = 0, expected;

    key_line = (char *)malloc(100);
    val_line = (char *)malloc(300);
    old = (char *)malloc(300);

    keys = fopen("yp.keys", "rt");
    vals = fopen("yp.total", "rt");

    passwd.bsize =  1024;
    passwd.cachesize = 1024 * 1024;
    passwd.ffactor = 10;
    passwd.hash = NULL;
    passwd.nelem = 0;
    passwd.lorder = 4321;


    db = dbopen("/usr/tmp/passwd.db", O_RDWR|O_CREAT|O_TRUNC|O_BINARY, 0664, DB_HASH,
		&passwd);
    if (!db) {
	fprintf(stderr, "create_db: couldn't create database file\n");
	exit(1);
    }

    while ((key_line = fgets(key_line, 100, keys)) != NULL) {
	if (n % 1000 == 0)
	  fprintf(stderr, "Putting #%d.\n", n);
	n++;
	fgets(val_line, 300, vals);
	key.size = strlen(key_line);
	key.data = (void *)key_line;
	val.size = strlen(val_line);
	val.data = (void *)val_line;
	if (db->put(db, &key, &val, 0) != 0)
	  fprintf(stderr, "Put error, n = %d\n", n);
	if (db->get(db, &key, &val, 0) != 0)
	  fprintf(stderr, "Immediate get error, n = %d\n", n);
    }
    fprintf(stderr, "Done with put!\n");
    free(key_line);
    free(val_line);
    fclose(keys);
    fclose(vals);
    db->close(db);




    keys = fopen("yp.keys", "rt");
    vals = fopen("yp.total", "rt");
    get_key = (char *)malloc(100);
    get_val = (char *)malloc(300);

    db = dbopen("/usr/tmp/passwd.db", O_RDWR|O_BINARY, 0664, DB_HASH, &passwd);
    if (!db)
      fprintf(stderr, "Could not open db!\n");
    n = 0;
    while ((get_key = fgets(get_key, 100, keys)) != NULL) {
	n++;
	if (n % 1000 == 0)
	  fprintf(stderr, "Getting #%d.\n", n);
	key.size = strlen(get_key);
	key.data = (void *)get_key;
	if (db->get(db, &key, &val, 0) != 0)
	  fprintf(stderr, "Retrieval error on %s\n", get_key);
	fgets(get_val, 300, vals);
	if (memcmp(val.data, (void *)get_val, val.size)) {
	    fprintf(stderr, "Unmatched get on %s.\n", get_key);
	    fprintf(stderr, "Input = %s\nOutput = %s\n", get_val,
		    (char *)val.data);
	}
    }
    expected = n;
    fclose(vals);
    fclose(keys);
    free(get_key);
    free(get_val);
    db->close(db);




    get_key = (char *)malloc(100);
    get_val = (char *)malloc(300);

    db = dbopen("/usr/tmp/passwd.db", O_RDWR, 0664, DB_HASH, &passwd);
    if (!db)
      fprintf(stderr, "Could not open db!\n");
    n = 0;
    for (;;) {
	n++;
	if (n % 1000 == 0)
	  fprintf(stderr, "Sequence getting #%d.\n", n);
	if (db->seq(db, &key, &val, 0) != 0) {
	    fprintf(stderr,
		    "Exiting sequence retrieve; n = %d, expected = %d\n",
		    n - 1 , expected);
	    break;
	}
    }
    free(get_key);
    free(get_val);
    db->close(db);

    get_key = (char *)malloc(100);
    key2 = (char *)malloc(100);

    keys = fopen("yp.keys", "rt");
    vals = fopen("yp.total", "rt");

    db = dbopen("/usr/tmp/passwd.db", O_RDWR|O_BINARY, 0664, DB_HASH, &passwd);
    n = 0;
    while ((get_key = fgets(get_key, 100, keys)) != NULL) {
	if (n % 1000 == 0)
	  fprintf(stderr, "Deleting #%d.\n", n);
	n+=2;
	key2 = fgets(get_key, 100, keys);
	if (!key2)
	  break;
	key.data = (void *)key2;
	key.size = strlen(key2);
	if (db->del(db, &key, 0) != 0)
	  fprintf(stderr, "Delete error on %d", n);
    }

    db->close(db);
    free(get_key);
    free(key2);
    fclose(keys);
    fclose(vals);

    get_key = (char *)malloc(100);
    key2 = (char *)malloc(100);
    get_val = (char *)malloc(300);

    keys = fopen("yp.keys", "rt");
    vals = fopen("yp.total", "rt");

    db = dbopen("/usr/tmp/passwd.db", O_RDWR|O_BINARY, 0664, DB_HASH, &passwd);
    n = 0;
    while ((get_key = fgets(get_key, 100, keys)) != NULL) {
	n += 2;
	if (n % 1000 == 0)
	  fprintf(stderr, "Re-retrieving #%d.\n", n);
	key2 = fgets(key2, 100, keys);
	if (!key2)
	  break;
	key.data = (void *)get_key;
	key.size = strlen(get_key);
	if (db->get(db, &key, &val, 0) != 0)
	  fprintf(stderr, "Retrieval after delete error on %d\n", n);
	fgets(get_val, 300, vals);
	if (memcmp(val.data, (void *)get_val, val.size)) {
	    fprintf(stderr, "Unmatched get after delete on %s.\n", get_key);
	    fprintf(stderr, "Input = %s\nOutput = %s\n", get_val,
		    (char *)val.data);
	}
	fgets(get_val, 300, vals);
    }

    db->close(db);
    free(get_key);
    free(key2);
    free(get_val);
    fclose(keys);
    fclose(vals);

    exit(0);
}
