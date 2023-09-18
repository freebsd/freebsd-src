/*
 * pythonmod.c: unbound module C wrapper
 *
 * Copyright (c) 2009, Zdenek Vasicek (vasicek AT fit.vutbr.cz)
 *                     Marek Vavrusa  (xvavru00 AT stud.fit.vutbr.cz)
 *
 * This software is open source.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *    * Redistributions of source code must retain the above copyright notice,
 *      this list of conditions and the following disclaimer.
 *
 *    * Redistributions in binary form must reproduce the above copyright notice,
 *      this list of conditions and the following disclaimer in the documentation
 *      and/or other materials provided with the distribution.
 *
 *    * Neither the name of the organization nor the names of its
 *      contributors may be used to endorse or promote products derived from this
 *      software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
/**
 * \file
 * Python module for unbound.  Calls python script.
 */

/* ignore the varargs unused warning from SWIGs internal vararg support */
#ifdef __GNUC__
#pragma GCC diagnostic ignored "-Wunused-parameter"
#ifndef __clang__
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#endif
#endif

#include "config.h"
#include "sldns/sbuffer.h"

#undef _POSIX_C_SOURCE
#undef _XOPEN_SOURCE
#include <Python.h>

#include "pythonmod/pythonmod.h"
#include "util/module.h"
#include "util/config_file.h"
#include "pythonmod_utils.h"

#ifdef S_SPLINT_S
typedef struct PyObject PyObject;
typedef struct PyThreadState PyThreadState;
typedef void* PyGILState_STATE;
#endif

/**
 *  counter for python module instances
 *  incremented by pythonmod_init(...)
 */
int py_mod_count = 0;

/** Python main thread */
PyThreadState* mainthr;

/**
 * Global state for the module.
 */
struct pythonmod_env {

	/** Python script filename. */
	const char* fname;

	/** Python module. */
	PyObject* module;

	/** Module init function */
	PyObject* func_init;
	/** Module deinit function */
	PyObject* func_deinit;
	/** Module operate function */
	PyObject* func_operate;
	/** Module super_inform function */
	PyObject* func_inform;

	/** Python dictionary. */
	PyObject* dict;

	/** Module data. */
	PyObject* data;

	/** Module qstate. */
	struct module_qstate* qstate;
};

/**
 * Per query state for the iterator module.
 */
struct pythonmod_qstate {

	/** Module per query data. */
	PyObject* data;
};

/* Generated */
#ifndef S_SPLINT_S
#include "pythonmod/interface.h"
#endif

