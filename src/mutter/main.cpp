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
#include <iomanip>
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

void
user_list(void)
{
	NameMap users;
	ServerPrx server;
	
	server = meta->getServer(serverId, ctx);
	users = server->getRegisteredUsers("", ctx);
	
	cerr << "ID #:   " << " " << "Username:" << endl;
	cerr << "~~~~~~~~ ~~~~~~~~~" << endl;
	
	for (NameMap::iterator ii=users.begin(); ii != users.end(); ii++) {
		cout << setw(8) << right << (*ii).first << " ";
		cout << left << (*ii).second << endl;
	}
}

void
serv_list(void)
{
	int i, id;
	string name, host, port;
	vector<ServerPrx> servers = meta->getAllServers(ctx);
	
	cerr << left << "ID    " << setw(40) << "Server Name" << setw(5) << " On?"
		<< "Host" << endl;
	cerr << left << "~~~~~ " << setw(40) << "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~"
		<< setw(5) << " ~~~" << "~~~~~~~~~~~~~~~~~~~" << endl;
	
	for (i=0; i < (int)servers.size(); i++)
	{
		id = servers[i]->id(ctx);
		name = servers[i]->getConf("registername", ctx);
		host = servers[i]->getConf("host", ctx);
		port = servers[i]->getConf("port", ctx);
		
		cout << setw(5) << right << id << " " << setw(40) << left << name;
		if (servers[i]->isRunning(ctx))
			cout << " On  ";
		else
			cout << " Off ";
		
		cout << host << ":" << port << endl;
	}
}

int
main (int argc, char **argv)
{
	string iceProxy;
	string iceSecret;
	string configKey;
	string configValue;
	char *username;
	
	int action;
	int ret;
	
	Ice::CommunicatorPtr ic;
	
	/*
	 * Start with some sensible defaults
	 */
	iceProxy = "Meta:tcp -h localhost -p 6502";
	serverId = 1;
	
	action = 0;
	ret = 0;
	
	/*
	 * Use boost to parse command line arguments
	 */
	try {
		po::options_description desc("Usage:");
		desc.add_options()
			("help", "Produce this help message.")
			("sid,s", po::value<int>(), "Set virtual server ID.")
			("ice-proxy,i", po::value<string>(), "Set the proxy to use for ICE.")
			("ice-secret,z", po::value<string>(), "Set the secret given to Murmur.")
			("config,C", po::value<string>(), "Peek/Poke Configuration setting <arg>")
			("value,V", po::value<string>(), "Data to be poked into Configuration setting (requires -C)")
			("list-servers,L", "List all virtual servers on the Murmur.")
			("list-users,l", "List all registered users on a virtual server.")
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
		
		if (vm.count("ice-secret")) {
			iceSecret = vm["ice-secret"].as<string>();
		}
		
		if (vm.count("ice-proxy")) {
			iceProxy = vm["ice-proxy"].as<string>();
		}
		
		if (vm.count("config")) {
			action = ACT_CONFPEEK;
			configKey = vm["config"].as<string>();
		}

		if (vm.count("value")) {
			action = ACT_CONFPOKE;
			configValue = vm["value"].as<string>();
		}
		
		if (vm.count("list-servers")) {
			action = ACT_SERVLIST;
		}
		
		if (vm.count("list-users")) {
			action = ACT_USERLIST;
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
		if (iceSecret.length() > 0)
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
		case ACT_SERVLIST:
			serv_list();
			break;
		case ACT_USERLIST:
			user_list();
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
