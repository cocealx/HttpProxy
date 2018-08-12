#include <windows.h>

#include <map>
#include <string>
#include <fstream>

using std::map;
using std::string;
using std::ifstream;

map<string,ULONG>	db;	// dns cache database
CRITICAL_SECTION spinlock;


int InitDNS(const char* fileName)
{
	InitializeCriticalSection(&spinlock);
	ifstream fileDNS(fileName);
	string ip;
	string host;
	while (fileDNS.good())
	{
		fileDNS >> ip >> host;
		db[host] = inet_addr(ip.c_str());
	}

	return 0;
}

// Interlocked
ULONG QueryIPByHostName(const char * hostName)
{
	EnterCriticalSection(&spinlock);
		ULONG ret = 0;
		map<string,ULONG>::iterator it = db.find(hostName);
		if (it == db.end() )
		{
			HOSTENT *hostent=gethostbyname(hostName);
			if (hostent)
			{
				in_addr inad = *( (in_addr*) *hostent->h_addr_list);
				ret = inad.s_addr;
				db[hostName] = ret;
			}
		} else {
			ret = it->second;
		}
	LeaveCriticalSection(&spinlock);
	return ret;
}