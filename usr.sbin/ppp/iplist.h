/*
 * $Id:$
 */

struct iplist {
  struct iplist_cur {
    struct in_addr ip;
    int pos;
    char *srcptr;
    int srcitem;
    u_long lstart, nItems;
  } cur;
  int nItems;
  char src[LINE_LEN];
};

extern int iplist_setsrc(struct iplist *, const char *);
extern void iplist_reset(struct iplist *);
extern struct in_addr iplist_setcurpos(struct iplist *, int);
extern struct in_addr iplist_setrandpos(struct iplist *);
extern int iplist_ip2pos(struct iplist *, struct in_addr);
extern struct in_addr iplist_next(struct iplist *);

#define iplist_isvalid(x) ((x)->src[0] != '\0')
