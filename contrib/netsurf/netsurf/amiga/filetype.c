/*
 * Copyright 2008, 2011 Chris Young <chris@unsatisfactorysoftware.co.uk>
 *
 * This file is part of NetSurf, http://www.netsurf-browser.org/
 *
 * NetSurf is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * NetSurf is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>
#include <string.h>
#include "amiga/filetype.h"
#include "amiga/object.h"
#include "content/fetch.h"
#include "content/content.h"
#include "utils/log.h"
#include "utils/utils.h"
#include <proto/icon.h>
#include <proto/dos.h>
#include <proto/datatypes.h>
#include <proto/exec.h>
#include <workbench/icon.h>

/**
 * filetype -- determine the MIME type of a local file
 */

struct MinList *ami_mime_list = NULL;

struct ami_mime_entry
{
	lwc_string *mimetype;
	lwc_string *datatype;
	lwc_string *filetype;
	lwc_string *plugincmd;
};

enum
{
	AMI_MIME_MIMETYPE,
	AMI_MIME_DATATYPE,
	AMI_MIME_FILETYPE,
	AMI_MIME_PLUGINCMD
};

const char *fetch_filetype(const char *unix_path)
{
	static char mimetype[50];
	struct DiskObject *dobj = NULL;
	struct DataType *dtn;
	BOOL found = FALSE;
	lwc_string *lwc_mimetype;

	/* First, check if we appear to have an icon.
	   We'll just do a filename check here for quickness, although the
	   first word ought to be checked against WB_DISKMAGIC really. */

	if(strncmp(unix_path + strlen(unix_path) - 5, ".info", 5) == 0) {
		strcpy(mimetype,"image/x-amiga-icon");
		found = TRUE;
	}


	/* Secondly try getting a tooltype "MIMETYPE" and use that as the MIME type.
	    Will fail over to default icons if the file doesn't have a real icon. */

	if(!found) {
		if(dobj = GetIconTags(unix_path,ICONGETA_FailIfUnavailable,FALSE,
						TAG_DONE)) {
			STRPTR ttype = NULL;
			ttype = FindToolType(dobj->do_ToolTypes, "MIMETYPE");
			if(ttype) {
				strcpy(mimetype,ttype);
				found = TRUE;
			}
			FreeDiskObject(dobj);
		}
	}

	/* If that didn't work, use the MIME file and DataTypes */

	if(!found) {
		BPTR lock;
		if (lock = Lock (unix_path, ACCESS_READ)) {
			if (dtn = ObtainDataTypeA (DTST_FILE, (APTR)lock, NULL)) {
				if(ami_mime_from_datatype(dtn, &lwc_mimetype, NULL)) {
					strcpy(mimetype, lwc_string_data(lwc_mimetype));
					found = TRUE;
					ReleaseDataType(dtn);
				}
			}
			UnLock(lock);
		}
	}

	/* Have a quick check for file extensions (inc RISC OS filetype).
	 * Makes detection a little more robust, and some of the redirects
	 * caused by links in the SVN tree prevent NetSurf from reading the
	 * MIME type from the icon (step two, above).
	 */

	if((!found) || (strcmp("text/plain", mimetype) == 0))
	{
		if((strncmp(unix_path + strlen(unix_path) - 4, ".css", 4) == 0) ||
			(strncmp(unix_path + strlen(unix_path) - 4, ",f79", 4) == 0))
		{
			strcpy(mimetype,"text/css");
			found = TRUE;
		}

		if((strncmp(unix_path + strlen(unix_path) - 4, ".htm", 4) == 0) ||
			(strncmp(unix_path + strlen(unix_path) - 5, ".html", 5) == 0) ||
			(strncmp(unix_path + strlen(unix_path) - 4, ",faf", 4) == 0))
		{
			strcpy(mimetype,"text/html");
			found = TRUE;
		}
		if(strncmp(unix_path + strlen(unix_path) - 3, ".js", 3) == 0) {
			strcpy(mimetype,"application/javascript");
			found = TRUE;
		}
	}

	if(!found) strcpy(mimetype,"text/plain"); /* If all else fails */

	return mimetype;
}

