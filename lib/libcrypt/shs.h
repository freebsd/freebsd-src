
typedef struct {
  unsigned long H[5];
  unsigned long W[80];
  int lenW;
  unsigned long sizeHi,sizeLo;
} SHS_CTX;


void shsInit(SHS_CTX *ctx);
void shsUpdate(SHS_CTX *ctx, const unsigned char *dataIn, int len);
void shsFinal(SHS_CTX *ctx, unsigned char hashOut[20]);
void shsBlock(const unsigned char *dataIn, int len, unsigned char hashOut[20]);

