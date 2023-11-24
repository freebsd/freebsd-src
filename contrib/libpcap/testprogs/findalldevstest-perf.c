#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#ifdef _WIN32
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #include <windows.h>
#else
  #include <unistd.h>
  #include <sys/resource.h>
#endif

#include <pcap.h>

#include "varattrs.h"
#include "pcap/funcattrs.h"
#include "portability.h"

int main(int argc _U_, char **argv _U_)
{
  pcap_if_t *alldevs;
  int exit_status = 0;
  char errbuf[PCAP_ERRBUF_SIZE+1];
#ifdef _WIN32
  FILETIME start_ktime, start_utime, end_ktime, end_utime;
  FILETIME dummy1, dummy2;
  ULARGE_INTEGER start_kticks, end_kticks, start_uticks, end_uticks;
  ULONGLONG ktime, utime, tottime;
#else
  struct rusage start_rusage, end_rusage;
  struct timeval ktime, utime, tottime;
#endif

#ifdef _WIN32
  if (!GetProcessTimes(GetCurrentProcess(), &dummy1, &dummy2,
                       &start_ktime, &start_utime))
  {
    fprintf(stderr, "GetProcessTimes() fails at start\n");
    exit(1);
  }
  start_kticks.LowPart = start_ktime.dwLowDateTime;
  start_kticks.HighPart = start_ktime.dwHighDateTime;
  start_uticks.LowPart = start_utime.dwLowDateTime;
  start_uticks.HighPart = start_utime.dwHighDateTime;
#else
  if (getrusage(RUSAGE_SELF, &start_rusage) == -1) {
    fprintf(stderr, "getrusage() fails at start\n");
    exit(1);
  }
#endif
  for (int i = 0; i < 500; i++)
  {
    if (pcap_findalldevs(&alldevs, errbuf) == -1)
    {
      fprintf(stderr,"Error in pcap_findalldevs: %s\n",errbuf);
      exit(1);
    }
    pcap_freealldevs(alldevs);
  }

#ifdef _WIN32
  if (!GetProcessTimes(GetCurrentProcess(), &dummy1, &dummy2,
                       &end_ktime, &end_utime))
  {
    fprintf(stderr, "GetProcessTimes() fails at end\n");
    exit(1);
  }
  end_kticks.LowPart = end_ktime.dwLowDateTime;
  end_kticks.HighPart = end_ktime.dwHighDateTime;
  end_uticks.LowPart = end_utime.dwLowDateTime;
  end_uticks.HighPart = end_utime.dwHighDateTime;
  ktime = end_kticks.QuadPart - start_kticks.QuadPart;
  utime = end_uticks.QuadPart - start_uticks.QuadPart;
  tottime = ktime + utime;
  printf("Total CPU secs: kernel %g, user %g, total %g\n",
      ((double)ktime) / 10000000.0,
      ((double)utime) / 10000000.0,
      ((double)tottime) / 10000000.0);
#else
  if (getrusage(RUSAGE_SELF, &end_rusage) == -1) {
    fprintf(stderr, "getrusage() fails at end\n");
    exit(1);
  }
  timersub(&end_rusage.ru_stime, &start_rusage.ru_stime, &ktime);
  timersub(&end_rusage.ru_utime, &start_rusage.ru_utime, &utime);
  timeradd(&ktime, &utime, &tottime);
  printf("Total CPU secs: kernel %g, user %g, total %g\n",
      (double)ktime.tv_sec + ((double)ktime.tv_usec / 1000000.0),
      (double)utime.tv_sec + ((double)utime.tv_usec / 1000000.0),
      (double)tottime.tv_sec + ((double)tottime.tv_usec / 1000000.0));
#endif
  exit(exit_status);
}
