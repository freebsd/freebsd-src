#include <sys/param.h>
#include <sys/module.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/sysproto.h>
#include <sys/sysent.h>

/* Return positive values to indicate failure */
int sys_osdb_exec(struct thread *td, struct osdb_exec_args *args) { return 1; }
int sys_osdb_prepare_v2(struct thread *td, struct osdb_prepare_v2_args *args) { return 1; }
int sys_osdb_step(struct thread *td, struct osdb_step_args *args) { return 1; }
int sys_osdb_finalize(struct thread *td, struct osdb_finalize_args *args) { return 1; }
int sys_osdb_column_blob(struct thread *td, struct osdb_column_blob_args *args) { return 1; }
int sys_osdb_column_double(struct thread *td, struct osdb_column_double_args *args) { return 1; }
int sys_osdb_column_int(struct thread *td, struct osdb_column_int_args *args) { return 1; }
int sys_osdb_column_int64(struct thread *td, struct osdb_column_int64_args *args) { return 1; }
int sys_osdb_column_text(struct thread *td, struct osdb_column_text_args *args) { return 1; }
int sys_osdb_column_text16(struct thread *td, struct osdb_column_text16_args *args) { return 1; }
int sys_osdb_column_value(struct thread *td, struct osdb_column_value_args *args) { return 1; }
int sys_osdb_column_bytes(struct thread *td, struct osdb_column_bytes_args *args) { return 1; }
int sys_osdb_column_bytes16(struct thread *td, struct osdb_column_bytes16_args *args) { return 1; }
int sys_osdb_column_type(struct thread *td, struct osdb_column_type_args *args) { return 1; }

  
