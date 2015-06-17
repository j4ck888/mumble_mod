/* Copyright (C) 2015, Tim Cooper <tim.cooper@layeh.com>

   All rights reserved.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

   - Redistributions of source code must retain the above copyright notice,
     this list of conditions and the following disclaimer.
   - Redistributions in binary form must reproduce the above copyright notice,
     this list of conditions and the following disclaimer in the documentation
     and/or other materials provided with the distribution.
   - Neither the name of the Mumble Developers nor the names of its
     contributors may be used to endorse or promote products derived from this
     software without specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR
   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <map>
#include <string>
#include <sstream>

#include <google/protobuf/compiler/code_generator.h>
#include <google/protobuf/compiler/plugin.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/io/printer.h>

#include <boost/algorithm/string/replace.hpp>

using namespace std;

using namespace google::protobuf;
using namespace google::protobuf::compiler;
using namespace google::protobuf::io;

const char *CSingle_SSingle = R"(
struct $service$_$method$ : public RPCCall {
	MurmurRPCImpl *rpc;
	::$ns$::$service$::AsyncService *service;

	::grpc::ServerContext context;
	::$in$ request;
	::grpc::ServerAsyncResponseWriter < ::$out$ > response;

	$service$_$method$(MurmurRPCImpl *rpc, ::$ns$::$service$::AsyncService *service) : rpc(rpc), service(service), response(&context) {
	}

	void impl(bool ok);

	void finish(bool) {
		delete this;
	}

	::boost::function<void(bool)> *done() {
		auto done_fn = ::boost::bind(&$service$_$method$::finish, this, _1);
		return new ::boost::function<void(bool)>(done_fn);
	}

	void error(::grpc::Status &err) {
		response.FinishWithError(err, this->done());
	}

	void handle(bool ok) {
		$service$_$method$::create(this->rpc, this->service);
		auto ie = new RPCExecEvent(::boost::bind(&$service$_$method$::impl, this, ok), this);
		QCoreApplication::instance()->postEvent(rpc, ie);
	}

	static void create(MurmurRPCImpl *rpc, ::$ns$::$service$::AsyncService *service) {
		auto call = new $service$_$method$(rpc, service);
		auto fn = ::boost::bind(&$service$_$method$::handle, call, _1);
		auto fn_ptr = new ::boost::function<void(bool)>(fn);
		service->Request$method$(&call->context, &call->request, &call->response, rpc->mCQ.get(), rpc->mCQ.get(), fn_ptr);
	}
};
)";

const char *CSingle_SStream = R"(
struct $service$_$method$ : public RPCCall {
	MurmurRPCImpl *rpc;
	::$ns$::$service$::AsyncService *service;

	::grpc::ServerContext context;
	::$in$ request;
	::grpc::ServerAsyncWriter < ::$out$ > response;

	$service$_$method$(MurmurRPCImpl *rpc, ::$ns$::$service$::AsyncService *service) : rpc(rpc), service(service), response(&context) {
	}

	void impl(bool ok);

	void finish(bool) {
		delete this;
	}

	::boost::function<void(bool)> *done() {
		auto done_fn = ::boost::bind(&$service$_$method$::finish, this, _1);
		return new ::boost::function<void(bool)>(done_fn);
	}

	::boost::function<void(bool)> *callback(::boost::function<void($service$_$method$ *, bool)> cb) {
		auto fn = ::boost::bind(&$service$_$method$::callbackAction, this, cb, _1);
		return new ::boost::function<void(bool)>(fn);
	}

	void error(::grpc::Status &err) {
		response.Finish(err, this->done());
	}

	void handle(bool ok) {
		$service$_$method$::create(this->rpc, this->service);
		auto ie = new RPCExecEvent(::boost::bind(&$service$_$method$::impl, this, ok), this);
		QCoreApplication::instance()->postEvent(rpc, ie);
	}

	static void create(MurmurRPCImpl *rpc, ::$ns$::$service$::AsyncService *service) {
		auto call = new $service$_$method$(rpc, service);
		auto fn = ::boost::bind(&$service$_$method$::handle, call, _1);
		auto fn_ptr = new ::boost::function<void(bool)>(fn);
		service->Request$method$(&call->context, &call->request, &call->response, rpc->mCQ.get(), rpc->mCQ.get(), fn_ptr);
	}

private:

	void callbackAction(::boost::function<void($service$_$method$ *, bool)> cb, bool ok) {
		auto ie = new RPCExecEvent(::boost::bind(cb, this, ok), this);
		QCoreApplication::instance()->postEvent(rpc, ie);
	}
};
)";

const char *CStream_SSingle = R"(
struct $service$_$method$ : public RPCCall {
	MurmurRPCImpl *rpc;
	::$ns$::$service$::AsyncService *service;

	::grpc::ServerContext context;
	::grpc::ServerAsyncReader< ::$out$, ::$in$ > stream;

	$service$_$method$(MurmurRPCImpl *rpc, ::$ns$::$service$::AsyncService *service) : rpc(rpc), service(service), stream(&context) {
	}

	void impl(bool ok);

	void finish(bool) {
		delete this;
	}

	::boost::function<void(bool)> *done() {
		auto done_fn = ::boost::bind(&$service$_$method$::finish, this, _1);
		return new ::boost::function<void(bool)>(done_fn);
	}

	void error(::grpc::Status &err) {
		stream.FinishWithError(err, this->done());
	}

	void handle(bool ok) {
		$service$_$method$::create(this->rpc, this->service);
		auto ie = new RPCExecEvent(::boost::bind(&$service$_$method$::impl, this, ok), this);
		QCoreApplication::instance()->postEvent(rpc, ie);
	}

	static void create(MurmurRPCImpl *rpc, ::$ns$::$service$::AsyncService *service) {
		auto call = new $service$_$method$(rpc, service);
		auto fn = ::boost::bind(&$service$_$method$::handle, call, _1);
		auto fn_ptr = new ::boost::function<void(bool)>(fn);
		service->Request$method$(&call->context, &call->stream, rpc->mCQ.get(), rpc->mCQ.get(), fn_ptr);
	}
};
)";

const char *CStream_SStream = R"(
struct $service$_$method$ : public RPCCall {
	MurmurRPCImpl *rpc;
	::$ns$::$service$::AsyncService *service;

	::grpc::ServerContext context;
	::grpc::ServerAsyncReaderWriter< ::$out$, ::$in$ > stream;

	$service$_$method$(MurmurRPCImpl *rpc, ::$ns$::$service$::AsyncService *service) : rpc(rpc), service(service), stream(&context) {
	}

	void impl(bool ok);

	void finish(bool) {
		delete this;
	}

	::boost::function<void(bool)> *done() {
		auto done_fn = ::boost::bind(&$service$_$method$::finish, this, _1);
		return new ::boost::function<void(bool)>(done_fn);
	}

	void error(::grpc::Status &err) {
		stream.Finish(err, this->done());
	}

	void handle(bool ok) {
		$service$_$method$::create(this->rpc, this->service);
		auto ie = new RPCExecEvent(::boost::bind(&$service$_$method$::impl, this, ok), this);
		QCoreApplication::instance()->postEvent(rpc, ie);
	}

	static void create(MurmurRPCImpl *rpc, ::$ns$::$service$::AsyncService *service) {
		auto call = new $service$_$method$(rpc, service);
		auto fn = ::boost::bind(&$service$_$method$::handle, call, _1);
		auto fn_ptr = new ::boost::function<void(bool)>(fn);
		service->Request$method$(&call->context, &call->stream, rpc->mCQ.get(), rpc->mCQ.get(), fn_ptr);
	}
};
)";

class WrapperGenerator : public CodeGenerator {
	bool Generate(const FileDescriptor *input, const string &parameter, GeneratorContext *context, string *error) const {
		auto cpp_source = context->Open(input->name() + ".Wrapper.cpp");
		Printer cpp(cpp_source, '$');

		cpp.Print("// DO NOT EDIT!\n");
		cpp.Print("// Auto generated by scripts/protoc-gen-grpcwrapper\n");
		cpp.Print("\n");

		auto ns = ConvertDot(input->package());

		int namespaces = 1;
		{
			stringstream stream(input->package());
			string current;
			while (getline(stream, current, '.')) {
				cpp.Print("namespace $current$ {\n", "current", current);
				namespaces++;
			}
		}
		cpp.Print("namespace Wrapper {\n", "ns", ns);

		map<string, string> tpl;
		tpl["ns"] = ns;

		for (int i = 0; i < input->service_count(); i++) {
			auto service = input->service(i);
			tpl["service"] = service->name();

			for (int j = 0; j < service->method_count(); j++) {
				auto method = service->method(j);
				tpl["method"] = method->name();
				tpl["in"] = CompiledName(ns, method->input_type());
				tpl["out"] = CompiledName(ns, method->output_type());

				const char *template_str;
				if (method->client_streaming()) {
					if (method->server_streaming()) {
						template_str = CStream_SStream;
					} else {
						template_str = CStream_SSingle;
					}
				} else {
					if (method->server_streaming()) {
						template_str = CSingle_SStream;
					} else {
						template_str = CSingle_SSingle;
					}
				}
				cpp.Print(tpl, template_str);
			}

			cpp.Print(tpl, "void $service$_Init(MurmurRPCImpl *impl, ::$ns$::$service$::AsyncService *service) {\n");
			for (int j = 0; j < service->method_count(); j++) {
				auto method = service->method(j);
				tpl["method"] = method->name();
				cpp.Print(tpl, "\t$service$_$method$::create(impl, service);\n");
			}
			cpp.Print("}\n");
		}

		cpp.Print("\n");
		while (namespaces--) {
			cpp.Print("}\n");
		}

		return true;
	}

	static string CompiledName(const string &ns, const Descriptor *type) {
		string s = type->name();
		for (type = type->containing_type(); type; type = type->containing_type()) {
			s = type->name() + "_" + s;
		}
		return ns + "::" + s;
	}

	static string ConvertDot(const string &str, const string &replace = "::") {
		return boost::replace_all_copy(str, ".", replace);
	}
};

int main(int argc, char *argv[]) {
	WrapperGenerator generator;
	return PluginMain(argc, argv, &generator);
}
