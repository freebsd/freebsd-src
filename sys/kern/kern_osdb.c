#include <sys/param.h>
#include <sys/module.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/sysproto.h>
#include <sys/sysent.h>

#include <compat/freebsd32/freebsd32_proto.h>

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
int sys_osdb_column_count(struct thread *td, struct osdb_column_count_args *args) { return 1; }
int sys_osdb_column_name(struct thread *td, struct osdb_column_name_args *args) { return 1; }
int sys_osdb_sample(struct thread *td, struct osdb_sample_args *args) { return 1; }
int sys_osdb_snapshot_clear(struct thread *td, struct osdb_snapshot_clear_args *args) { return 1; }

int sys_osdb_vtable_create(struct thread *td, struct osdb_vtable_create_args *args) { return 1; }
int sys_osdb_vtable_connect(struct thread *td, struct osdb_vtable_connect_args *args) { return 1; }
int sys_osdb_vtable_bestindex(struct thread *td, struct osdb_vtable_bestindex_args *args) { return 1; }
int sys_osdb_vtable_disconnect(struct thread *td, struct osdb_vtable_disconnect_args *args) { return 1; }
int sys_osdb_vtable_destroy(struct thread *td, struct osdb_vtable_destroy_args *args) { return 1; }
int sys_osdb_vtable_open(struct thread *td, struct osdb_vtable_open_args *args) { return 1; }
int sys_osdb_vtable_close(struct thread *td, struct osdb_vtable_close_args *args) { return 1; }
int sys_osdb_vtable_filter(struct thread *td, struct osdb_vtable_filter_args *args) { return 1; }
int sys_osdb_vtable_next(struct thread *td, struct osdb_vtable_next_args *args) { return 1; }
int sys_osdb_vtable_eof(struct thread *td, struct osdb_vtable_eof_args *args) { return 1; }
int sys_osdb_vtable_column(struct thread *td, struct osdb_vtable_column_args *args) { return 1; }
int sys_osdb_vtable_rowid(struct thread *td, struct osdb_vtable_rowid_args *args) { return 1; }
int sys_osdb_vtable_update(struct thread *td, struct osdb_vtable_update_args *args) { return 1; }


int freebsd32_osdb_prepare_v2(struct thread *td, struct freebsd32_osdb_prepare_v2_args *args) { return 1; }
//int freebsd32_osdb_step(struct thread *td, struct freebsd32_osdb_step_args *args) { return 1; }
//int freebsd32_osdb_finalize(struct thread *td, struct freebsd32_osdb_finalize_args *args) { return 1; }
//int freebsd32_osdb_column_blob(struct thread *td, struct freebsd32_osdb_column_blob_args *args) { return 1; }
//int freebsd32_osdb_column_double(struct thread *td, struct freebsd32_osdb_column_double_args *args) { return 1; }
//int freebsd32_osdb_column_int(struct thread *td, struct freebsd32_osdb_column_int_args *args) { return 1; }
int freebsd32_osdb_column_int64(struct thread *td, struct freebsd32_osdb_column_int64_args *args) { return 1; }
//int freebsd32_osdb_column_text(struct thread *td, struct freebsd32_osdb_column_text_args *args) { return 1; }
//int freebsd32_osdb_column_text16(struct thread *td, struct freebsd32_osdb_column_text16_args *args) { return 1; }
//int freebsd32_osdb_column_value(struct thread *td, struct freebsd32_osdb_column_value_args *args) { return 1; }
//int freebsd32_osdb_column_bytes(struct thread *td, struct freebsd32_osdb_column_bytes_args *args) { return 1; }
//int freebsd32_osdb_column_bytes16(struct thread *td, struct freebsd32_osdb_column_bytes16_args *args) { return 1; }
//int freebsd32_osdb_column_type(struct thread *td, struct freebsd32_osdb_column_type_args *args) { return 1; }
//int freebsd32_osdb_column_count(struct thread *td, struct freebsd32_osdb_column_count_args *args) { return 1; }
//int freebsd32_osdb_column_name(struct thread *td, struct freebsd32_osdb_column_name_args *args) { return 1; }

