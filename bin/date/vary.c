#include <time.h>
#include <string.h>
#include <stdlib.h>
#include "vary.h"

struct trans {
  int val;
  char *str;
};

static struct trans trans_mon[] = {
  { 1, "jan" }, { 2, "feb" }, { 3, "mar" }, { 4, "apr" }, { 5, "may" },
  { 6, "jun" }, { 7, "jul" }, { 8, "aug" }, { 9, "sep" }, { 10, "oct" },
  { 11, "nov" }, { 12, "dec" },
  { 1, "january" }, { 2, "february" }, { 3, "march" }, { 4, "april" },
  { 6, "june" }, { 7, "july" }, { 8, "august" }, { 9, "september" },
  { 10, "october" }, { 11, "november" }, { 12, "december" },
  { -1, NULL }
};

static struct trans trans_wday[] = {
  { 0, "sun" }, { 1, "mon" }, { 2, "tue" }, { 3, "wed" }, { 4, "thr" },
  { 4, "thu" }, { 5, "fri" }, { 6, "sat" },
  { 0, "sunday" }, { 1, "monday" }, { 2, "tuesday" }, { 3, "wednesday" },
  { 4, "thursday" }, { 5, "friday" }, { 6, "saturday" },
  { -1, NULL }
};

static char digits[] = "0123456789";

static int
trans(const struct trans t[], const char *arg)
{
  int f;

  if (strspn(arg, digits) == strlen(arg))
    return atoi(arg);

  for (f = 0; t[f].val != -1; f++)
    if (!strcasecmp(t[f].str, arg))
      return t[f].val;

  return -1;
}

struct vary *
vary_append(struct vary *v, char flag, char *arg)
{
  struct vary *result, **nextp;

  if (!strchr("DWMY", flag))
    return 0;

  if (v) {
    result = v;
    while (v->next)
      v = v->next;
    nextp = &v->next;
  } else
    nextp = &result;

  *nextp = (struct vary *)malloc(sizeof(struct vary));
  (*nextp)->flag = flag;
  (*nextp)->arg = arg;
  (*nextp)->next = NULL;
  return result;
}