/** log python error */
static void
log_py_err(void)
{
	char *result = NULL;
	const char* iomod = "cStringIO";
	PyObject *modStringIO = NULL;
	PyObject *modTB = NULL;
	PyObject *obFuncStringIO = NULL;
	PyObject *obStringIO = NULL;
	PyObject *obFuncTB = NULL;
	PyObject *argsTB = NULL;
	PyObject *obResult = NULL;
	PyObject *ascstr = NULL;
	PyObject *exc_typ, *exc_val, *exc_tb;

	/* Fetch the error state now before we cruch it */
	/* exc val contains the error message
	 * exc tb contains stack traceback and other info. */
	PyErr_Fetch(&exc_typ, &exc_val, &exc_tb);
	PyErr_NormalizeException(&exc_typ, &exc_val, &exc_tb);

	/* Import the modules we need - cStringIO and traceback */
	modStringIO = PyImport_ImportModule("cStringIO");
	if (modStringIO==NULL) {
		/* python 1.4 and before */
		modStringIO = PyImport_ImportModule("StringIO");
		iomod = "StringIO";
	}
	if (modStringIO==NULL) {
		/* python 3 */
		modStringIO = PyImport_ImportModule("io");
		iomod = "io";
	}
	if (modStringIO==NULL) {
		log_err("pythonmod: cannot print exception, "
			"cannot ImportModule cStringIO or StringIO or io");
		goto cleanup;
	}
	modTB = PyImport_ImportModule("traceback");
	if (modTB==NULL) {
		log_err("pythonmod: cannot print exception, "
			"cannot ImportModule traceback");
		goto cleanup;
	}

	/* Construct a cStringIO object */
	obFuncStringIO = PyObject_GetAttrString(modStringIO, "StringIO");
	if (obFuncStringIO==NULL) {
		log_err("pythonmod: cannot print exception, "
			"cannot GetAttrString %s.StringIO", iomod);
		goto cleanup;
	}
	obStringIO = PyObject_CallObject(obFuncStringIO, NULL);
	if (obStringIO==NULL) {
		log_err("pythonmod: cannot print exception, "
			"cannot call %s.StringIO()", iomod);
		goto cleanup;
	}

	/* Get the traceback.print_exception function, and call it. */
	obFuncTB = PyObject_GetAttrString(modTB, "print_exception");
	if (obFuncTB==NULL) {
		log_err("pythonmod: cannot print exception, "
			"cannot GetAttrString traceback.print_exception");
		goto cleanup;
	}
	argsTB = Py_BuildValue("OOOOO", (exc_typ ? exc_typ : Py_None),
		(exc_val ? exc_val : Py_None), (exc_tb  ? exc_tb  : Py_None),
		Py_None, obStringIO);
	if (argsTB==NULL) {
		log_err("pythonmod: cannot print exception, "
			"cannot BuildValue for print_exception");
		goto cleanup;
	}

	obResult = PyObject_CallObject(obFuncTB, argsTB);
	if (obResult==NULL) {
		PyErr_Print();
		log_err("pythonmod: cannot print exception, "
			"call traceback.print_exception() failed");
		goto cleanup;
	}

	/* Now call the getvalue() method in the StringIO instance */
	Py_DECREF(obFuncStringIO);
	obFuncStringIO = PyObject_GetAttrString(obStringIO, "getvalue");
	if (obFuncStringIO==NULL) {
		log_err("pythonmod: cannot print exception, "
			"cannot GetAttrString StringIO.getvalue");
		goto cleanup;
	}
	Py_DECREF(obResult);
	obResult = PyObject_CallObject(obFuncStringIO, NULL);
	if (obResult==NULL) {
		log_err("pythonmod: cannot print exception, "
			"call StringIO.getvalue() failed");
		goto cleanup;
	}

	/* And it should be a string all ready to go - duplicate it. */
	if (!PyString_Check(obResult) && !PyUnicode_Check(obResult)) {
		log_err("pythonmod: cannot print exception, "
			"StringIO.getvalue() result did not String_Check"
			" or Unicode_Check");
		goto cleanup;
	}
	if(PyString_Check(obResult)) {
		result = PyString_AsString(obResult);
	} else {
		ascstr = PyUnicode_AsASCIIString(obResult);
		result = PyBytes_AsString(ascstr);
	}
	log_err("pythonmod: python error: %s", result);

cleanup:
	Py_XDECREF(modStringIO);
	Py_XDECREF(modTB);
	Py_XDECREF(obFuncStringIO);
	Py_XDECREF(obStringIO);
	Py_XDECREF(obFuncTB);
	Py_XDECREF(argsTB);
	Py_XDECREF(obResult);
	Py_XDECREF(ascstr);

	/* clear the exception, by not restoring it */
	/* Restore the exception state */
	/* PyErr_Restore(exc_typ, exc_val, exc_tb); */
	/* when using PyErr_Restore there is no need to Py_XDECREF for
	 * these 3 pointers. */
	Py_XDECREF(exc_typ);
	Py_XDECREF(exc_val);
	Py_XDECREF(exc_tb);
}

/* we only want to unwind Python once at exit */
static void
pythonmod_atexit(void)
{
   log_assert(py_mod_count == 0);
   log_assert(mainthr != NULL);
 
   PyEval_RestoreThread(mainthr);
   Py_Finalize();
}

