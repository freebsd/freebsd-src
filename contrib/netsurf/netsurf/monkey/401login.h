
#include <stdbool.h>

#include "utils/nsurl.h"
#include "utils/errors.h"


void gui_401login_open(nsurl *url, const char *realm,
                       nserror (*cb)(bool proceed, void *pw), void *cbpw);
