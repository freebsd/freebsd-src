#include "as.h"
#include "emul.h"

static const char *criself_bfd_name PARAMS ((void));

static const char *
criself_bfd_name ()
{
  abort ();
  return NULL;
}

#define emul_bfd_name	criself_bfd_name
#define emul_format	&elf_format_ops

#define emul_name	"criself"
#define emul_struct_name criself
#define emul_default_endian 0
#include "emul-target.h"
