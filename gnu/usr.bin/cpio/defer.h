struct deferment
  {
    struct deferment *next;
    struct new_cpio_header header;
  };

struct deferment *create_deferment P_((struct new_cpio_header *file_hdr));
void free_deferment P_((struct deferment *d));
