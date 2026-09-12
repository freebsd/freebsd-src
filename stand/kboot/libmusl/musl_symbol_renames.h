/* Rename musl internals that collide with libsa's public loader APIs. */
#define close kboot_musl_close
#define ioctl kboot_musl_ioctl
