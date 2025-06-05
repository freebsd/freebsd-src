/* chkseq.c  Check sequential read and write */

#include <sys/stat.h>
#include "db-int.h"
#include <errno.h>    /* Error numbers */
#include <fcntl.h>    /* O_CREAT,  O_RDWR */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


void main(int argc, char *argv[]) {
  char id1[] = {"          "}, id2[] = {"          "};
  int i;
  long in = 0L, out = 0L;
  DB *dbp, *dbpo;
  DBT key, data, keyo, datao;
  FILE *fopen(), *fin;

  unlink("test.db");
  if ((fin = fopen("data","r")) == NULL) {
    printf("Unable to open %s\n","data");
    exit(25);
  }
  if ((dbp = dbopen("test.db",O_RDWR | O_CREAT | O_BINARY, 0664
       , DB_BTREE, NULL )) == NULL) {
    printf("\n Open error on test.db %d %s\n",errno,strerror(errno));
    exit(25);
  }

  while (fscanf(fin," %10s%10s",id1,id2) > 0) {
    key.size = 11;
    data.size = 11;
    key.data = id1;
    data.data = id2;
    printf("%10s %10s\n",key.data,data.data);
    if (dbp->put(dbp, &key, &data,R_NOOVERWRITE) != 0) {
      printf("Error writing output\n");
    }
    out++;
  }
  printf("%d Records in\n",out);
  dbp->close(dbp);

  if ((dbp = dbopen("test.db", O_RDWR | O_BINARY, 0664
       , DB_BTREE, NULL )) == NULL) {
    printf("\n Error on dbopen %d %s\n",errno,strerror(errno));
    exit(61);
  }

  while (dbp->seq(dbp, &key, &data,R_NEXT) == 0) {
    strcpy(id1,key.data);
    keyo.size = 11;
    datao.size = 11;
    keyo.data = id1;
    strcpy(id2,data.data);
    id2[0] = 'U';
    datao.data=id2;
    printf("%10s %10s\n",key.data,data.data);
    in++;
    if (in > 10) break;
#ifdef notdef
    if (dbp->put(dbp, &keyo, &datao,0) != 0) {
        printf("Write failed at %d\n",in);
        exit(85);
    }
#else
    if (dbp->put(dbp, &keyo, &datao,R_CURSOR) != 0) {
        printf("Write failed at %d\n",in);
        exit(85);
    }
#endif
  }
  printf("%d Records copied\n",in);
  in = 0;
    dbp->seq(dbp, &key, &data,R_FIRST);
    printf("%10s %10s\n",key.data,data.data);
    in++;
  while (dbp->seq(dbp, &key, &data,R_NEXT) == 0) {
    in++;
    printf("%10s %10s\n",key.data,data.data);
  }
  printf("%d Records read\n",in);
  dbp->close(dbp);
}
