#ifdef KR_headers
extern double sqrt();
double f__cabs(real, imag) double real, imag;
#else
#undef abs
#include "math.h"
double f__cabs(double real, double imag)
#endif
{
struct {double x, y;} z;
z.x = real;
z.y = imag;
return cabs(z);
}
