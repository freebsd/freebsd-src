#include <sys/param.h>
#include <sys/module.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/sysproto.h>
#include <sys/sysent.h>

/* Return positive values to indicate failure */
int sys_dbq_prepare_v2(struct thread *td, struct dbq_prepare_v2_args *args) { return 1; }
int sys_dbq_step(struct thread *td, struct dbq_step_args *args) { return 1; }
int sys_dbqSafetyCheckOk(struct thread *td, struct dbqSafetyCheckOk_args *args) { return 1; }
int sys_dbq_mutex_enter(struct thread *td, struct dbq_mutex_enter_args *args) { return 1; }
int sys_dbqError(struct thread *td, struct dbqError_args *args) { return 1; }
int sys_dbq_column_count(struct thread *td, struct dbq_column_count_args *args) { return 1; }
int sys_dbqDbMallocRaw(struct thread *td, struct dbqDbMallocRaw_args *args) { return 1; }
int sys_dbq_column_name(struct thread *td, struct dbq_column_name_args *args) { return 1; }
int sys_dbq_column_text(struct thread *td, struct dbq_column_text_args *args) { return 1; }
int sys_dbq_column_type(struct thread *td, struct dbq_column_type_args *args) { return 1; }
int sys_dbqOomFault(struct thread *td, struct dbqOomFault_args *args) { return 1; }
int sys_dbqVdbeFinalize(struct thread *td, struct dbqVdbeFinalize_args *args) { return 1; }
int sys_dbqIsspace(struct thread *td, struct dbqIsspace_args *args) { return 1; }
int sys_dbqDbFree(struct thread *td, struct dbqDbFree_args *args) { return 1; }
int sys_dbqApiExit(struct thread *td, struct dbqApiExit_args *args) { return 1; }
int sys_dbqDbStrDup(struct thread *td, struct dbqDbStrDup_args *args) { return 1; }
int sys_dbqAssert(struct thread *td, struct dbqAssert_args *args) { return 1; }
int sys_dbq_mutex_leave(struct thread *td, struct dbq_mutex_leave_args *args) { return 1; }