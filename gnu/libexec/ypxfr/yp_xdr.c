#include <rpc/rpc.h>
#include "yp.h"
#ifndef lint
/*static char sccsid[] = "from: @(#)yp.x	2.1 88/08/01 4.0 RPCSRC";*/
static char rcsid[] = "yp.x,v 1.1 1994/08/04 19:01:55 wollman Exp";
#endif /* not lint */

extern void Perror __P((const char *, ...));

struct {
        union {
        int (*encoder)(char *, int, char **, int *, char **, int *);
        int (*decoder)(int, char *, int, char *, int, char *);
        } foreach;
        char *data;
} *xdr_ypall_callback;

bool_t           
__xdr_ypstat(XDR *xdrs, ypstat *objp)
{
 
         if (!xdr_enum(xdrs, (enum_t *)objp)) {
                 return (FALSE);
         }
        return (TRUE);
}

bool_t           
__xdr_valdat(XDR *xdrs, valdat *objp)
{
 
         if (!xdr_bytes(xdrs, (char **)&objp->valdat_val, (u_int *)&objp->valdat_len, YPMAXRECORD)) {
                 return (FALSE);
         }
        return (TRUE);
}        

bool_t         
__xdr_keydat(XDR *xdrs, keydat *objp)
{
 
         if (!xdr_bytes(xdrs, (char **)&objp->keydat_val, (u_int *)&objp->keydat_len, YPMAXRECORD)) {
                 return (FALSE);
         }
        return (TRUE);
} 

bool_t
__xdr_ypresp_key_val(XDR *xdrs, ypresp_key_val *objp)
{
 
         if (!__xdr_ypstat(xdrs, &objp->stat)) {
                 return (FALSE);
         }
         if (!__xdr_valdat(xdrs, &objp->val)) {
                 return (FALSE);
         }
         if (!__xdr_keydat(xdrs, &objp->key)) {
                 return (FALSE);
         }
        return (TRUE);
}
bool_t
__xdr_ypresp_all(XDR *xdrs, ypresp_all *objp)
{
   int CallAgain = 0;
   if (xdrs->x_op == XDR_DECODE) {
      while(1) {
#if 1
         int s=objp->ypresp_all_u.val.stat;
         bzero((char *)objp, sizeof (*objp));
         objp->ypresp_all_u.val.stat=s;
#endif
         if (!xdr_bool(xdrs, &objp->more)) {
            return (FALSE);
         }
         switch (objp->more) {
         case TRUE:
            if (!__xdr_ypresp_key_val(xdrs, &objp->ypresp_all_u.val)) {
               return (FALSE);
            }
            if (CallAgain==0) {
               CallAgain=(*(xdr_ypall_callback->foreach.decoder))(
               objp->ypresp_all_u.val.stat,
               objp->ypresp_all_u.val.key.keydat_val,
               objp->ypresp_all_u.val.key.keydat_len,
               objp->ypresp_all_u.val.val.valdat_val,
               objp->ypresp_all_u.val.val.valdat_len,
               xdr_ypall_callback->data);
            }
            break;
         case FALSE:
            return TRUE;
         }
#if 0
         xdrs->x_op=XDR_FREE;
         if (!__xdr_ypresp_all(xdrs, objp)) {
            return FALSE;
         }
         xdrs->x_op=XDR_DECODE;
#else
         xdr_free(__xdr_ypresp_all, (char *)objp);
#endif
      }
   } else if (xdrs->x_op == XDR_ENCODE) {
      while(1) {
         if (!xdr_bool(xdrs, &(objp->more))) {
            return (FALSE);
         }
         if (!__xdr_ypresp_key_val(xdrs, &objp->ypresp_all_u.val)) {
            Perror("__xdr_ypresp_key_val failed\n");
            return (FALSE);
         }
         if (objp->ypresp_all_u.val.stat!=YP_TRUE) {
            objp->more=FALSE;
            if (!xdr_bool(xdrs, &(objp->more))) {
               return (FALSE);
            }
            return(TRUE);
         }
         objp->ypresp_all_u.val.stat =
            (enum ypstat)(*(xdr_ypall_callback->foreach.encoder))(
            objp->ypresp_all_u.val.key.keydat_val,
            objp->ypresp_all_u.val.key.keydat_len,
            &(objp->ypresp_all_u.val.key.keydat_val),
            &(objp->ypresp_all_u.val.key.keydat_len),
            &(objp->ypresp_all_u.val.val.valdat_val),
            &(objp->ypresp_all_u.val.val.valdat_len));
      }
   } else {
#if 0
        bool_t more=objp->more;
        if (more==TRUE) {
           if (!xdr_bool(xdrs, &objp->more)) {
              return FALSE;
           }
           if (!__xdr_ypresp_key_val(xdrs, &objp->ypresp_all_u.val)) {
              return (FALSE);
           }
        }
#endif
           
      return(TRUE);
   }
}
