#ifndef __libgxx_dirent_h

#include <_G_config.h>

#if !_G_HAVE_DIRENT
#define __libgxx_dirent_h
#define direct dirent
#include <sys/dir.h>
#else

extern "C" {

#ifdef __dirent_h_recursive
#include_next <dirent.h>
#else
// Note: sys/dir.h checks __dirent_h_recursive
#define __dirent_h_recursive
#define opendir __hide_opendir
#define closedir __hide_closedir
#define readdir __hide_readdir
#define telldir __hide_telldir
#define seekdir __hide_seekdir

#include_next <dirent.h>

#define __libgxx_dirent_h
#undef opendir
#undef closedir
#undef readdir
#undef telldir
#undef seekdir

DIR *opendir(const char *);
int closedir(DIR *);
struct dirent *readdir(DIR *);
long telldir(DIR *);
void seekdir(DIR *, long);
// We don't bother with rewinddir (many systems define it as a macro).
// void rewinddir(DIR *);
#endif
}

#endif
#endif