const char *ami_content_type_to_file_type(content_type type)
{
	switch(type)
	{
		case CONTENT_HTML:
			return "html";
		break;

		case CONTENT_TEXTPLAIN:
			return "ascii";
		break;

		case CONTENT_CSS:
			return "css";
		break;

		case CONTENT_IMAGE:
			return "picture";
		break;

		default:
			return "project";	
		break;
	}
}


nserror ami_mime_init(const char *mimefile)
{
	lwc_error lerror;
	char buffer[256];
	BPTR fh = 0;
	struct RDArgs *rargs = NULL;
	STRPTR template = "MIMETYPE/A,DT=DATATYPE/K,TYPE=DEFICON/K,CMD=PLUGINCMD/K";
	long rarray[] = {0,0,0,0};
	struct nsObject *node;
	struct ami_mime_entry *mimeentry;

	if(ami_mime_list == NULL)
		ami_mime_list = NewObjList();

	rargs = AllocDosObjectTags(DOS_RDARGS,TAG_DONE);
	if(rargs == NULL) return NSERROR_NOMEM;

	if(fh = FOpen(mimefile, MODE_OLDFILE, 0))
	{
		while(FGets(fh, (UBYTE *)&buffer, 256) != 0)
		{
			rargs->RDA_Source.CS_Buffer = (char *)&buffer;
			rargs->RDA_Source.CS_Length = 256;
			rargs->RDA_Source.CS_CurChr = 0;

			rargs->RDA_DAList = NULL;
			rargs->RDA_Buffer = NULL;
			rargs->RDA_BufSiz = 0;
			rargs->RDA_ExtHelp = NULL;
			rargs->RDA_Flags = 0;

			rarray[AMI_MIME_MIMETYPE] = 0;
			rarray[AMI_MIME_DATATYPE] = 0;
			rarray[AMI_MIME_FILETYPE] = 0;
			rarray[AMI_MIME_PLUGINCMD] = 0;

			if(ReadArgs(template, rarray, rargs))
			{
				node = AddObject(ami_mime_list, AMINS_MIME);
				mimeentry = AllocVecTags(sizeof(struct ami_mime_entry), AVT_ClearWithValue, 0, TAG_DONE);
				node->objstruct = mimeentry;

				if(rarray[AMI_MIME_MIMETYPE])
				{
					lerror = lwc_intern_string((char *)rarray[AMI_MIME_MIMETYPE],
								strlen((char *)rarray[AMI_MIME_MIMETYPE]), &mimeentry->mimetype);
					if (lerror != lwc_error_ok)
						return NSERROR_NOMEM;
				}

				if(rarray[AMI_MIME_DATATYPE])
				{
					lerror = lwc_intern_string((char *)rarray[AMI_MIME_DATATYPE],
								strlen((char *)rarray[AMI_MIME_DATATYPE]), &mimeentry->datatype);
					if (lerror != lwc_error_ok)
						return NSERROR_NOMEM;
				}

				if(rarray[AMI_MIME_FILETYPE])
				{
					lerror = lwc_intern_string((char *)rarray[AMI_MIME_FILETYPE],
								strlen((char *)rarray[AMI_MIME_FILETYPE]), &mimeentry->filetype);
					if (lerror != lwc_error_ok)
						return NSERROR_NOMEM;
				}

				if(rarray[AMI_MIME_PLUGINCMD])
				{
					lerror = lwc_intern_string((char *)rarray[AMI_MIME_PLUGINCMD],
								strlen((char *)rarray[AMI_MIME_PLUGINCMD]), &mimeentry->plugincmd);
					if (lerror != lwc_error_ok)
						return NSERROR_NOMEM;
				}
				FreeArgs(rargs);
			}
		}
		FClose(fh);
	}
	FreeDosObject(DOS_RDARGS, rargs);
}

