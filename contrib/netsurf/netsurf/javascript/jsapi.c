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

#include <unistd.h>
#include <signal.h>

#include "javascript/jsapi.h"
#include "render/html_internal.h"
#include "content/content.h"
#include "javascript/content.h"
#include "javascript/js.h"

#include "utils/log.h"

#include "window.h"
#include "event.h"

#define ENABLE_JS_HEARTBEAT 1

static JSRuntime *rt; /* global runtime */

void js_initialise(void)
{
	/* Create a JS runtime. */

#if JS_VERSION >= 180
	JS_SetCStringsAreUTF8(); /* we prefer our runtime to be utf-8 */
#endif

	rt = JS_NewRuntime(8L * 1024L * 1024L);
	JSLOG("New runtime handle %p", rt);

	if (rt != NULL) {
		/* register script content handler */
		javascript_init();
	}
}

void js_finalise(void)
{
	if (rt != NULL) {
		JSLOG("destroying runtime handle %p", rt);
		JS_DestroyRuntime(rt);
	}
	JS_ShutDown();
}

/* The error reporter callback. */
static void
js_reportError(JSContext *cx, const char *message, JSErrorReport *report)
{
	JSLOG("%s:%u:%s",
	      report->filename ? report->filename : "<no filename>",
	      (unsigned int) report->lineno,
	      message);
}

/* heartbeat routines */
#ifndef ENABLE_JS_HEARTBEAT

struct heartbeat;

/* prepares a context with a heartbeat handler */
static bool
setup_heartbeat(JSContext *cx, int timeout, jscallback *cb, void *cbctx)
{
	return true;
}

/* enables the heartbeat on a context */
static struct heartbeat *enable_heartbeat(JSContext *cx)
{
	return NULL;
}

/* disables heartbeat on a context */
static bool
disable_heartbeat(struct heartbeat *hb)
{
	return true;
}

#else

/* private context for heartbeats */
struct jscontext_priv {
	int timeout;
	jscallback *cb;
	void *cbctx;

	unsigned int branch_reset; /**< reset value for branch counter */
	unsigned int branch_count; /**< counter for branch callback */
	time_t last; /**< last time heartbeat happened */
	time_t end; /**< end time for the current script execution */
};

/** execution heartbeat */
static JSBool heartbeat_callback(JSContext *cx)
{
	struct jscontext_priv *priv = JS_GetContextPrivate(cx);
	JSBool ret = JS_TRUE;
	time_t now = time(NULL);

	/* dynamically update the branch times to ensure we do not get
	 * called back more than once a second
	 */
	if (now == priv->last) {
		priv->branch_reset = priv->branch_reset * 2;
	}
	priv->last = now;

	JSLOG("Running heatbeat at %d end %d", now , priv->end);

	if ((priv->cb != NULL) &&
	    (now > priv->end)) {
		if (priv->cb(priv->cbctx) == false) {
			ret = JS_FALSE; /* abort */
		} else {
			priv->end = time(NULL) + priv->timeout;
		}
	}

	return ret;
}

#if JS_VERSION >= 180

struct heartbeat {
	JSContext *cx;
	struct sigaction sact; /* signal handler action to restore */
	int alm; /* alarm value to restore */
};

static struct heartbeat *cur_hb;

static bool
setup_heartbeat(JSContext *cx, int timeout, jscallback *cb, void *cbctx)
{
	struct jscontext_priv *priv;

	if (timeout == 0) {
		return true;
	}

	priv = calloc(1, sizeof(*priv));
	if (priv == NULL) {
		return false;
	}

	priv->timeout = timeout;
	priv->cb = cb;
	priv->cbctx = cbctx;

	JS_SetContextPrivate(cx, priv);

	/* if heartbeat is enabled disable JIT or callbacks do not happen */
	JS_SetOptions(cx, JS_GetOptions(cx) & ~JSOPTION_JIT);

	JS_SetOperationCallback(cx, heartbeat_callback);

	return true;
}

static void sig_alm_handler(int signum)
{
	JS_TriggerOperationCallback(cur_hb->cx);
	alarm(1);
	JSDBG("alarm signal handler for context %p", cur_hb->cx);
}

static struct heartbeat *enable_heartbeat(JSContext *cx)
{
	struct jscontext_priv *priv = JS_GetContextPrivate(cx);
	struct sigaction sact;
	struct heartbeat *hb;

	if (priv == NULL) {
		return NULL;
	}

	priv->last = time(NULL);
	priv->end = priv->last + priv->timeout;

	hb = malloc(sizeof(*hb));
	if (hb != NULL) {
		sigemptyset(&sact.sa_mask);
		sact.sa_flags = 0;
		sact.sa_handler = sig_alm_handler;
		if (sigaction(SIGALRM, &sact, &hb->sact) == 0) {
			cur_hb = hb;
			hb->cx = cx;
			hb->alm = alarm(1);
		} else {
			free(hb);
			hb = NULL;
			LOG(("Unable to set heartbeat"));
		}
	}
	return hb;
}

