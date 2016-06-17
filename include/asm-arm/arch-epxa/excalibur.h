#include <linux/config.h>
#ifdef CONFIG_EPXA10DB_R2
#include "exc-epxa10dbr2.h"
#elif defined CONFIG_EPXA10DB_R3
#include "exc-epxa10dbr3.h"
#elif defined CONFIG_EPXA1DB
#include "exc-epxa1db.h"
#else
#error "must select an EPXA development board"
#endif
