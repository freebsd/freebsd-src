/*
 * Copyright 2012 Vincent Sanders <vince@netsurf-browser.org>
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

/** \file
 * spidermonkey jsapi compatability glue.
 */

#ifndef _NETSURF_JAVASCRIPT_JSAPI_H_
#define _NETSURF_JAVASCRIPT_JSAPI_H_

/* include the correct header */
#ifdef WITH_MOZJS
#include "js/jsapi.h"
#else
#include "mozjs/jsapi.h"
#endif

/* logging macros */
#define JSLOG(args...) LOG((args))
#ifdef ENABLE_VERBOSE_JS_DEBUG
#define JSDBG(args...) LOG((args))
#else
#define JSDBG(args...)
#endif

#if JS_VERSION < 180

/************************** Spidermonkey 1.7.0 **************************/

#include <string.h>

#ifndef JSVERSION_LATEST
#define JSVERSION_LATEST JS_VERSION
#endif

#ifndef JSOPTION_JIT
#define JSOPTION_JIT 0
#endif

/* *CAUTION* these native function macros introduce and use jsapi_this
 * and jsapi_rval variables, native function code should not conflict
 * with these
 */

/* native function definition with five parameters */
#define JSAPI_FUNC(name, cx, argc, vp) \
	jsapi_func_##name(cx, JSObject *jsapi_this, argc, vp, jsval *jsapi_rval)

/* native function return value - No macro available */
#define JSAPI_FUNC_RVAL(cx, vp) (jsapi_rval)

/* native function return value setter - No macro available  */
#define JSAPI_FUNC_SET_RVAL(cx, vp, v) (*jsapi_rval = (v))

/* arguments */
#define JSAPI_FUNC_ARGV(cx, vp) (vp)

/* check if a jsval is an object */
#define JSAPI_JSVAL_IS_OBJECT(v) JSVAL_IS_OBJECT(v)