/** disable heartbeat
 *
 * /param hb heartbeat to disable may be NULL
 * /return true on success.
 */
static bool
disable_heartbeat(struct heartbeat *hb)
{
	if (hb != NULL) {
		sigaction(SIGALRM, &hb->sact, NULL); /* restore old handler */
		alarm(hb->alm); /* restore alarm signal */
	}
	return true;
}

#else

/* need to setup callback to prevent long running scripts infinite
 * hanging.
 *
 * old method is to use:
 *  JSBranchCallback JS_SetBranchCallback(JSContext *cx, JSBranchCallback cb);
 * which gets called a *lot* and should only do something every 5k calls
 * The callback function
 *  JSBool (*JSBranchCallback)(JSContext *cx, JSScript *script);
 * returns JS_TRUE to carry on and JS_FALSE to abort execution
 * single thread of execution on the context
 * documented in
 *   https://developer.mozilla.org/en-US/docs/SpiderMonkey/JSAPI_Reference/JS_SetBranchCallback
 *
 */

#define INITIAL_BRANCH_RESET 5000

struct heartbeat;

static JSBool branch_callback(JSContext *cx, JSScript *script)
{
	struct jscontext_priv *priv = JS_GetContextPrivate(cx);
	JSBool ret = JS_TRUE;

	priv->branch_count--;
	if (priv->branch_count == 0) {
		priv->branch_count = priv->branch_reset; /* reset branch count */

		ret = heartbeat_callback(cx);
	}
	return ret;
}

static bool
setup_heartbeat(JSContext *cx, int timeout, jscallback *cb, void *cbctx)
{
	struct jscontext_priv *priv;

	if (timeout == 0) {
		return true;
	}

	priv = calloc(1, sizeof(*priv));
	if (priv == NULL) {
		return false;
	}

	priv->timeout = timeout;
	priv->cb = cb;
	priv->cbctx = cbctx;

	priv->branch_reset = INITIAL_BRANCH_RESET;
	priv->branch_count = priv->branch_reset;

	JS_SetContextPrivate(cx, priv);

	JS_SetBranchCallback(cx, branch_callback);

	return true;
}

static struct heartbeat *enable_heartbeat(JSContext *cx)
{
	struct jscontext_priv *priv = JS_GetContextPrivate(cx);

	if (priv != NULL) {
		priv->last = time(NULL);
		priv->end = priv->last + priv->timeout;
	}
	return NULL;
}

static bool
disable_heartbeat(struct heartbeat *hb)
{
	return true;
}

#endif

#endif

jscontext *js_newcontext(int timeout, jscallback *cb, void *cbctx)
{
	JSContext *cx;

	if (rt == NULL) {
		return NULL;
	}

	cx = JS_NewContext(rt, 8192);
	if (cx == NULL) {
		return NULL;
	}

	/* set options on context */
	JS_SetOptions(cx, JS_GetOptions(cx) | JSOPTION_VAROBJFIX | JSOPTION_JIT);

	JS_SetVersion(cx, JSVERSION_LATEST);
	JS_SetErrorReporter(cx, js_reportError);

	/* run a heartbeat */
	setup_heartbeat(cx, timeout, cb, cbctx);

	/*JS_SetGCZeal(cx, 2); */

	JSLOG("New Context %p", cx);

	return (jscontext *)cx;
}

void js_destroycontext(jscontext *ctx)
{
	JSContext *cx = (JSContext *)ctx;
	struct jscontext_priv *priv;

	if (cx != NULL) {
		JSLOG("Destroying Context %p", cx);
		priv = JS_GetContextPrivate(cx);

		JS_DestroyContext(cx);

		free(priv);
	}
}


/** Create new compartment to run scripts within
 *
 * This performs the following actions
 * 1. constructs a new global object by initialising a window class
 * 2. Instantiate the global a window object
 */
jsobject *js_newcompartment(jscontext *ctx, void *win_priv, void *doc_priv)
{
	JSContext *cx = (JSContext *)ctx;
	JSObject *window_proto;
	JSObject *window;

	if (cx == NULL) {
		return NULL;
	}

	window_proto = jsapi_InitClass_Window(cx, NULL);
	if (window_proto == NULL) {
		JSLOG("Unable to initialise window class");
		return NULL;
	}

	window = jsapi_new_Window(cx, window_proto, NULL, win_priv, doc_priv);

	return (jsobject *)window;
}



bool js_exec(jscontext *ctx, const char *txt, size_t txtlen)
{
	JSContext *cx = (JSContext *)ctx;
	jsval rval;
	JSBool eval_res;
	struct heartbeat *hb;

	/* JSLOG("%p \"%s\"",cx ,txt); */

	if (ctx == NULL) {
		return false;
	}

	if (txt == NULL) {
		return false;
	}

	if (txtlen == 0) {
		return false;
	}

	hb = enable_heartbeat(cx);

	eval_res = JS_EvaluateScript(cx,
				     JS_GetGlobalObject(cx),
				     txt, txtlen,
				     "<head>", 0, &rval);

	disable_heartbeat(hb);

	if (eval_res == JS_TRUE) {

		return true;
	}

	return false;
}