int pythonmod_init(struct module_env* env, int id)
{
   int py_mod_idx = py_mod_count++;

   /* Initialize module */
   FILE* script_py = NULL;
   PyObject* py_init_arg = NULL, *res = NULL;
   PyGILState_STATE gil;
   int init_standard = 1, i = 0;
#if PY_MAJOR_VERSION < 3
   PyObject* PyFileObject = NULL;
#endif
   struct config_strlist* cfg_item = env->cfg->python_script;

   struct pythonmod_env* pe = (struct pythonmod_env*)calloc(1, sizeof(struct pythonmod_env));
   if (!pe)
   {
      log_err("pythonmod: malloc failure");
      return 0;
   }

   env->modinfo[id] = (void*) pe;

   /* Initialize module */
   pe->fname=NULL; i = 0;
   while (cfg_item!=NULL) {
      if (py_mod_idx==i++) {
         pe->fname=cfg_item->str;
         break;
      }
      cfg_item = cfg_item->next;
   }
   if(pe->fname==NULL || pe->fname[0]==0) {
      log_err("pythonmod[%d]: no script given.", py_mod_idx);
      return 0;
   }

   /* Initialize Python libraries */
   if (py_mod_count==1 && !Py_IsInitialized()) 
   {
#if PY_VERSION_HEX >= 0x03080000
      PyStatus status;
      PyPreConfig preconfig;
      PyConfig config;
#endif
#if PY_MAJOR_VERSION >= 3
      wchar_t progname[8];
      mbstowcs(progname, "unbound", 8);
#else
      char *progname = "unbound";
#endif
#if PY_VERSION_HEX < 0x03080000
      Py_SetProgramName(progname);
#else
      /* Python must be preinitialized, before the PyImport_AppendInittab
       * call. */
      PyPreConfig_InitPythonConfig(&preconfig);
      status = Py_PreInitialize(&preconfig);
      if(PyStatus_Exception(status)) {
	log_err("python exception in Py_PreInitialize: %s%s%s",
		(status.func?status.func:""), (status.func?": ":""),
		(status.err_msg?status.err_msg:""));
	return 0;
      }
#endif
      Py_NoSiteFlag = 1;
#if PY_MAJOR_VERSION >= 3
      PyImport_AppendInittab(SWIG_name, (void*)SWIG_init);
#endif
#if PY_VERSION_HEX < 0x03080000
      Py_Initialize();
#else
      PyConfig_InitPythonConfig(&config);
      status = PyConfig_SetString(&config, &config.program_name, progname);
      if(PyStatus_Exception(status)) {
	log_err("python exception in PyConfig_SetString(.. program_name ..): %s%s%s",
		(status.func?status.func:""), (status.func?": ":""),
		(status.err_msg?status.err_msg:""));
	PyConfig_Clear(&config);
	return 0;
      }
      config.site_import = 0;
      status = Py_InitializeFromConfig(&config);
      if(PyStatus_Exception(status)) {
	log_err("python exception in Py_InitializeFromConfig: %s%s%s",
		(status.func?status.func:""), (status.func?": ":""),
		(status.err_msg?status.err_msg:""));
	PyConfig_Clear(&config);
	return 0;
      }
      PyConfig_Clear(&config);
#endif
#if PY_MAJOR_VERSION <= 2 || (PY_MAJOR_VERSION == 3 && PY_MINOR_VERSION <= 6)
      /* initthreads only for python 3.6 and older */
      PyEval_InitThreads();
#endif
      SWIG_init();
      mainthr = PyEval_SaveThread();

      /* register callback to unwind Python at exit */
      atexit(pythonmod_atexit);
   }

   gil = PyGILState_Ensure();

   if (py_mod_count==1) {
      /* Initialize Python */
      if(PyRun_SimpleString("import sys \n") < 0 ) {
         log_err("pythonmod: cannot initialize core module: unboundmodule.py");
         goto python_init_fail;
      }
      PyRun_SimpleString("sys.path.append('.') \n");
      PyRun_SimpleString("sys.path.append('"RUN_DIR"') \n");
      PyRun_SimpleString("sys.path.append('"SHARE_DIR"') \n");
      if(env->cfg->directory && env->cfg->directory[0]) {
         char wdir[1524];
         snprintf(wdir, sizeof(wdir), "sys.path.append('%s') \n",
         env->cfg->directory);
         PyRun_SimpleString(wdir);
      }
      if(PyRun_SimpleString("import site\n") < 0) {
         log_err("pythonmod: cannot initialize core module: unboundmodule.py");
         goto python_init_fail;
      }
      if(PyRun_SimpleString("sys.path.extend(site.getsitepackages())\n") < 0) {
         log_err("pythonmod: cannot initialize core module: unboundmodule.py");
         goto python_init_fail;
      }
      if(PyRun_SimpleString("from unboundmodule import *\n") < 0)
      {
         log_err("pythonmod: cannot initialize core module: unboundmodule.py");
         goto python_init_fail;
      }
   }

   /* Check Python file load */
   /* uses python to open the file, this works on other platforms,
    * eg. Windows, to open the file in the correct mode for python */
#if PY_MAJOR_VERSION < 3
   PyFileObject = PyFile_FromString((char*)pe->fname, "r");
   script_py = PyFile_AsFile(PyFileObject);
#else
   script_py = fopen(pe->fname, "r");
#endif
   if (script_py == NULL)
   {
      log_err("pythonmod: can't open file %s for reading", pe->fname);
      goto python_init_fail;
   }

   /* Load file */
   pe->module = PyImport_AddModule("__main__");
   Py_XINCREF(pe->module);
   pe->dict = PyModule_GetDict(pe->module);
   Py_XINCREF(pe->dict);
   pe->data = PyDict_New();
   Py_XINCREF(pe->data);  /* reference will be stolen below */
   if(PyModule_AddObject(pe->module, "mod_env", pe->data) < 0) {
	log_err("pythonmod: could not add mod_env object");
	Py_XDECREF(pe->data);  /* 2 times, here and on python_init_fail; */
	                       /* on failure the reference is not stolen */
	goto python_init_fail;
   }

   if (PyRun_SimpleFile(script_py, pe->fname) < 0) {
#if PY_MAJOR_VERSION <= 2 || (PY_MAJOR_VERSION == 3 && PY_MINOR_VERSION < 9)
      /* for python before 3.9 */
      log_err("pythonmod: can't parse Python script %s", pe->fname);
      /* print the error to logs too, run it again */
      fseek(script_py, 0, SEEK_SET);
      /* we don't run the file, like this, because then side-effects
       *    s = PyRun_File(script_py, pe->fname, Py_file_input, 
       *        PyModule_GetDict(PyImport_AddModule("__main__")), pe->dict);
       * could happen (again). Instead we parse the file again to get
       * the error string in the logs, for when the daemon has stderr
       * removed.  SimpleFile run already printed to stderr, for then
       * this is called from unbound-checkconf or unbound -dd the user
       * has a nice formatted error.
      */
      /* ignore the NULL return of _node, it is NULL due to the parse failure
       * that we are expecting */
      (void)PyParser_SimpleParseFile(script_py, pe->fname, Py_file_input);
#else
      /* for python 3.9 and newer */
      char* fstr = NULL;
      size_t flen = 0;
      log_err("pythonmod: can't parse Python script %s", pe->fname);
      /* print the error to logs too, run it again */
      fseek(script_py, 0, SEEK_END);
      flen = (size_t)ftell(script_py);
      fstr = malloc(flen+1);
      if(!fstr) {
	      log_err("malloc failure to print parse error");

/* close the file */
#if PY_MAJOR_VERSION < 3
	      Py_XDECREF(PyFileObject);
#else
	      fclose(script_py);
#endif

	      goto python_init_fail;
      }
      fseek(script_py, 0, SEEK_SET);
      if(fread(fstr, flen, 1, script_py) < 1) {
	      log_err("file read failed to print parse error: %s: %s",
		pe->fname, strerror(errno));
	      free(fstr);

/* close the file */
#if PY_MAJOR_VERSION < 3
	      Py_XDECREF(PyFileObject);
#else
	      fclose(script_py);
#endif

	      goto python_init_fail;
      }
      fstr[flen] = 0;
      /* we compile the string, but do not run it, to stop side-effects */
      /* ignore the NULL return of _node, it is NULL due to the parse failure
       * that we are expecting */
      (void)Py_CompileString(fstr, pe->fname, Py_file_input);
#endif

      log_py_err();

/* close the file */
#if PY_MAJOR_VERSION < 3
      Py_XDECREF(PyFileObject);
#else
      fclose(script_py);
#endif

#if PY_MAJOR_VERSION <= 2 || (PY_MAJOR_VERSION == 3 && PY_MINOR_VERSION < 9)
      /* no cleanup needed for python before 3.9 */
#else
      /* cleanup for python 3.9 and newer */
      free(fstr);
#endif
      goto python_init_fail;
   }

/* close the file */
#if PY_MAJOR_VERSION < 3
   Py_XDECREF(PyFileObject);
#else
   fclose(script_py);
#endif

   if ((pe->func_init = PyDict_GetItemString(pe->dict, "init_standard")) == NULL)
   {
      init_standard = 0;
      if ((pe->func_init = PyDict_GetItemString(pe->dict, "init")) == NULL)
      {
         log_err("pythonmod: function init is missing in %s", pe->fname);
         goto python_init_fail;
      }
   }
   Py_XINCREF(pe->func_init);
   if ((pe->func_deinit = PyDict_GetItemString(pe->dict, "deinit")) == NULL)
   {
      log_err("pythonmod: function deinit is missing in %s", pe->fname);
      goto python_init_fail;
   }
   Py_XINCREF(pe->func_deinit);
   if ((pe->func_operate = PyDict_GetItemString(pe->dict, "operate")) == NULL)
   {
      log_err("pythonmod: function operate is missing in %s", pe->fname);
      goto python_init_fail;
   }
   Py_XINCREF(pe->func_operate);
   if ((pe->func_inform = PyDict_GetItemString(pe->dict, "inform_super")) == NULL)
   {
      log_err("pythonmod: function inform_super is missing in %s", pe->fname);
      goto python_init_fail;
   }
   Py_XINCREF(pe->func_inform);

   if (init_standard)
   {
      py_init_arg = SWIG_NewPointerObj((void*) env, SWIGTYPE_p_module_env, 0);
   }
   else
   {
      py_init_arg = SWIG_NewPointerObj((void*) env->cfg,
        SWIGTYPE_p_config_file, 0);
   }
   res = PyObject_CallFunction(pe->func_init, "iO", id, py_init_arg);
   if (PyErr_Occurred())
   {
      log_err("pythonmod: Exception occurred in function init");
      log_py_err();
      goto python_init_fail;
   }

   Py_XDECREF(res);
   Py_XDECREF(py_init_arg);
   PyGILState_Release(gil);
   return 1;

python_init_fail:
   Py_XDECREF(pe->module);
   Py_XDECREF(pe->dict);
   Py_XDECREF(pe->data);
   Py_XDECREF(pe->func_init);
   Py_XDECREF(pe->func_deinit);
   Py_XDECREF(pe->func_operate);
   Py_XDECREF(pe->func_inform);
   Py_XDECREF(res);
   Py_XDECREF(py_init_arg);
   PyGILState_Release(gil);
   return 0;
}

