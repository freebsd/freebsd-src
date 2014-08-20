/*
 *   AmigaGuide Class
 *   A BOOPSI class for displaying AmigaGuide files.
 *   by Daniel "Trixie" Jedlicka
 */

#undef __USE_INLINE__

#include "amigaguide_class.h"



struct localObjectData
{
 struct NewAmigaGuide  nag;
 struct AmigaGuideMsg *agm;
 AMIGAGUIDECONTEXT     agHandle;
 uint32                agContextID;
 uint32                agSignal;
};

struct Library         *AmigaGuideBase = NULL;
struct AmigaGuideIFace *IAmigaGuide = NULL;


/* **********************************  function prototypes   ************************************ */

static uint32 dispatchAGClass(Class *, Object *, Msg);
BOOL          freeAGClass(Class *);

// class methods
uint32 om_new(Class *, Object *, struct opSet *);
uint32 om_dispose(Class *, Object *, Msg);
uint32 om_set(Class *, Object *, struct opSet *);
uint32 om_get(Class *, Object *, struct opGet *);
uint32 agm_open(Class *, Object *, Msg);
uint32 agm_close(Class *, Object *, Msg);
uint32 agm_process(Class *, Object *, Msg);


/* ***************************  class initialization and disposal   ***************************** */


Class *initAGClass(void)
{
 Class *cl = NULL;


 // Open amigaguide.library and its interface.
 if ( (AmigaGuideBase = IExec->OpenLibrary("amigaguide.library", 52)) )
  {
   if ( (IAmigaGuide  = (struct AmigaGuideIFace *)IExec->GetInterface(AmigaGuideBase, "main", 1L, NULL)) )
    {
     if ( (cl = IIntuition->MakeClass(NULL, "rootclass", NULL, sizeof(struct localObjectData), 0)) )
      {
       cl->cl_Dispatcher.h_Entry = (HOOKFUNC)dispatchAGClass;
       IIntuition->AddClass(cl);
      }
     else freeAGClass(NULL);
    }
   else freeAGClass(NULL);
  }

 return cl;

}



BOOL freeAGClass(Class *cl)
{
 BOOL retVal = FALSE;


 // Close amigaguide.library and free the class.
 if (IAmigaGuide)    IExec->DropInterface((struct Interface *)IAmigaGuide);
 if (AmigaGuideBase) IExec->CloseLibrary(AmigaGuideBase);
 if (cl)             retVal = IIntuition->FreeClass(cl);

 return retVal;
}



/* **************************************  class dispatcher  ************************************ */


static uint32 dispatchAGClass(Class *cl, Object *o, Msg msg)
{

 switch (msg->MethodID)
  {
   case OM_NEW:
     return om_new(cl, o, (struct opSet *)msg);

   case OM_DISPOSE:
     return om_dispose(cl, o, msg);

   case OM_UPDATE:
   case OM_SET:
     return om_set(cl, o, (struct opSet *)msg);

   case OM_GET:
     return om_get(cl, o, (struct opGet *)msg);

   case AGM_OPEN:
     return agm_open(cl, o, msg);

   case AGM_CLOSE:
     return agm_close(cl, o, msg);

   case AGM_PROCESS:
     return agm_process(cl, o, msg);

   default:
     return IIntuition->IDoSuperMethodA(cl, o, msg);
  }

}


/* ***************************************  class methods  ************************************** */

uint32 om_new(Class *cl, Object *o, struct opSet *msg)
{
 struct localObjectData *lod = NULL;
 uint32 retVal = 0L;


 if ( (retVal = IIntuition->IDoSuperMethodA(cl, o, (Msg)msg)) )
  {
   // Obtain pointer to our object's local instance data.
   if ( (lod = (struct localObjectData *)INST_DATA(cl, retVal)) )
    {
     // Initialize values.
     lod->agHandle          = NULL;
     lod->agContextID       = 0;
     lod->nag.nag_Name      = NULL;
     lod->nag.nag_Screen    = NULL;
     lod->nag.nag_PubScreen = NULL;
     lod->nag.nag_BaseName  = NULL;
     lod->nag.nag_Context   = NULL;
     lod->nag.nag_Client    = NULL; // private, must be NULL!

     // Set initial object attributes based on the tags from NewObject().
     om_set(cl, (Object *)retVal, msg);
    }
  }

 return retVal;

}





uint32 om_dispose(Class *cl, Object *o, Msg msg)
{

 // Close the document, should it still be opened.
 agm_close(cl, o, msg);

 // Let superclass dispose of the object.
 return IIntuition->IDoSuperMethodA(cl, o, msg);

}