static int mdays[12] = { 31, 0, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

static int
daysinmonth(const struct tm *t)
{
  int year;

  year = t->tm_year + 1900;

  if (t->tm_mon == 1)
    if (!(year % 400))
      return 29;
    else if (!(year % 100))
      return 28;
    else if (!(year % 4))
      return 29;
    else
      return 28;
  else if (t->tm_mon >= 0 && t->tm_mon < 12)
    return mdays[t->tm_mon];

  return 0;
}


static int
adjyear(struct tm *t, char type, int val)
{
  switch (type) {
    case '+':
      t->tm_year += val;
      break;
    case '-':
      t->tm_year -= val;
      break;
    default:
      t->tm_year = val;
      if (t->tm_year < 69)
      	t->tm_year += 100;		/* as per date.c */
      else if (t->tm_year > 1900)
        t->tm_year -= 1900;             /* struct tm holds years since 1900 */
      break;
  }
  return mktime(t) != -1;
}

static int
sadjyear(struct tm *t, char *arg)
{
  switch (*arg) {
    case '+':
    case '-':
      return adjyear(t, *arg, atoi(arg+1));
    default:
      return adjyear(t, '\0', atoi(arg));
  }
}

static int
adjmon(struct tm *t, char type, int val, int istext)
{
  if (val < 0)
    return 0;

  switch (type) {
    case '+':
      if (istext)
        if (val <= t->tm_mon)
          val += 11 - t->tm_mon;	/* early next year */
        else
          val -= t->tm_mon + 1;		/* later this year */
      if (!adjyear(t, '+', (t->tm_mon + val) / 12))
        return 0;
      val %= 12;
      t->tm_mon += val;
      if (t->tm_mon > 11)
        t->tm_mon -= 12;
      break;

    case '-':
      if (istext)
        if (val-1 > t->tm_mon)
          val = 13 - val + t->tm_mon;	/* later last year */
        else
          val = t->tm_mon - val + 1;	/* early this year */
      if (!adjyear(t, '-', val / 12))
        return 0;
      val %= 12;
      if (val > t->tm_mon) {
        if (!adjyear(t, '-', 1))
          return 0;
        val -= 12;
      }
      t->tm_mon -= val;
      break;

    default:
      if (val > 12 || val < 1)
        return 0;
      t->tm_mon = --val;
  }

  return mktime(t) != -1;
}

static int
sadjmon(struct tm *t, char *arg)
{
  int istext;
  int val;

  switch (*arg) {
    case '+':
    case '-':
      istext = strspn(arg+1, digits) != strlen(arg+1);
      val = trans(trans_mon, arg+1);
      return adjmon(t, *arg, val, istext);
    default:
      istext = strspn(arg, digits) != strlen(arg);
      val = trans(trans_mon, arg);
      return adjmon(t, '\0', val, istext);
  }
}

static int
adjday(struct tm *t, char type, int val)
{
  int mdays;
  switch (type) {
    case '+':
      while (val) {
        mdays = daysinmonth(t);
        if (val > mdays - t->tm_mday) {
          val -= mdays - t->tm_mday + 1;
          t->tm_mday = 1;
          if (!adjmon(t, '+', 1, 0))
            return 0;
        } else {
          t->tm_mday += val;
          val = 0;
        }
      }
      break;
    case '-':
      while (val)
        if (val >= t->tm_mday) {
          val -= t->tm_mday;
          t->tm_mday = 1;
          if (!adjmon(t, '-', 1, 0))
            return 0;
          t->tm_mday = daysinmonth(t);
        } else {
          t->tm_mday -= val;
          val = 0;
        }
      break;
    default:
      if (val > 0 && val <= daysinmonth(t))
        t->tm_mday = val;
      else
        return 0;
      break;
  }

  return mktime(t) != -1;
}

static int
sadjwday(struct tm *t, char *arg)
{
  int istext;
  int val;

  switch (*arg) {
    case '+':
    case '-':
      istext = strspn(arg+1, digits) != strlen(arg+1);
      val = trans(trans_wday, arg+1);
      break;
    default:
      istext = 0;
      val = trans(trans_wday, arg);
      break;
  }

  if (val < 0)
    return 0;

  switch (*arg) {
    case '+':
      if (istext)
        if (val < t->tm_wday)
          val = 7 - t->tm_wday + val;  /* early next week */
        else
          val -= t->tm_wday;           /* later this week */
      else
        val *= 7;                      /* "-W +5" == "5 weeks in the future" */
      return adjday(t, '+', val);
    case '-':
      if (istext)
        if (val > t->tm_wday)
          val = 7 - val + t->tm_wday;  /* later last week */
        else
          val = t->tm_wday - val;      /* early this week */
      else
        val *= 7;                      /* "-W -5" == "5 weeks ago" */
      return adjday(t, '-', val);
    default:
      if (val < t->tm_wday)
        return adjday(t, '-', t->tm_wday - val);
      else if (val > 6)
        return 0;
      else if (val > t->tm_wday)
        return adjday(t, '+', val - t->tm_wday);
  }
  return 1;
}

static int
sadjday(struct tm *t, char *arg)
{
  switch (*arg) {
    case '+':
    case '-':
      return adjday(t, *arg, atoi(arg+1));
    default:
      return adjday(t, '\0', atoi(arg));
  }
}

const struct vary *
vary_apply(const struct vary *v, struct tm *t)
{
  for (; v; v = v->next) {
    switch (v->flag) {
      case 'D':
        if (!sadjday(t, v->arg))
          return v;
        break;
      case 'W':
        if (!sadjwday(t, v->arg))
          return v;
        break;
      case 'M':
        if (!sadjmon(t, v->arg))
          return v;
        break;
      case 'Y':
        if (!sadjyear(t, v->arg))
          return v;
        break;
      default:
        return v;
    }
  }
  return 0;
}

void
vary_destroy(struct vary *v)
{
  struct vary *n;

  while (v) {
    n = v->next;
    free(v);
    v = n;
  }
}