dom_exception _dom_event_create(dom_document *doc, dom_event **evt);
#define dom_event_create(d, e) _dom_event_create((dom_document *)(d), (dom_event **) (e))

bool js_fire_event(jscontext *ctx, const char *type, dom_document *doc, dom_node *target)
{
	JSContext *cx = (JSContext *)ctx;
	dom_node *node = target;
	JSObject *jsevent;
	jsval rval;
	jsval argv[1];
	JSBool ret = JS_TRUE;
	dom_exception exc;
	dom_event *event;
	dom_string *type_dom;
	struct heartbeat *hb;

	if (cx == NULL) {
		return false;
	}

	if (node == NULL) {
		/* deliver manufactured event to window */
		JSLOG("Dispatching event %s at window", type);

		/* create and initialise and event object */
		exc = dom_string_create((unsigned char*)type,
					strlen(type),
					&type_dom);
		if (exc != DOM_NO_ERR) {
			return false;
		}

		exc = dom_event_create(doc, &event);
		if (exc != DOM_NO_ERR) {
			return false;
		}

		exc = dom_event_init(event, type_dom, false, false);
		dom_string_unref(type_dom);
		if (exc != DOM_NO_ERR) {
			return false;
		}

		jsevent = jsapi_new_Event(cx, NULL, NULL, event);
		if (jsevent == NULL) {
			return false;
		}

		hb = enable_heartbeat(cx);

		/* dispatch event at the window object */
		argv[0] = OBJECT_TO_JSVAL(jsevent);

		ret = JS_CallFunctionName(cx,
					  JS_GetGlobalObject(cx),
					  "dispatchEvent",
					  1,
					  argv,
					  &rval);

		disable_heartbeat(hb);

	} else {
		JSLOG("Dispatching event %s at %p", type, node);

		/* create and initialise and event object */
		exc = dom_string_create((unsigned char*)type,
					strlen(type),
					&type_dom);
		if (exc != DOM_NO_ERR) {
			return false;
		}

		exc = dom_event_create(doc, &event);
		if (exc != DOM_NO_ERR) {
			return false;
		}

		exc = dom_event_init(event, type_dom, true, true);
		dom_string_unref(type_dom);
		if (exc != DOM_NO_ERR) {
			return false;
		}

		dom_event_target_dispatch_event(node, event, &ret);

	}

	if (ret == JS_TRUE) {
		return true;
	}
	return false;
}

struct js_dom_event_private {
	JSContext *cx; /* javascript context */
	jsval funcval; /* javascript function to call */
	struct dom_node *node; /* dom node event listening on */
	dom_string *type; /* event type */
	dom_event_listener *listener; /* the listener containing this */
};

static void
js_dom_event_listener(struct dom_event *event, void *pw)
{
	struct js_dom_event_private *private = pw;
	jsval event_argv[1];
	jsval event_rval;
	JSObject *jsevent;

	JSLOG("WOOT dom event with %p", private);

	if (!JSVAL_IS_VOID(private->funcval)) {
		jsevent = jsapi_new_Event(private->cx, NULL, NULL, event);
		if (jsevent != NULL) {

			/* dispatch event at the window object */
			event_argv[0] = OBJECT_TO_JSVAL(jsevent);

			JS_CallFunctionValue(private->cx,
					     NULL,
					     private->funcval,
					     1,
					     event_argv,
					     &event_rval);
		}
	}
}

/* add a listener to a dom node
 *
 * 1. Create a dom_event_listener From a handle_event function pointer
 *    and a private word In a document context
 *
 * 2. Register for your events on a target (dom nodes are targets)
 *    dom_event_target_add_event_listener(node, evt_name, listener,
 *    capture_or_not)
 *
 */

bool
js_dom_event_add_listener(jscontext *ctx,
			  struct dom_document *document,
			  struct dom_node *node,
			  struct dom_string *event_type_dom,
			  void *js_funcval)
{
	JSContext *cx = (JSContext *)ctx;
	dom_exception exc;
	struct js_dom_event_private *private;

	private = malloc(sizeof(struct js_dom_event_private));
	if (private == NULL) {
		return false;
	}

	exc = dom_event_listener_create(document,
					js_dom_event_listener,
					private,
					&private->listener);
	if (exc != DOM_NO_ERR) {
		return false;
	}

	private->cx = cx;
	private->funcval = *(jsval *)js_funcval;
	private->node = node;
	private->type = event_type_dom;

	JSLOG("adding %p to listener", private);

	JSAPI_ADD_VALUE_ROOT(cx, &private->funcval);
	exc = dom_event_target_add_event_listener(private->node,
						  private->type,
						  private->listener,
						  true);
	if (exc != DOM_NO_ERR) {
		JSLOG("failed to add listener");
		JSAPI_REMOVE_VALUE_ROOT(cx, &private->funcval);
	}

	return true;
}
