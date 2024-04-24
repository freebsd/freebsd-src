#include <sys/param.h>
#include <sys/module.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/sysproto.h>
#include <sys/sysent.h>
#include <compat/freebsd32/freebsd32_proto.h>

int freebsd32_osdb_column_int64(struct thread *td, struct osdb_column_int64_args *args) { return 1; }

  