void ami_mime_free(void)
{
	ami_mime_dump();
	FreeObjList(ami_mime_list);
}

void ami_mime_entry_free(struct ami_mime_entry *mimeentry)
{
	if(mimeentry->mimetype) lwc_string_unref(mimeentry->mimetype);
	if(mimeentry->datatype) lwc_string_unref(mimeentry->datatype);
	if(mimeentry->filetype) lwc_string_unref(mimeentry->filetype);
	if(mimeentry->plugincmd) lwc_string_unref(mimeentry->plugincmd);
}


/**
 * Return next matching MIME entry
 *
 * \param search lwc_string to search for (or NULL for all)
 * \param type of value being searched for (AMI_MIME_#?)
 * \param start_node node to continue search (updated on exit)
 * \return entry or NULL if no match
 */

struct ami_mime_entry *ami_mime_entry_locate(lwc_string *search,
		int type, struct Node **start_node)
{
	struct nsObject *node;
	struct nsObject *nnode;
	struct ami_mime_entry *mimeentry;
	lwc_error lerror;
	bool ret = false;

	if(IsMinListEmpty(ami_mime_list)) return NULL;

	if(*start_node)
	{
		node = (struct nsObject *)GetSucc(*start_node);
		if(node == NULL) return NULL;
	}
	else
	{
		node = (struct nsObject *)GetHead((struct List *)ami_mime_list);
	}

	do
	{
		nnode=(struct nsObject *)GetSucc((struct Node *)node);
		mimeentry = node->objstruct;

		lerror = lwc_error_ok;

		switch(type)
		{
			case AMI_MIME_MIMETYPE:
				if(search != NULL)
					lerror = lwc_string_isequal(mimeentry->mimetype, search, &ret);
				else if(mimeentry->mimetype != NULL)
					ret = true;
			break;

			case AMI_MIME_DATATYPE:
				if(search != NULL)
					lerror = lwc_string_isequal(mimeentry->datatype, search, &ret);
				else if(mimeentry->datatype != NULL)
					ret = true;
			break;

			case AMI_MIME_FILETYPE:
				if(search != NULL)
					lerror = lwc_string_isequal(mimeentry->filetype, search, &ret);
				else if(mimeentry->filetype != NULL)
					ret = true;
			break;

			case AMI_MIME_PLUGINCMD:
				if(search != NULL)
					lerror = lwc_string_isequal(mimeentry->plugincmd, search, &ret);
				else if(mimeentry->plugincmd != NULL)
					ret = true;
			break;
		}

		if((lerror == lwc_error_ok) && (ret == true))
			break;

	}while(node=nnode);

	*start_node = (struct Node *)node;

	if(ret == true) return mimeentry;
		else return NULL;
}


