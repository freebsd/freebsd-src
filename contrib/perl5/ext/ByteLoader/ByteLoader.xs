#define PERL_NO_GET_CONTEXT
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"
#include "byterun.h"

static int
xgetc(PerlIO *io)
{
    dTHX;
    return PerlIO_getc(io);
}

static int
xfread(char *buf, size_t size, size_t n, PerlIO *io)
{
    dTHX;
    int i = PerlIO_read(io, buf, n * size);
    if (i > 0)
	i /= size;
    return i;
}

static void
freadpv(U32 len, void *data, XPV *pv)
{
    dTHX;
    New(666, pv->xpv_pv, len, char);
    PerlIO_read((PerlIO*)data, (void*)pv->xpv_pv, len);
    pv->xpv_len = len;
    pv->xpv_cur = len - 1;
}

static I32
byteloader_filter(pTHXo_ int idx, SV *buf_sv, int maxlen)
{
    dTHR;
    OP *saveroot = PL_main_root;
    OP *savestart = PL_main_start;
    struct bytestream bs;

    bs.data = PL_rsfp;
    bs.pfgetc = (int(*) (void*))xgetc;
    bs.pfread = (int(*) (char*,size_t,size_t,void*))xfread;
    bs.pfreadpv = freadpv;

    byterun(aTHXo_ bs);

    if (PL_in_eval) {
        OP *o;

        PL_eval_start = PL_main_start;

        o = newSVOP(OP_CONST, 0, newSViv(1));
        PL_eval_root = newLISTOP(OP_LINESEQ, 0, PL_main_root, o);
        PL_main_root->op_next = o;
        PL_eval_root = newUNOP(OP_LEAVEEVAL, 0, PL_eval_root);
        o->op_next = PL_eval_root;
    
        PL_main_root = saveroot;
        PL_main_start = savestart;
    }

    return 0;
}

MODULE = ByteLoader		PACKAGE = ByteLoader

PROTOTYPES:	ENABLE

void
import(...)
  PPCODE:
    filter_add(byteloader_filter, NULL);

void
unimport(...)
  PPCODE:
    filter_del(byteloader_filter);