void pythonmod_deinit(struct module_env* env, int id)
{
   int cbtype;
   struct pythonmod_env* pe = env->modinfo[id];
   if(pe == NULL)
      return;

   /* Free Python resources */
   if(pe->module != NULL)
   {
      PyObject* res;
      PyGILState_STATE gil = PyGILState_Ensure();

      /* Deinit module */
      res = PyObject_CallFunction(pe->func_deinit, "i", id);
      if (PyErr_Occurred()) {
         log_err("pythonmod: Exception occurred in function deinit");
         log_py_err();
      }
      /* Free result if any */
      Py_XDECREF(res);
      /* Free shared data if any */
      Py_XDECREF(pe->module);
      Py_XDECREF(pe->dict);
      Py_XDECREF(pe->data);
      Py_XDECREF(pe->func_init);
      Py_XDECREF(pe->func_deinit);
      Py_XDECREF(pe->func_inform);
      Py_XDECREF(pe->func_operate);
      PyGILState_Release(gil);

      py_mod_count--;
   }
   pe->fname = NULL;
   free(pe);

   /* iterate over all possible callback types and clean up each in turn */
   for (cbtype = 0; cbtype < inplace_cb_types_total; cbtype++)
      inplace_cb_delete(env, cbtype, id);

   /* Module is deallocated in Python */
   env->modinfo[id] = NULL;
}

