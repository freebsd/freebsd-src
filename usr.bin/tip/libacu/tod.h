int tod_cmp (const struct timeval *a, const struct timeval *b);
int tod_lt (const struct timeval *a, const struct timeval *b) ;
int tod_gt (const struct timeval *a, const struct timeval *b);
int tod_lte (const struct timeval *a, const struct timeval *b);
int tod_gte (const struct timeval *a, const struct timeval *b);
int tod_eq (const struct timeval *a, const struct timeval *b);
void tod_addto (struct timeval *a, const struct timeval *b);
void tod_subfrom (struct timeval *a, struct timeval b);
void tod_gettime (struct timeval *tp);
