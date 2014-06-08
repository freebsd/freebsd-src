#include <stddef.h>
#include <libevdev/libevdev.h>

int main(void) {
	return libevdev_new_from_fd(0, NULL);
}
