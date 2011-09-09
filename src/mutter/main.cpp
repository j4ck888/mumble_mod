/* Copyright (C) 2011, Jamie Fraser <jamie.f@mumbledog.com>

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

#include <boost/program_options.hpp>
namespace po = boost::program_options;

#include <iostream>
#include <iterator>
#include <Ice/Ice.h>
#include <Murmur.h>

#include <mutter.h>

using namespace std;
using namespace Murmur;

MetaPrx meta;
Ice::Context ctx;
int serverId;

void
config_peek(string key)
{
	string value;
	ServerPrx server;
	
	server = meta->getServer(serverId, ctx);
	value = server->getConf(key, ctx);
	cout << key << "=" << value << endl;
}

void
config_poke(string key, string val)
{
	string value;
	ServerPrx server;
	
	server = meta->getServer(serverId, ctx);
	server->setConf(key, val, ctx);
	value = server->getConf(key, ctx);
	cout << key << "=" << value << endl;
}

int
main (int argc, char **argv)
{
	char *iceProxy;
	char *iceSecret;
	string configKey;
	string configValue;
	char *username;
	
	int action;
	int ret;
	
	Ice::CommunicatorPtr ic;
	
	/*
	 * Start with some sensible defaults
	 */
	iceProxy = (char *)"Meta:tcp -h localhost -p 6502";
	iceSecret = NULL;
	serverId = 1;
	
	action = 0;
	ret = 0;
	
	/*
	 * Use boost to parse command line arguments
	 */
	try {
		po::options_description desc("Usage:");
		desc.add_options()
			("help", "produce help message")
			("sid", po::value<int>(), "Set virtual server ID")
			("config,C", po::value<string>(), "Peek/Poke Configuration setting <arg>")
			("value,V", po::value<string>(), "Data to be poked into Configuration setting (requires -C, use '-' for stdin)")
		;
		
		po::variables_map vm;
		po::store(po::parse_command_line(argc, argv, desc), vm);
		po::notify(vm);
		
		if (vm.count("help")) {
			cout << desc << endl;
			return (-1);
		}
		
		if (vm.count("sid")) {
			serverId = vm["compression"].as<int>();
		}
		
		if (vm.count("config")) {
			action = ACT_CONFPEEK;
			configKey = vm["config"].as<string>();
		}

		if (vm.count("value")) {
			action = ACT_CONFPOKE;
			configValue = vm["value"].as<string>();
		}
	}
	catch (exception &e) {
		cerr << "error: " << e.what() << endl;
		return (-1);
	}
	catch (...) {
		cerr << "ouch!" << endl;
		return (-1);
	}
	
	/*
	 * Start Ice stuff
	 */
	if (!action) {
		cerr << "Nothing to do - exiting. See mutter --help" << endl;
		return (-1);
	} else try {
		ic = Ice::initialize(argc, argv);
		Ice::ObjectPrx base = ic->stringToProxy(iceProxy);
		if (iceSecret)
			ctx["secret"] = (string)iceSecret;
		
		meta = MetaPrx::checkedCast(base);
		if (!meta)
			throw "Ice Error: Invalid Proxy";
		
		/*
		 * Actually perform the action
		 */
		
		switch (action)
		{
		case ACT_CONFPEEK:
			config_peek(configKey);
			break;
		case ACT_CONFPOKE:
			config_poke(configKey, configValue);
			break;
		}
	} catch (const Ice::Exception& ex) {
		cerr << ex << endl;
		ret = -1;
	} catch (const char* msg) {
		cerr << msg << endl;
		ret = -1;
	}
	
	/*
	 * Show's over, clean up after Ice
	 */
	if (ic)
		ic->destroy();
	
	return (ret);
}