void pythonmod_inform_super(struct module_qstate* qstate, int id, struct module_qstate* super)
{
   struct pythonmod_env* pe = (struct pythonmod_env*)qstate->env->modinfo[id];
   struct pythonmod_qstate* pq = (struct pythonmod_qstate*)qstate->minfo[id];
   PyObject* py_qstate, *py_sqstate, *res;
   PyGILState_STATE gil = PyGILState_Ensure();

   log_query_info(VERB_ALGO, "pythonmod: inform_super, sub is", &qstate->qinfo);
   log_query_info(VERB_ALGO, "super is", &super->qinfo);

   py_qstate = SWIG_NewPointerObj((void*) qstate, SWIGTYPE_p_module_qstate, 0);
   py_sqstate = SWIG_NewPointerObj((void*) super, SWIGTYPE_p_module_qstate, 0);

   res = PyObject_CallFunction(pe->func_inform, "iOOO", id, py_qstate,
	py_sqstate, pq->data);

   if (PyErr_Occurred())
   {
      log_err("pythonmod: Exception occurred in function inform_super");
      log_py_err();
      qstate->ext_state[id] = module_error;
   }
   else if ((res == NULL)  || (!PyObject_IsTrue(res)))
   {
      log_err("pythonmod: python returned bad code in inform_super");
      qstate->ext_state[id] = module_error;
   }

   Py_XDECREF(res);
   Py_XDECREF(py_sqstate);
   Py_XDECREF(py_qstate);

   PyGILState_Release(gil);
}

