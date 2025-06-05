/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
#if defined(_WIN32)
/* Windows doesn't have the concept of a system log, so just
** do nothing here.
*/
void
syslog(int pri, const char *fmt, ...)
{
    return;
}
#endif
