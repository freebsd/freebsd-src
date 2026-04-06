/*
 * System Call Security and Filtering
 * uOS(m) - User OS Mobile
 */

#ifndef _SYSCALL_SECURITY_H_
#define _SYSCALL_SECURITY_H_

#include <stdint.h>

/* System call security levels */
#define SYSCALL_LEVEL_UNRESTRICTED  0
#define SYSCALL_LEVEL_RESTRICTED    1
#define SYSCALL_LEVEL_PRIVILEGED    2
#define SYSCALL_LEVEL_DENIED        3

/* Security context for processes */
typedef struct {
    uint32_t uid;
    uint32_t euid;
    uint32_t gid;
    uint32_t egid;
    uint32_t security_level;
    uint64_t capabilities;
} security_context_t;

/* Initialize syscall security */
void syscall_security_init(void);

/* Check if syscall is allowed for current context */
int syscall_check_permission(uint64_t syscall_num, security_context_t *ctx);

/* Set security context for current task */
void syscall_set_context(security_context_t *ctx);

/* Get current security context */
security_context_t *syscall_get_context(void);

#endif /* _SYSCALL_SECURITY_H_ */