APTR ami_mime_guess_add_datatype(struct DataType *dt, lwc_string **lwc_mimetype)
{
	struct nsObject *node;
	char mimetype[100];
	char *dt_name_lwr;
	struct ami_mime_entry *mimeentry;
	lwc_error lerror;
	struct DataTypeHeader *dth = dt->dtn_Header;
	char *p;

	node = AddObject(ami_mime_list, AMINS_MIME);
	mimeentry = AllocVecTags(sizeof(struct ami_mime_entry), AVT_ClearWithValue, 0, TAG_DONE);
	node->objstruct = mimeentry;

	lerror = lwc_intern_string(dth->dth_Name, strlen(dth->dth_Name), &mimeentry->datatype);
	if (lerror != lwc_error_ok)
		return NULL;

	dt_name_lwr = strdup(dth->dth_Name);
	if(dt_name_lwr == NULL) return NULL;

	strlwr(dt_name_lwr);
	p = dt_name_lwr;

	while(*p != '\0')
	{
		if(*p == ' ') *p = '-';
		if(*p == '/') *p = '-';
		p++;
	}

	switch(dth->dth_GroupID)
	{
		case GID_TEXT:
		case GID_DOCUMENT:
			if(strcmp("ascii", dt_name_lwr)==0)
			{
				strcpy(mimetype,"text/plain");
			}
			else
			{
				sprintf(mimetype,"text/%s", dt_name_lwr);
			}
		break;
		case GID_SOUND:
		case GID_INSTRUMENT:
		case GID_MUSIC:
			sprintf(mimetype,"audio/%s", dt_name_lwr);
		break;
		case GID_PICTURE:
			if(strcmp("sprite", dt_name_lwr)==0)
			{
				strcpy(mimetype,"image/x-riscos-sprite");
			}
			else
			{
				sprintf(mimetype,"image/%s", dt_name_lwr);
			}
		break;
		case GID_ANIMATION:
		case GID_MOVIE:
			sprintf(mimetype,"video/%s", dt_name_lwr);
		break;
		case GID_SYSTEM:
		default:
			if(strcmp("directory", dt_name_lwr)==0)
			{
				strcpy(mimetype,"application/x-netsurf-directory");
			}
			else if(strcmp("binary", dt_name_lwr)==0)
			{
				strcpy(mimetype,"application/octet-stream");
			}
			else sprintf(mimetype,"application/%s", dt_name_lwr);
		break;
	}

	lerror = lwc_intern_string(mimetype, strlen(mimetype), &mimeentry->mimetype);
	if (lerror != lwc_error_ok)
		return NULL;

	*lwc_mimetype = mimeentry->mimetype;

	lerror = lwc_intern_string(dt_name_lwr, strlen(dt_name_lwr), &mimeentry->filetype);
	if (lerror != lwc_error_ok)
		return NULL;

	free(dt_name_lwr);
	return node;
}

/**
 * Return a MIME Type matching a DataType
 *
 * \param dt a DataType structure
 * \param mimetype lwc_string to hold the MIME type
 * \param start_node node to feed back in to continue search
 * \return node or NULL if no match
 */

struct Node *ami_mime_from_datatype(struct DataType *dt,
		lwc_string **mimetype, struct Node *start_node)
{
	struct DataTypeHeader *dth;
	struct Node *node;
	struct ami_mime_entry *mimeentry;
	lwc_string *dt_name;
	lwc_error lerror;

	if(dt == NULL) return NULL;

	dth = dt->dtn_Header;
	lerror = lwc_intern_string(dth->dth_Name, strlen(dth->dth_Name), &dt_name);
	if (lerror != lwc_error_ok)
		return NULL;

	node = start_node;
	mimeentry = ami_mime_entry_locate(dt_name, AMI_MIME_DATATYPE, &node);
	lwc_string_unref(dt_name);

	if(mimeentry != NULL)
	{
		*mimetype = mimeentry->mimetype;
		return (struct Node *)node;
	}
	else
	{
		if(start_node == NULL)
		{
			/* If there are no matching entries in the file, guess */
			return ami_mime_guess_add_datatype(dt, mimetype);
		}
		else
		{
			return NULL;
		}
	}
}

/**
 * Return the DefIcons type matching a MIME type
 *
 * \param mimetype lwc_string MIME type
 * \param filetype ptr to lwc_string to hold DefIcons type
 * \param start_node node to feed back in to continue search
 * \return node or NULL if no match
 */

struct Node *ami_mime_to_filetype(lwc_string *mimetype,
		lwc_string **filetype, struct Node *start_node)
{
	struct Node *node;
	struct ami_mime_entry *mimeentry;

	node = start_node;
	mimeentry = ami_mime_entry_locate(mimetype, AMI_MIME_MIMETYPE, &node);

	if(mimeentry != NULL)
	{
		*filetype = mimeentry->filetype;
		return (struct Node *)node;
	}
	else
	{
		return NULL;
	}
}

