#include <X11/Xlib.h>
#include <X11/Intrinsic.h>

char *xmalloc(int n)
{
    return XtMalloc(n);
}
