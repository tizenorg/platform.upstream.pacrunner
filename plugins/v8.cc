/*
 *
 *  PACrunner - Proxy configuration daemon
 *
 *  Copyright (C) 2010  Intel Corporation. All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>

#include <netdb.h>
#include <arpa/inet.h>
#include <linux/if_arp.h>

#include <v8.h>
#include "javascript.h"

extern "C" {
#include "pacrunner.h"
#include "js.h"
};

static struct pacrunner_proxy *current_proxy = NULL;
static v8::Persistent<v8::Context> jsctx;
static v8::Persistent<v8::Function> jsfn;
static guint gc_source = 0;

static gboolean v8_gc(gpointer user_data)
{
	const int kLongIdlePauseInMs = 1000;
	v8::Locker lck(v8::Isolate::GetCurrent());
	return !v8::Isolate::GetCurrent()->IdleNotification(kLongIdlePauseInMs);
}

static int getaddr(const char *node, char *host, size_t hostlen)
{
	struct sockaddr_in addr;
	struct ifreq ifr;
	int sk, err;

	sk = socket(PF_INET, SOCK_DGRAM, 0);
	if (sk < 0)
		return -EIO;

	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, node, sizeof(ifr.ifr_name));

	err = ioctl(sk, SIOCGIFADDR, &ifr);

	close(sk);

	if (err < 0)
		return -EIO;

	memcpy(&addr, &ifr.ifr_addr, sizeof(addr));
	snprintf(host, hostlen, "%s", inet_ntoa(addr.sin_addr));

	return 0;
}

static int resolve(const char *node, char *host, size_t hostlen)
{
	struct addrinfo *info;
	int err;

	if (getaddrinfo(node, NULL, NULL, &info) < 0)
		return -EIO;

	err = getnameinfo(info->ai_addr, info->ai_addrlen,
				host, hostlen, NULL, 0, NI_NUMERICHOST);

	freeaddrinfo(info);

	if (err < 0)
		return -EIO;

	return 0;
}

static void myipaddress(const v8::FunctionCallbackInfo<v8::Value>& args)
{
	const char *interface;
	char address[NI_MAXHOST];

	DBG("");

	if (current_proxy == NULL) {
		args.GetIsolate()->ThrowException(v8::String::NewFromUtf8(v8::Isolate::GetCurrent(), "No current proxy"));
		return;
	}

	interface = pacrunner_proxy_get_interface(current_proxy);
	if (interface == NULL) {
		args.GetIsolate()->ThrowException(v8::String::NewFromUtf8(v8::Isolate::GetCurrent(), "Error fetching interface"));
		return;
	}

	if (getaddr(interface, address, sizeof(address)) < 0) {
		args.GetIsolate()->ThrowException(v8::String::NewFromUtf8(v8::Isolate::GetCurrent(), "Error fetching IP address"));
		return;
	}

	DBG("address %s", address);

	args.GetReturnValue().Set(v8::String::NewFromUtf8(v8::Isolate::GetCurrent(), address));
}

static void dnsresolve(const v8::FunctionCallbackInfo<v8::Value>& args)
{
	char address[NI_MAXHOST];
	v8::String::Utf8Value host(args[0]);

	if (args.Length() != 1) {
		args.GetIsolate()->ThrowException(v8::String::NewFromUtf8(v8::Isolate::GetCurrent(), "Bad parameters"));
		return;
	}
		
	DBG("host %s", *host);

	if (resolve(*host, address, sizeof(address)) < 0) {
		args.GetIsolate()->ThrowException(v8::String::NewFromUtf8(v8::Isolate::GetCurrent(), "Failed to resolve"));
		return;
	}

	DBG("address %s", address);

	args.GetReturnValue().Set(v8::String::NewFromUtf8(v8::Isolate::GetCurrent(), address));
}

static void create_object(void)
{
	if (!current_proxy)
		return;

	const char *pac = pacrunner_proxy_get_script(current_proxy);
	if (!pac) {
		printf("no script\n");
		return;
	}
	v8::Handle<v8::ObjectTemplate> global = v8::ObjectTemplate::New();

	global->Set(v8::String::NewFromUtf8(v8::Isolate::GetCurrent(), "myIpAddress"),
		    v8::FunctionTemplate::New(v8::Isolate::GetCurrent(), myipaddress));
	global->Set(v8::String::NewFromUtf8(v8::Isolate::GetCurrent(), "dnsResolve"),
		    v8::FunctionTemplate::New(v8::Isolate::GetCurrent(), dnsresolve));

	v8::Handle<v8::Context> ctx = v8::Context::New(v8::Isolate::GetCurrent(), NULL, global);
	jsctx.Reset(v8::Isolate::GetCurrent(), ctx);

	v8::Context::Scope context_scope(ctx);

	v8::TryCatch exc;
	v8::Handle<v8::Script> script_scr;
	v8::Handle<v8::Value> result;

	script_scr = v8::Script::Compile(v8::String::NewFromUtf8(v8::Isolate::GetCurrent(), JAVASCRIPT_ROUTINES));
	if (script_scr.IsEmpty()) {
		v8::String::Utf8Value err(exc.Exception());
		DBG("Javascript failed to compile: %s", *err);
		jsctx.Reset();
		return;
	}
	result = script_scr->Run();
	if (exc.HasCaught()) {
		v8::String::Utf8Value err(exc.Exception());
		DBG("Javascript library failed: %s", *err);
		jsctx.Reset();
		return;
	}

	script_scr = v8::Script::Compile(v8::String::NewFromUtf8(v8::Isolate::GetCurrent(), pac));
	if (script_scr.IsEmpty()) {
		v8::String::Utf8Value err(exc.Exception());
		DBG("PAC script failed to compile: %s", *err);
		jsctx.Reset();
		return;
	}
	result = script_scr->Run();
	if (exc.HasCaught()) {
		v8::String::Utf8Value err(exc.Exception());
		DBG("PAC script failed: %s", *err);
		jsctx.Reset();
		return;
	}

	v8::Handle<v8::String> fn_name = v8::String::NewFromUtf8(v8::Isolate::GetCurrent(), "FindProxyForURL");
	v8::Handle<v8::Value> fn_val = ctx->Global()->Get(fn_name);

	if (!fn_val->IsFunction()) {
		DBG("FindProxyForUrl is not a function");
		jsctx.Reset();
		return;
	}

	v8::Handle<v8::Function> fn = v8::Handle<v8::Function>::Cast(fn_val);
	jsfn.Reset(v8::Isolate::GetCurrent(), fn);

	return;
}

static void destroy_object(void)
{
	if (!jsfn.IsEmpty()) {
		jsfn.Reset();
	}
	if (!jsctx.IsEmpty()) {
		jsctx.Reset();
	}
}

static int v8_set_proxy(struct pacrunner_proxy *proxy)
{
	v8::Locker lck(v8::Isolate::GetCurrent());

	DBG("proxy %p", proxy);

	if (current_proxy != NULL)
		destroy_object();

	current_proxy = proxy;

	if (current_proxy != NULL)
		create_object();

	if (!gc_source)
		gc_source = g_idle_add(v8_gc, NULL);

	return 0;
}


static char *v8_execute(const char *url, const char *host)
{
	v8::Locker lck(v8::Isolate::GetCurrent());

	DBG("url %s host %s", url, host);

	if (jsctx.IsEmpty() || jsfn.IsEmpty())
		return NULL;

	v8::Local<v8::Context> ctx = v8::Local<v8::Context>::New(v8::Isolate::GetCurrent(), jsctx);
	v8::Local<v8::Function> func = v8::Local<v8::Function>::New(v8::Isolate::GetCurrent(), jsfn);

	v8::Context::Scope context_scope(ctx);

	v8::Handle<v8::Value> args[2] = {
		v8::String::NewFromUtf8(v8::Isolate::GetCurrent(), url),
		v8::String::NewFromUtf8(v8::Isolate::GetCurrent(), host)
	};

	v8::Handle<v8::Value> result;
	v8::TryCatch exc;

	result = func->Call(ctx->Global(), 2, args);
	if (exc.HasCaught()) {
		v8::Handle<v8::Message> msg = exc.Message();
		int line = msg->GetLineNumber();
		v8::String::Utf8Value err(msg->Get());
		DBG("Failed to run FindProxyForUrl(): at line %d: %s",
		    line, *err);
		return NULL;
	}
	
	if (!result->IsString()) {
		DBG("FindProxyForUrl() failed to return a string");
		return NULL;
	}

	char *retval = g_strdup(*v8::String::Utf8Value(result->ToString()));

	if (!gc_source)
		gc_source = g_idle_add(v8_gc, NULL);
		
	return retval;
}

static struct pacrunner_js_driver v8_driver = {
	"v8",
	PACRUNNER_JS_PRIORITY_HIGH,
	v8_set_proxy,
	v8_execute,
};

static int v8_init(void)
{
	DBG("");

	return pacrunner_js_driver_register(&v8_driver);
}

static void v8_exit(void)
{
	const int kLongIdlePauseInMs = 1000;

	DBG("");

	pacrunner_js_driver_unregister(&v8_driver);

	v8_set_proxy(NULL);

	if (gc_source) {
		g_source_remove(gc_source);
		gc_source = 0;
	}
	while (!v8::Isolate::GetCurrent()->IdleNotification(kLongIdlePauseInMs))
		;
}

PACRUNNER_PLUGIN_DEFINE(v8, v8_init, v8_exit)
