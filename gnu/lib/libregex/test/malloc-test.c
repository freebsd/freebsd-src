

typedef struct {
   unsigned *bits;
   unsigned size;
} bits_list_type;

#define BYTEWIDTH  8
#define NULL 0

#define BITS_BLOCK_SIZE (sizeof (unsigned) * BYTEWIDTH)
#define BITS_BLOCK(position) ((position) / BITS_BLOCK_SIZE)
#define BITS_MASK(position) (1 << ((position) % BITS_BLOCK_SIZE))

static unsigned
init_bits_list (bits_list_ptr)
  bits_list_type *bits_list_ptr;
{
  bits_list_ptr->bits = NULL;
  bits_list_ptr->bits = (unsigned *) malloc (sizeof (unsigned));

  if (bits_list_ptr->bits == NULL)
    return 0;

  bits_list_ptr->bits[0] = (unsigned)0;
  bits_list_ptr->size = BITS_BLOCK_SIZE;

  return 1;
}


main()
{
  bits_list_type dummy;
  bits_list_type dummy_1;
  bits_list_type dummy_2;
  bits_list_type dummy_3;

  init_bits_list (&dummy);
printf("init 1\n");
  init_bits_list (&dummy_1);
printf("init 2\n");
  init_bits_list (&dummy_2);
printf("init 3\n");
  init_bits_list (&dummy_3);
printf("init 4\n");
}
