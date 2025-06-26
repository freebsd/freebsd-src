#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/resource.h> // For getrusage

#define BUFFER_SIZE (10 * 1024 * 1024) // 10 MB
#define PAGE_SIZE 4096

int main() {
    pid_t pid = getpid();
    printf("Fault Generator PID: %d\n", pid);

    char *buffer = (char *)malloc(BUFFER_SIZE);
    if (buffer == NULL) {
        perror("Failed to allocate buffer");
        return 1;
    }

    printf("Buffer allocated. Touching pages to generate minor faults...\n");
    for (size_t i = 0; i < BUFFER_SIZE; i += PAGE_SIZE) {
        buffer[i] = (char)(i % 256); // Write to each page
    }
    printf("Finished touching pages.\n");

    struct rusage usage_self;
    if (getrusage(RUSAGE_SELF, &usage_self) == 0) {
        printf("getrusage(SELF): minflt=%ld, majflt=%ld\n",
               usage_self.ru_minflt, usage_self.ru_majflt);
    } else {
        perror("getrusage failed");
    }

    printf("Looping indefinitely. Use 'procstat -f %d' or sysctl to check fault counts.\n", pid);
    printf("Press Ctrl+C to exit.\n");

    while (1) {
        sleep(10);
        // Optionally, re-touch pages or do other memory operations here
        // to generate more faults over time if desired for continuous testing.
        // For now, most faults are generated at the start.
        if (getrusage(RUSAGE_SELF, &usage_self) == 0) {
            printf("Internal getrusage(SELF) update: minflt=%ld, majflt=%ld\n",
                   usage_self.ru_minflt, usage_self.ru_majflt);
        }
    }

    free(buffer); // Should not be reached in normal Ctrl+C exit
    return 0;
}
