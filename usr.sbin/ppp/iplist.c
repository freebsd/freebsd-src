/*
 * $Id:$
 */

#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <stdlib.h>
#include <string.h>

#include "command.h"
#include "mbuf.h"
#include "log.h"
#include "defs.h"
#include "iplist.h"

static int
do_inet_aton(const char *start, const char *end, struct in_addr *ip)
{
  static char ipstr[16];

  if (end - start > 15) {
    LogPrintf(LogWARN, "%.*s: Invalid IP address\n", end-start, start);
    return 0;
  }
  strncpy(ipstr, start, end-start);
  ipstr[end-start] = '\0';
  return inet_aton(ipstr, ip);
}

static void
iplist_first(struct iplist *list)
{
  list->cur.pos = -1;
}

static int
iplist_setrange(struct iplist *list, char *range)
{
  char *ptr, *to;

  if ((ptr = strpbrk(range, ",-")) == NULL) {
    if (!inet_aton(range, &list->cur.ip))
      return 0;
    list->cur.lstart = ntohl(list->cur.ip.s_addr);
    list->cur.nItems = 1;
  } else {
    if (!do_inet_aton(range, ptr, &list->cur.ip))
      return 0;
    if (*ptr == ',') {
      list->cur.lstart = ntohl(list->cur.ip.s_addr);
      list->cur.nItems = 1;
    } else {
      struct in_addr endip;

      to = ptr+1;
      if ((ptr = strpbrk(to, ",-")) == NULL)
        ptr = to + strlen(to);
      if (*to == '-')
        return 0;
      if (!do_inet_aton(to, ptr, &endip))
        return 0;
      list->cur.lstart = ntohl(list->cur.ip.s_addr);
      list->cur.nItems = ntohl(endip.s_addr) - list->cur.lstart + 1;
      if (list->cur.nItems < 1)
        return 0;
    }
  }
  list->cur.srcitem = 0;
  list->cur.srcptr = range;
  return 1;
}

static int
iplist_nextrange(struct iplist *list)
{
  char *ptr, *to, *end;

  ptr = list->cur.srcptr;
  if (ptr != NULL && (ptr = strchr(ptr, ',')) != NULL)
    ptr++;
  else
    ptr = list->src;

  while (*ptr != '\0' && !iplist_setrange(list, ptr)) {
    if ((end = strchr(ptr, ',')) == NULL)
      end = ptr + strlen(ptr);
    if (end == ptr)
      return 0;
    LogPrintf(LogWARN, "%.*s: Invalid IP range (skipping)\n", end - ptr, ptr);
    to = ptr;
    do
      *to = *end++;
    while (*to++ != '\0');
    if (*ptr == '\0')
      ptr = list->src;
  }

  return 1;
}

struct in_addr
iplist_next(struct iplist *list)
{
  if (list->cur.pos == -1) {
    list->cur.srcptr = NULL;
    if (!iplist_nextrange(list)) {
      list->cur.ip.s_addr = INADDR_ANY;
      return list->cur.ip;
    }
  } else if (++list->cur.srcitem == list->cur.nItems) {
    if (!iplist_nextrange(list)) {
      list->cur.ip.s_addr = INADDR_ANY;
      list->cur.pos = -1;
      return list->cur.ip;
    }
  } else
    list->cur.ip.s_addr = htonl(list->cur.lstart + list->cur.srcitem);
  list->cur.pos++;

  return list->cur.ip;
}

int
iplist_setsrc(struct iplist *list, const char *src)
{
  strncpy(list->src, src, sizeof(list->src));
  list->src[sizeof(list->src)-1] = '\0';
  list->cur.srcptr = list->src;
  do {
    if (iplist_nextrange(list))
      list->nItems += list->cur.nItems;
    else
      return 0;
  } while (list->cur.srcptr != list->src);
  return 1;
}

void
iplist_reset(struct iplist *list)
{
  list->src[0] = '\0';
  list->nItems = 0;
  list->cur.pos = -1;
}

struct in_addr
iplist_setcurpos(struct iplist *list, int pos)
{
  if (pos < 0 || pos >= list->nItems) {
    list->cur.pos = -1;
    list->cur.ip.s_addr = INADDR_ANY;
    return list->cur.ip;
  }

  list->cur.srcptr = NULL;
  list->cur.pos = 0;
  while (1) {
    iplist_nextrange(list);
    if (pos < list->cur.nItems) {
      if (pos) {
        list->cur.srcitem = pos;
        list->cur.pos += pos;
        list->cur.ip.s_addr = htonl(list->cur.lstart + list->cur.srcitem);
      }
      break;
    }
    pos -= list->cur.nItems;
    list->cur.pos += list->cur.nItems;
  }

  return list->cur.ip;
}

struct in_addr
iplist_setrandpos(struct iplist *list)
{
  randinit();
  return iplist_setcurpos(list, random() % list->nItems);
}

int
iplist_ip2pos(struct iplist *list, struct in_addr ip)
{
  struct iplist_cur cur;
  int f, result;

  result = -1;
  memcpy(&cur, &list->cur, sizeof(cur));

  for (iplist_first(list), f = 0; f < list->nItems; f++)
    if (iplist_next(list).s_addr == ip.s_addr) {
      result = list->cur.pos;
      break;
    }

  memcpy(&list->cur, &cur, sizeof(list->cur));
  return result;
}
