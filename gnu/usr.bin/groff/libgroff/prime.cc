extern "C" {
#include <math.h>
}

int is_prime(unsigned n)
{
  if (n <= 3)
    return 1;
  if (!(n & 1))
    return 0;
  if (n % 3 == 0)
    return 0;
  unsigned lim = unsigned(sqrt((double)n));
  unsigned d = 5;
  for (;;) {
    if (d > lim)
      break;
    if (n % d == 0)
      return 0;
    d += 2;
    if (d > lim)
      break;
    if (n % d == 0)
      return 0;
    d += 4;
  }
  return 1;
}