const char *ami_mime_content_to_filetype(struct hlcache_handle *c)
{
	struct Node *node;
	lwc_string *filetype;
	lwc_string *mimetype;

	mimetype = content_get_mime_type(c);

	node = ami_mime_to_filetype(mimetype, &filetype, NULL);

	if(node && (filetype != NULL))
		return lwc_string_data(filetype);
	else
		return ami_content_type_to_file_type(content_get_type(c));
}

/**
 * Return all MIME types containing a plugincmd
 *
 * \param mimetype ptr to lwc_string MIME type
 * \param start_node node to feed back in to continue search
 * \return node or NULL if no match
 */

struct Node *ami_mime_has_cmd(lwc_string **mimetype, struct Node *start_node)
{
	struct Node *node;
	struct ami_mime_entry *mimeentry;

	node = start_node;
	mimeentry = ami_mime_entry_locate(NULL, AMI_MIME_PLUGINCMD, &node);

	if(mimeentry != NULL)
	{
		*mimetype = mimeentry->mimetype;
		return (struct Node *)node;
	}
	else
	{
		return NULL;
	}
}

/**
 * Return the plugincmd matching a MIME type
 *
 * \param mimetype lwc_string MIME type
 * \param plugincmd ptr to lwc_string to hold plugincmd
 * \param start_node node to feed back in to continue search
 * \return node or NULL if no match
 */

struct Node *ami_mime_to_plugincmd(lwc_string *mimetype,
		lwc_string **plugincmd, struct Node *start_node)
{
	struct Node *node;
	struct ami_mime_entry *mimeentry;

	node = start_node;
	mimeentry = ami_mime_entry_locate(mimetype, AMI_MIME_MIMETYPE, &node);

	if(mimeentry != NULL)
	{
		*plugincmd = mimeentry->plugincmd;
		return (struct Node *)node;
	}
	else
	{
		return NULL;
	}
}

lwc_string *ami_mime_content_to_cmd(struct hlcache_handle *c)
{
	struct Node *node;
	lwc_string *plugincmd;
	lwc_string *mimetype;

	mimetype = content_get_mime_type(c);

	node = ami_mime_to_plugincmd(mimetype,
		&plugincmd, NULL);

	if(node && (plugincmd != NULL)) return plugincmd;
		else return NULL;
}

/**
 * Compare the MIME type of an hlcache_handle to a DefIcons type
 */

bool ami_mime_compare(struct hlcache_handle *c, const char *type)
{
	bool ret = false;
	lwc_error lerror;
	lwc_string *filetype;
	lwc_string *mime_filetype;
	lwc_string *mime = content_get_mime_type(c);

	if(ami_mime_to_filetype(mime, &mime_filetype, NULL) == NULL)
		return false;

	lerror = lwc_intern_string(type, strlen(type), &filetype);
	if (lerror != lwc_error_ok)
		return false;

	lerror = lwc_string_isequal(filetype, mime_filetype, &ret);
	if (lerror != lwc_error_ok)
		return false;

	lwc_string_unref(filetype);

	return ret;
}


void ami_mime_dump(void)
{
	struct Node *node = NULL;
	struct ami_mime_entry *mimeentry;

	while(mimeentry = ami_mime_entry_locate(NULL, AMI_MIME_MIMETYPE, &node))
	{
		LOG(("%s DT=\"%s\" TYPE=\"%s\" CMD=\"%s\"",
			mimeentry->mimetype ? lwc_string_data(mimeentry->mimetype) : "",
			mimeentry->datatype ? lwc_string_data(mimeentry->datatype) : "",
			mimeentry->filetype ? lwc_string_data(mimeentry->filetype) : "",
			mimeentry->plugincmd ? lwc_string_data(mimeentry->plugincmd) : ""));
	};
}