uint32 om_set(Class *cl, Object *o, struct opSet *msg)
{
 struct localObjectData *lod = (struct localObjectData *)INST_DATA(cl, o);
 struct TagItem *ti = NULL, *tags = msg->ops_AttrList;
 uint32 retVal = 0L;
 

 while ((ti = IUtility->NextTagItem (&tags)))
  {
   switch (ti->ti_Tag)
    {
     case AMIGAGUIDE_Name:
       lod->nag.nag_Name = (STRPTR)ti->ti_Data;
       retVal++;
     break;

     case AMIGAGUIDE_Screen:
       lod->nag.nag_Screen = (struct Screen *)ti->ti_Data;
       retVal++;
     break;

     case AMIGAGUIDE_PubScreen:
       lod->nag.nag_PubScreen = (STRPTR)ti->ti_Data;
       retVal++;
     break;

     case AMIGAGUIDE_BaseName:
       lod->nag.nag_BaseName = (STRPTR)ti->ti_Data;
       retVal++;
     break;

     case AMIGAGUIDE_ContextArray:
       lod->nag.nag_Context = (STRPTR *)ti->ti_Data;
       retVal++;
     break;

     case AMIGAGUIDE_ContextID:
       lod->agContextID = (uint32)ti->ti_Data;
       retVal++;
     break;

     default:
     break;
    }
  }

 return retVal;

}





uint32 om_get(Class *cl, Object *o, struct opGet *msg)
{
 struct localObjectData *lod = (struct localObjectData *)INST_DATA(cl, o);
 uint32 retVal = 0L;


 switch (msg->opg_AttrID)
  {
   case AMIGAGUIDE_Name:
     *(msg->opg_Storage) = (uint32)lod->nag.nag_Name;
     retVal = 1L;
   break;

   case AMIGAGUIDE_Screen:
     *(msg->opg_Storage) = (uint32)lod->nag.nag_Screen;
     retVal = 1L;
   break;

   case AMIGAGUIDE_PubScreen:
     *(msg->opg_Storage) = (uint32)lod->nag.nag_PubScreen;
     retVal = 1L;
   break;

   case AMIGAGUIDE_BaseName:
     *(msg->opg_Storage) = (uint32)lod->nag.nag_BaseName;
     retVal = 1L;
   break;

   case AMIGAGUIDE_ContextArray:
     *(msg->opg_Storage) = (uint32)lod->nag.nag_Context;
     retVal = 1L;
   break;

   case AMIGAGUIDE_ContextID:
     *(msg->opg_Storage) = (uint32)lod->agContextID;
     retVal = 1L;
   break;

   case AMIGAGUIDE_Signal:
     *(msg->opg_Storage) = (uint32)lod->agSignal;
     retVal = 1L;
   break;

   default:
     retVal = IIntuition->IDoSuperMethodA(cl, o, (Msg)msg);
  }

 return retVal;

}





uint32 agm_open(Class *cl, Object *o, Msg msg)
{
 struct localObjectData *lod = (struct localObjectData *)INST_DATA(cl, o);
 BOOL   agActive = FALSE;
 uint32 retVal = 0L;


 // Close a previous instance.
 if ( lod->agHandle ) agm_close(cl, o, msg);

   // (Re)establish the AmigaGuide context and open the database asynchronously.
   if ( (lod->agHandle = IAmigaGuide->OpenAmigaGuideAsync(&(lod->nag), NULL)) )
    {
     if ( (lod->agSignal = IAmigaGuide->AmigaGuideSignal(lod->agHandle)) )
      {
       // Wait until the database is displayed and ready.
       IExec->Wait(lod->agSignal);
       while ( agActive == FALSE )
        {
         while ( (lod->agm = IAmigaGuide->GetAmigaGuideMsg(lod->agHandle)) )
          {
           // The AmigaGuide process started OK.
           if ( lod->agm->agm_Type == ActiveToolID ) agActive = TRUE;

           // Opening the guide file failed for some reason, continue as usual.
           if ( lod->agm->agm_Type == ToolStatusID && lod->agm->agm_Pri_Ret ) agActive = TRUE;

           IAmigaGuide->ReplyAmigaGuideMsg(lod->agm);
          }
        }
       if ( lod->nag.nag_Context )
        {
         // A context node array is provided = open the current context node.
         IAmigaGuide->SetAmigaGuideContext(lod->agHandle, lod->agContextID, NULL);
         retVal = IAmigaGuide->SendAmigaGuideContext(lod->agHandle, NULL);
        }
       else
        {
         // No context array is provided = open the main node.
         retVal = IAmigaGuide->SendAmigaGuideCmd(lod->agHandle, "LINK MAIN", TAG_DONE);
        }
      }
    }


 return retVal;
}





uint32 agm_close(Class *cl, Object *o, Msg msg)
{
 struct localObjectData *lod = (struct localObjectData *)INST_DATA(cl, o);
 uint32 retVal = 0L;


 if ( lod->agHandle )
  {
   IAmigaGuide->CloseAmigaGuide(lod->agHandle);
   lod->agHandle = NULL;
   lod->agSignal = 0;
   retVal = 1L;
  }

 return retVal;

}






uint32 agm_process(Class *cl, Object *o, Msg msg)
{
 struct localObjectData *lod = (struct localObjectData *)INST_DATA(cl, o);
 uint32 retVal = 0L;
 

 if (lod->agHandle)
  {
   while ( (lod->agm = IAmigaGuide->GetAmigaGuideMsg(lod->agHandle)) )
    {
     switch (lod->agm->agm_Type)
      {
       case ShutdownMsgID:
         agm_close(cl, o, msg);
         retVal = 1L;
       break;

       default:
         //printf("%d\n", lod->agm->agm_Type);
       break;
      }
     IAmigaGuide->ReplyAmigaGuideMsg(lod->agm);
    }
  }

 return retVal;
 
}

