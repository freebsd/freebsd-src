
struct utimbuf {
  time_t actime;
  time_t modtime;
};

int utime (char *, struct utimbuf *);
