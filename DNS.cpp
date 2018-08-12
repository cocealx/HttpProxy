#include<stdlib.h>
#include<stdio.h>
#include<unistd.h>
#include<string.h>
#include<pthread.h>
#include<sys/socket.h>
#include<sys/types.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<signal.h>
#include<netdb.h>
#include <map>
#include <string>
#include <fstream>
typedef unsigned int ULONG; 
#define SOCKET_ERROR
using std::map;
using std::string;
using std::ifstream;

//dns缓存数据库
map<string,unsigned int>	db;	// dns cache database

//可以理解为锁定一个资源
pthread_mutex_t lock;

//////把文件中缓存的dns和对应的ip读入到map里面
int InitDNS(const char* fileName)
{
	//初始化锁资源
	if(pthread_mutex_init(&lock,NULL)!=0)
		return -1;
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

/////通过主机名查看缓存或者通过域名解析
ULONG QueryIPByHostName(const char * hostName)
{
	//申请锁资源
		pthread_mutex_lock(&lock);
		ULONG ret = 0;
		map<string,ULONG>::iterator it = db.find(hostName);
		//如果在map里面没有找到说明还没有缓存，进行一个缓存
		if (it == db.end())
		{
			//gethostbyname()返回对应于给定主机名的包含主机名字和地址信息的hostent结构指针
			//struct hostent
			//{
			//	char *h_name;
			//	char ** h_aliases;
			//	short h_addrtype;
			//	short h_length;
			//	char ** h_addr_list;
			//};
			//通过函数调用去解析网站对应的ip
			struct hostent*hostent1=gethostbyname(hostName);
			if (hostent1)
			{
				////保存ip
				in_addr inad = *((in_addr*) *hostent1->h_addr_list);
				ret = inad.s_addr;
				//添加到map里面，下次在访问的时候就不用在通过函数gethonsbyname()去解析了
				db[hostName] = ret;
			}
		} 
		//如果已经缓存了，直接把ip返回去
		else
		{
			ret = it->second;
		}
		///释放锁资源
	pthread_mutex_unlock(&lock);
	return ret;
}
