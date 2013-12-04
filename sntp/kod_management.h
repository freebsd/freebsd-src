#ifndef KOD_MANAGEMENT_H
#define KOD_MANAGEMENT_H

#include <time.h>

struct kod_entry {
	char hostname[255];
	time_t timestamp;
	char type[5];
};

int search_entry(char *hostname, struct kod_entry **dst);

void add_entry(char *hostname, char *type);
void delete_entry(char *hostname, char *type);
void kod_init_kod_db(const char *db_file);
void write_kod_db(void);


#endif