/* native function specifier with five parameters and no JS_FS macro */
#define JSAPI_FS(name, nargs, flags) \
	{ #name, jsapi_func_##name, nargs, flags, 0 }

/* native function specifier list end */
#define JSAPI_FS_END { NULL, NULL, 0, 0, 0 }




/* native proprty definition */
#define JSAPI_PROP(name, cx, obj, vp) \
	jsapi_property_##name(cx, obj, jsval jsapi_id, vp)
#define JSAPI_STRICTPROP JSAPI_PROP

/* native property return value */
#define JSAPI_PROP_RVAL(cx, vp) (*(vp))

/* native property getter return value */
#define JSAPI_PROP_SET_RVAL(cx, vp, v) (*(vp) = (v))

/* native property ID value as a jsval */
#define JSAPI_PROP_IDVAL(cx, vp) (*(vp) = jsapi_id)

/* native property specifier */
#define JSAPI_PS(name, fnname, tinyid, flags)				\
	{ name , tinyid , flags , jsapi_property_##fnname##_get , jsapi_property_##fnname##_set }

/* native property specifier with no setter */
#define JSAPI_PS_RO(name, fnname, tinyid, flags)				\
	{ name , tinyid , flags | JSPROP_READONLY, jsapi_property_##fnname##_get , NULL }

/* native property specifier list end */
#define JSAPI_PS_END { NULL, 0, 0, NULL, NULL }

#define JS_StrictPropertyStub JS_PropertyStub




/* The object instance in a native call */
/* "this" JSObject getter */
JSObject * js_ComputeThis(JSContext *cx, JSObject *thisp, void *argv);
#define JSAPI_THIS_OBJECT(cx, vp) \
        js_ComputeThis(cx, JSVAL_TO_OBJECT(vp[-1]), vp)

static inline JSObject *
JS_NewCompartmentAndGlobalObject(JSContext *cx,
				 JSClass *jsclass,
				 JSPrincipals *principals)
{
	JSObject *global;
	global = JS_NewObject(cx, jsclass, NULL, NULL);
	if (global == NULL) {
		return NULL;
	}
	return global;
}


#define JSString_to_char(injsstring, outchar, outlen)	\
	outchar = JS_GetStringBytes(injsstring);		\
	outlen = strlen(outchar)

/* string type cast */
#define JSAPI_STRING_TO_JSVAL(str) ((str == NULL)?JSVAL_NULL:STRING_TO_JSVAL(str))

#define JSAPI_CLASS_NO_INTERNAL_MEMBERS NULL

/* Garbage Collector */

/* macros for GC marking */
#define JSAPI_JSCLASS_MARK_IS_TRACE 0

#define JSAPI_JSCLASS_MARKOP(x) (x)

#define JSAPI_MARKOP(name) uint32_t name(JSContext *cx, JSObject *obj, void *arg)

#define JSAPI_MARKCX cx

#define JSAPI_GCMARK(thing) JS_MarkGCThing(cx, thing, "object", arg)

#define JSAPI_MARKOP_RETURN(value) return value


/* Macros for manipulating GC root */
#define JSAPI_ADD_OBJECT_ROOT(cx, obj) JS_AddRoot(cx, obj)
#define JSAPI_REMOVE_OBJECT_ROOT(cx, obj) JS_RemoveRoot(cx, obj)

#define JSAPI_ADD_VALUE_ROOT(cx, obj) JS_AddRoot(cx, obj)
#define JSAPI_REMOVE_VALUE_ROOT(cx, obj) JS_RemoveRoot(cx, obj)

#elif JS_VERSION == 180

/************************** Spidermonkey 1.8.0 **************************/

#include <string.h>

/* *CAUTION* these macros introduce and use jsapi_this and jsapi_rval
 * parameters, native function code should not conflict with these
 */

/* five parameter jsapi native call */
#define JSAPI_FUNC(name, cx, argc, vp) \
	jsapi_func_##name(cx, JSObject *jsapi_this, argc, vp, jsval *jsapi_rval)

/* five parameter function descriptor */
#define JSAPI_FS(name, nargs, flags) \
	JS_FS(#name, jsapi_func_##name, nargs, flags, 0)

/* function descriptor end */
#define JSAPI_FS_END JS_FS_END

/* return value */
#define JSAPI_RVAL(cx, vp) JS_RVAL(cx, jsapi_rval)

/* return value setter */
#define JSAPI_FUNC_SET_RVAL(cx, vp, v) JS_SET_RVAL(cx, jsapi_rval, v)

/* arguments */
#define JSAPI_FUNC_ARGV(cx, vp) (vp)

/* check if a jsval is an object */
#define JSAPI_JSVAL_IS_OBJECT(v) JSVAL_IS_OBJECT(v)

/* The object instance in a native call */
#define JSAPI_THIS_OBJECT(cx,vp) jsapi_this



/* proprty native calls */
#define JSAPI_PROP(name, cx, obj, vp) \
	jsapi_property_##name(cx, obj, jsval jsapi_id, vp)
#define JSAPI_STRICTPROP JSAPI_PROP

/* native property return value */
#define JSAPI_PROP_RVAL JS_RVAL

/* native property return value setter */
#define JSAPI_PROP_SET_RVAL JS_SET_RVAL

/* native property ID value as a jsval */
#define JSAPI_PROP_IDVAL(cx, vp) (*(vp) = jsapi_id)

/* property specifier */
#define JSAPI_PS(name, fnname, tinyid, flags)				\
	{ name , tinyid , flags , jsapi_property_##fnname##_get , jsapi_property_##fnname##_set }

#define JSAPI_PS_RO(name, fnname, tinyid, flags)				\
	{ name , tinyid , flags | JSPROP_READONLY, jsapi_property_##fnname##_get , NULL }

#define JSAPI_PS_END { NULL, 0, 0, NULL, NULL }




static inline JSObject *
JS_NewCompartmentAndGlobalObject(JSContext *cx,
				 JSClass *jsclass,
				 JSPrincipals *principals)
{
	JSObject *global;
	global = JS_NewObject(cx, jsclass, NULL, NULL);
	if (global == NULL) {
		return NULL;
	}
	return global;
}

#define JS_StrictPropertyStub JS_PropertyStub

#define JSString_to_char(injsstring, outchar, outlen)	\
	outchar = JS_GetStringBytes(injsstring);		\
	outlen = strlen(outchar)

/* string type cast */
#define JSAPI_STRING_TO_JSVAL(str) ((str == NULL)?JSVAL_NULL:STRING_TO_JSVAL(str))

#define JSAPI_CLASS_NO_INTERNAL_MEMBERS NULL

/* Garbage Collector */

/* GC marking */
#ifdef JSCLASS_MARK_IS_TRACE
/* mark function pointer requires casting */
#define JSAPI_JSCLASS_MARK_IS_TRACE JSCLASS_MARK_IS_TRACE
#define JSAPI_JSCLASS_MARKOP(x) ((JSMarkOp)x)
#else
/* mark function pointer does not require casting */
#define JSAPI_JSCLASS_MARK_IS_TRACE 0
#define JSAPI_JSCLASS_MARKOP(x) (x)
#endif

#define JSAPI_MARKOP(name) JSBool name(JSTracer *trc, JSObject *obj)

#define JSAPI_MARKCX trc->context

#define JSAPI_GCMARK(thing) JS_CallTracer(trc, thing, JSTRACE_OBJECT);

#define JSAPI_MARKOP_RETURN(value) return value

/* Macros for manipulating GC root */
#define JSAPI_ADD_OBJECT_ROOT(cx, obj) JS_AddRoot(cx, obj)
#define JSAPI_REMOVE_OBJECT_ROOT(cx, obj) JS_RemoveRoot(cx, obj)

#define JSAPI_ADD_VALUE_ROOT(cx, obj) JS_AddRoot(cx, obj)
#define JSAPI_REMOVE_VALUE_ROOT(cx, obj) JS_RemoveRoot(cx, obj)


#else /* #if JS_VERSION == 180 */

/************************** Spidermonkey 1.8.5 **************************/

/* three parameter jsapi native call */
#define JSAPI_FUNC(name, cx, argc, vp) jsapi_func_##name(cx, argc, vp)

/* three parameter function descriptor */
#define JSAPI_FS(name, nargs, flags) \
	JS_FS(#name, jsapi_func_##name, nargs, flags)

/* function descriptor end */
#define JSAPI_FS_END JS_FS_END

/* return value */
#define JSAPI_RVAL JS_RVAL

/* return value setter */
#define JSAPI_FUNC_SET_RVAL JS_SET_RVAL

/* arguments */
#define JSAPI_FUNC_ARGV(cx, vp) JS_ARGV(cx,vp)

/* check if a jsval is an object */
#define JSAPI_JSVAL_IS_OBJECT(v) JSVAL_IS_OBJECT(v)
/* The docuemntation says this is obsolete and should be
 * ((JSVAL_IS_NULL(v)) || (JSVAL_IS_PRIMITIVE(v)))
 * which doesnt work
 */


/* The object instance in a native call */
#define JSAPI_THIS_OBJECT(cx,vp) JS_THIS_OBJECT(cx,vp)

/* proprty native calls */
#define JSAPI_PROP(name, cx, obj, vp) \
	jsapi_property_##name(cx, obj, jsid jsapi_id, vp)
#define JSAPI_STRICTPROP(name, cx, obj, vp) \
	jsapi_property_##name(cx, obj, jsid jsapi_id, JSBool strict, vp)

/* native property return value */
#define JSAPI_PROP_RVAL JS_RVAL

/* native property getter return value */
#define JSAPI_PROP_SET_RVAL JS_SET_RVAL

/* native property ID value as a jsval */
#define JSAPI_PROP_IDVAL(cx, vp) JS_IdToValue(cx, jsapi_id, vp)

/* property specifier */
#define JSAPI_PS(name, fnname, tinyid, flags) {			\
		name,						\
		tinyid,						\
		flags,						\
		jsapi_property_##fnname##_get,			\
		jsapi_property_##fnname##_set			\
	}

#define JSAPI_PS_RO(name, fnname, tinyid, flags) {		\
		name,						\
		tinyid,						\
		flags | JSPROP_READONLY,			\
		jsapi_property_##fnname##_get,			\
		NULL						\
	}

#define JSAPI_PS_END { NULL, 0, 0, NULL, NULL }


#define JSString_to_char(injsstring, outchar, outlen)		\
	outlen = JS_GetStringLength(injsstring);		\
	outchar = alloca(sizeof(char)*(outlen+1));		\
	JS_EncodeStringToBuffer(injsstring, outchar, outlen);	\
	outchar[outlen] = '\0'

/* string type cast */
#define JSAPI_STRING_TO_JSVAL(str) ((str == NULL)?JSVAL_NULL:STRING_TO_JSVAL(str))

#define JSAPI_CLASS_NO_INTERNAL_MEMBERS JSCLASS_NO_INTERNAL_MEMBERS

/* GC marking */
#ifdef JSCLASS_MARK_IS_TRACE
/* mark function pointer requires casting */
#define JSAPI_JSCLASS_MARK_IS_TRACE JSCLASS_MARK_IS_TRACE
#define JSAPI_JSCLASS_MARKOP(x) ((JSMarkOp)x)
#else
/* mark function pointer does not require casting */
#define JSAPI_JSCLASS_MARK_IS_TRACE 0
#define JSAPI_JSCLASS_MARKOP(x) (x)
#endif

#define JSAPI_MARKOP(name) void name(JSTracer *trc, JSObject *obj)

#define JSAPI_MARKCX trc->context

#define JSAPI_GCMARK(thing) JS_CallTracer(trc, thing, JSTRACE_OBJECT);

#define JSAPI_MARKOP_RETURN(value)


/* Macros for manipulating GC root */
#define JSAPI_ADD_OBJECT_ROOT(cx, obj) JS_AddObjectRoot(cx, obj)
#define JSAPI_REMOVE_OBJECT_ROOT(cx, obj) JS_RemoveObjectRoot(cx, obj)

#define JSAPI_ADD_VALUE_ROOT(cx, val) JS_AddValueRoot(cx, val)
#define JSAPI_REMOVE_VALUE_ROOT(cx, val) JS_RemoveValueRoot(cx, val)

#endif

/************************** **************************/

#endif
