/*
 * Mobile App Framework - Userspace Launcher
 *
 * Lightweight userspace daemon/CLI for managing mobile applications with 
 * sandboxing, lifecycle management, and execution.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/jail.h>
#include <sys/wait.h>
#include <errno.h>

static int launch_app(const char *pkg)
{
    char path[256];
    
    /* Map package ID to executable path */
    if (strcmp(pkg, "com.uos.settings") == 0) {
        strcpy(path, "/mobile/apps/settings/settings");
    } else if (strcmp(pkg, "com.uos.terminal") == 0) {
        strcpy(path, "/mobile/apps/terminal/terminal");
    } else if (strcmp(pkg, "com.uos.calculator") == 0) {
        strcpy(path, "/mobile/apps/calculator/calculator");
    } else if (strcmp(pkg, "com.uos.notes") == 0) {
        strcpy(path, "/mobile/apps/notes/notes");
    } else if (strcmp(pkg, "com.uos.browser") == 0) {
        strcpy(path, "/mobile/apps/browser/browser");
    } else if (strcmp(pkg, "com.uos.contacts") == 0) {
        strcpy(path, "/mobile/apps/contacts/contacts");
    } else if (strcmp(pkg, "com.uos.messages") == 0) {
        strcpy(path, "/mobile/apps/messages/messages");
    } else {
        /* Fallback for external apps */
        snprintf(path, sizeof(path), "/mobile/apps/%s/%s", pkg, pkg);
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return -1;
    }
    
    if (pid == 0) {
        /* Child process: Set up sandbox and execute */
        
        /* 
         * Attempt to create a lightweight jail sandbox. 
         * If this fails (e.g. lack of privileges in virtenv), we fall back 
         * to direct execution so the OS still functions on low-resource envs.
         */
        struct jail j;
        memset(&j, 0, sizeof(j));
        j.version = 0;
        j.path = "/";
        j.hostname = (char *)pkg;
        j.jailname = (char *)pkg;
        
        if (jail(&j) < 0) {
            fprintf(stderr, "Warning: Could not jail '%s' (errno %d), running unjailed.\n", pkg, errno);
        }
        
        char *args[] = { path, NULL };
        execv(path, args);
        
        /* If execv fails */
        perror("execv");
        exit(1);
    }
    
    /* Parent returns immediately */
    printf("Launched '%s' with PID %d\n", pkg, (int)pid);
    return 0;
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        printf("Usage: mobile_app_start <package_id>\n");
        return 1;
    }
    
    const char *pkg = argv[1];
    
    if (launch_app(pkg) < 0) {
        return 1;
    }
    
    return 0;
}