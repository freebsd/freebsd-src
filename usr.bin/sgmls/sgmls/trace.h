/* TRACE.H: Declarations for internal trace functions. */

#ifdef TRACE

/* Trace variables.
*/
extern int  trace;            /* Switch: 1=trace state transitions; 0=don't. */
extern int atrace;            /* Switch: 1=trace attribute activity; 0=don't. */
extern int ctrace;            /* Switch: 1=trace context checking; 0=don't. */
extern int dtrace;            /* Switch: 1=trace declaration parsing; 0=don't.*/
extern int etrace;            /* Switch: 1=trace entity activity; 0=don't.*/
extern int gtrace;            /* Switch: 1=trace group creations; 0=don't. */
extern int itrace;            /* Switch: 1=trace ID activity; 0=don't. */
extern int mtrace;            /* Switch: 1=trace MS activity; 0=don't. */
extern int ntrace;            /* Switch: 1=trace data notation activity. */
extern char emd[];            /* For "EMD" parameter type in dtrace calls. */

VOID traceadl P((struct ad *));
VOID tracecon P((int,int,int,struct parse *,int,int));
VOID tracedcn P((struct dcncb *));
VOID tracedsk P((struct tag *,struct tag *,int,int));
VOID traceecb P((char *,struct entity *));
VOID traceend P((char *,struct thdr *,struct mpos *,int,int));
VOID traceesn P((struct ne *));
VOID traceetd P((struct etd *));
VOID traceetg P((struct tag *,struct etd *,int,int));
VOID tracegi P((char *,struct etd *,struct thdr *,struct mpos *));
VOID tracegml P((struct restate *,int,int,int));
VOID tracegrp P((struct etd **));
VOID traceid P((char *,struct id *));
VOID tracemd P((char *));
VOID tracemod P((struct thdr *));
VOID tracems P((int,int,int,int));
VOID tracengr P((struct dcncb **));
VOID tracepcb P((struct parse *));
VOID tracepro P((void));
VOID traceset P((void));
VOID tracesrm P((char *,struct entity **,UNCH *));
VOID tracestg P((struct etd *,int,int,struct etd *,int));
VOID tracestk P((struct tag *,int,int));
VOID tracetkn P((int,UNCH *));
VOID traceval P((struct parse *,unsigned int,UNCH *,int));

#define TRACEADL(al) ((void)(atrace && (traceadl(al), 1)))
#define TRACECON(etagimct, dostag, datarc, pcb, conrefsw, didreq) \
  ((void)(gtrace \
          && (tracecon(etagimct, dostag, datarc, pcb, conrefsw, didreq), 1)))
#define TRACEDCN(dcn) ((void)(ntrace && (tracedcn(dcn), 1)))
#define TRACEDSK(pts, ptso, ts3, etictr) \
  ((void)(gtrace && (tracedsk(pts, ptso, ts3, etictr), 1)))
#define TRACEECB(action, p) \
  ((void)(etrace && (traceecb(action, p), 1)))
#define TRACEEND(stagenm, mod, pos, rc, opt) \
  ((void)(ctrace && (traceend(stagenm, mod, pos, rc, opt), 1)))
#define TRACEESN(p) \
  ((void)((etrace || atrace || ntrace) && (traceesn(p), 1)))
#define TRACEETD(p) ((void)(gtrace && (traceetd(p), 1)))
#define TRACEETG(pts, curetd, tsl, etagimct) \
  ((void)(gtrace && (traceetg(pts, curetd, tsl, etagimct), 1)))
#define TRACEGI(stagenm, gi, mod, pos) \
  ((void)(ctrace && (tracegi(stagenm, gi, mod, pos), 1)))
#define TRACEGML(scb, pss, conactsw, conact) \
  ((void)(trace && (tracegml(scb, pss, conactsw, conact), 1)))
#define TRACEGRP(p) ((void)(gtrace && (tracegrp(p), 1)))
#define TRACEID(action, p) ((void)(itrace && (traceid(action, p), 1)))
#define TRACEMD(p) ((void)(dtrace && (tracemd(p), 1)))
#define TRACEMOD(p) ((void)(gtrace && (tracemod(p), 1)))
#define TRACEMS(action, code, mslevel, msplevel) \
  ((void)(mtrace && (tracems(action, code, mslevel, msplevel), 1)))
#define TRACENGR(p) ((void)(gtrace && (tracengr(p), 1)))
#define TRACEPCB(p) ((void)(trace && (tracepcb(p), 1)))
#define TRACEPRO() (tracepro())
#define TRACESET() (traceset())
#define TRACESRM(action, pg, gi) \
  ((void)(etrace && (tracesrm(action, pg, gi), 1)))
#define TRACESTG(curetd, dataret, rc, nextetd, mexts) \
  ((void)(gtrace && (tracestg(curetd, dataret, rc, nextetd, mexts), 1)))
#define TRACESTK(pts, ts2, etictr) \
  ((void)(gtrace && (tracestk(pts, ts2, etictr), 1)))
#define TRACETKN(scope, lextoke) \
  ((void)(trace && (tracetkn(scope, lextoke), 1)))
#define TRACEVAL(pcb, atype, aval, tokencnt) \
  ((void)(atrace && (traceval(pcb, atype, aval, tokencnt), 1)))

#else /* not TRACE */

#define TRACEADL(al) /* empty */
#define TRACECON(etagimct, dostag, datarc, pcb, conrefsw, didreq) /* empty */
#define TRACEDCN(dcn) /* empty */
#define TRACEDSK(pts, ptso, ts3, etictr) /* empty */
#define TRACEECB(action, p) /* empty */
#define TRACEEND(stagenm, mod, pos, rc, opt) /* empty */
#define TRACEESN(p) /* empty */
#define TRACEETG(pts, curetd, tsl, etagimct) /* empty */
#define TRACEETD(p) /* empty */
#define TRACEGI(stagenm, gi, mod, pos) /* empty */
#define TRACEGML(scb, pss, conactsw, conact) /* empty */
#define TRACEGRP(p) /* empty */
#define TRACEID(action, p) /* empty */
#define TRACEMD(p) /* empty */
#define TRACEMOD(p) /* empty */
#define TRACEMS(action, code, mslevel, msplevel) /* empty */
#define TRACENGR(p) /* empty */
#define TRACEPCB(p) /* empty */
#define TRACEPRO() /* empty */
#define TRACESET() /* empty */
#define TRACESRM(action, pg, gi) /* empty */
#define TRACESTG(curetd, dataret, rc, nextetd, mexts) /* empty */
#define TRACESTK(pts, ts2, etictr) /* empty */
#define TRACETKN(scope, lextoke) /* empty */
#define TRACEVAL(pcb, atype, aval, tokencnt) /* empty */

#endif /* not TRACE */