void pythonmod_operate(struct module_qstate* qstate, enum module_ev event,
	int id, struct outbound_entry* ATTR_UNUSED(outbound))
{
   struct pythonmod_env* pe = (struct pythonmod_env*)qstate->env->modinfo[id];
   struct pythonmod_qstate* pq = (struct pythonmod_qstate*)qstate->minfo[id];
   PyObject* py_qstate, *res;
   PyGILState_STATE gil = PyGILState_Ensure();

   if ( pq == NULL)
   {
      /* create qstate */
      pq = qstate->minfo[id] = malloc(sizeof(struct pythonmod_qstate));
      if(!pq) {
		log_err("pythonmod_operate: malloc failure for qstate");
		PyGILState_Release(gil);
		return;
      }

      /* Initialize per query data */
      pq->data = PyDict_New();
      if(!pq->data) {
		log_err("pythonmod_operate: malloc failure for query data dict");
		PyGILState_Release(gil);
		return;
      }
   }

   /* Call operate */
   py_qstate = SWIG_NewPointerObj((void*) qstate, SWIGTYPE_p_module_qstate, 0);
   res = PyObject_CallFunction(pe->func_operate, "iiOO", id, (int) event,
	py_qstate, pq->data);
   if (PyErr_Occurred())
   {
      log_err("pythonmod: Exception occurred in function operate, event: %s", strmodulevent(event));
      log_py_err();
      qstate->ext_state[id] = module_error;
   }
   else if ((res == NULL)  || (!PyObject_IsTrue(res)))
   {
      log_err("pythonmod: python returned bad code, event: %s", strmodulevent(event));
      qstate->ext_state[id] = module_error;
   }
   Py_XDECREF(res);
   Py_XDECREF(py_qstate);

   PyGILState_Release(gil);
}

void pythonmod_clear(struct module_qstate* qstate, int id)
{
   struct pythonmod_qstate* pq;
   if (qstate == NULL)
      return;

   pq = (struct pythonmod_qstate*)qstate->minfo[id];
   verbose(VERB_ALGO, "pythonmod: clear, id: %d, pq:%p", id, pq);
   if(pq != NULL)
   {
      PyGILState_STATE gil = PyGILState_Ensure();
      Py_DECREF(pq->data);
      PyGILState_Release(gil);
      /* Free qstate */
      free(pq);
   }

   qstate->minfo[id] = NULL;
}

size_t pythonmod_get_mem(struct module_env* env, int id)
{
   struct pythonmod_env* pe = (struct pythonmod_env*)env->modinfo[id];
   verbose(VERB_ALGO, "pythonmod: get_mem, id: %d, pe:%p", id, pe);
   if(!pe)
      return 0;
   return sizeof(*pe);
}

/**
 * The module function block
 */
static struct module_func_block pythonmod_block = {
   "python",
   &pythonmod_init, &pythonmod_deinit, &pythonmod_operate, &pythonmod_inform_super,
   &pythonmod_clear, &pythonmod_get_mem
};

struct module_func_block* pythonmod_get_funcblock(void)
{
   return &pythonmod_block;
}
