/*-
 * This file is in the public domain.
 */

typedef void (fwmethod)(int dev_fd, const char *filename, char ich, int count);
extern fwmethod dvrecv;
extern fwmethod dvsend;
extern fwmethod mpegtsrecv;
