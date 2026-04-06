/*
 * System Call Security Implementation
 * uOS(m) - User OS Mobile
 */

#include "syscall_security.h"
#include "task.h"

#define MAX_SYSCALLS 256

/* Syscall permission table */
static uint8_t syscall_permissions[MAX_SYSCALLS];

/* Current security context */
static security_context_t current_context;

/* Initialize syscall security */
void syscall_security_init(void) {
    /* Set default permissions - most syscalls unrestricted for now */
    for (int i = 0; i < MAX_SYSCALLS; i++) {
        syscall_permissions[i] = SYSCALL_LEVEL_UNRESTRICTED;
    }

    /* Set privileged syscalls */
    syscall_permissions[1] = SYSCALL_LEVEL_PRIVILEGED;  /* exit - needs cleanup */
    syscall_permissions[39] = SYSCALL_LEVEL_PRIVILEGED; /* getpid */
    syscall_permissions[110] = SYSCALL_LEVEL_PRIVILEGED; /* getppid */
    syscall_permissions[102] = SYSCALL_LEVEL_PRIVILEGED; /* getuid */
    syscall_permissions[107] = SYSCALL_LEVEL_PRIVILEGED; /* getgid */

    /* Initialize default context (root) */
    current_context.uid = 0;
    current_context.euid = 0;
    current_context.gid = 0;
    current_context.egid = 0;
    current_context.security_level = 0;
    current_context.capabilities = 0xFFFFFFFFFFFFFFFFULL; /* All capabilities for root */
}

/* Check if syscall is allowed for current context */
int syscall_check_permission(uint64_t syscall_num, security_context_t *ctx) {
    if (syscall_num >= MAX_SYSCALLS) {
        return 0; /* Deny unknown syscalls */
    }

    uint8_t required_level = syscall_permissions[syscall_num];

    /* Root (uid 0) can do anything */
    if (ctx->uid == 0) {
        return 1;
    }

    /* Check security level */
    if (ctx->security_level > required_level) {
        return 0;
    }

    /* Check capabilities for privileged operations */
    if (required_level == SYSCALL_LEVEL_PRIVILEGED) {
        /* For now, allow if not root - can be extended with capability checks */
        return 0;
    }

    return 1;
}

/* Set security context for current task */
void syscall_set_context(security_context_t *ctx) {
    if (ctx) {
        current_context = *ctx;
    }
}

/* Get current security context */
security_context_t *syscall_get_context(void) {
    return &current_context;
}