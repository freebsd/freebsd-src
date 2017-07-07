/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * $Id$
 */

#include <sys/file.h>
#include <fcntl.h>
#include <db.h>
#include <stdio.h>

main(int argc, char *argv[])
{
    char *file;
    DB *db;
    DBT dbkey, dbdata;
    int code, i;

    HASHINFO     info;

    info.hash = NULL;
    info.bsize = 256;
    info.ffactor = 8;
    info.nelem = 25000;
    info.lorder = 0;

    if (argc != 2) {
        fprintf(stderr, "usage: argv[0] dbfile\n");
        exit(2);
    }

    file = argv[1];

    if((db = dbopen(file, O_RDWR, 0666, DB_HASH, &info)) == NULL) {
        perror("Opening db file");
        exit(1);
    }

    if ((code = (*db->seq)(db, &dbkey, &dbdata, R_FIRST)) == -1) {
        perror("starting db iteration");
        exit(1);
    }

    while (code == 0) {
        for (i=0; i<dbkey.size; i++)
            printf("%02x", (int) ((unsigned char *) dbkey.data)[i]);
        printf("\t");
        for (i=0; i<dbdata.size; i++)
            printf("%02x", (int) ((unsigned char *) dbdata.data)[i]);
        printf("\n");

        code = (*db->seq)(db, &dbkey, &dbdata, R_NEXT);
    }

    if (code == -1) {
        perror("during db iteration");
        exit(1);
    }

    if ((*db->close)(db) == -1) {
        perror("closing db");
        exit(1);
    }

    exit(0);
}
