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

//dns�������ݿ�
map<string,unsigned int>	db;	// dns cache database

//�������Ϊ����һ����Դ
pthread_mutex_t lock;

//////���ļ��л����dns�Ͷ�Ӧ��ip���뵽map����
int InitDNS(const char* fileName)
{
	//��ʼ������Դ
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

/////ͨ���������鿴�������ͨ����������
ULONG QueryIPByHostName(const char * hostName)
{
	//��������Դ
		pthread_mutex_lock(&lock);
		ULONG ret = 0;
		map<string,ULONG>::iterator it = db.find(hostName);
		//�����map����û���ҵ�˵����û�л��棬����һ������
		if (it == db.end())
		{
			//gethostbyname()���ض�Ӧ�ڸ����������İ����������ֺ͵�ַ��Ϣ��hostent�ṹָ��
			//struct hostent
			//{
			//	char *h_name;
			//	char ** h_aliases;
			//	short h_addrtype;
			//	short h_length;
			//	char ** h_addr_list;
			//};
			//ͨ����������ȥ������վ��Ӧ��ip
			struct hostent*hostent1=gethostbyname(hostName);
			if (hostent1)
			{
				////����ip
				in_addr inad = *((in_addr*) *hostent1->h_addr_list);
				ret = inad.s_addr;
				//��ӵ�map���棬�´��ڷ��ʵ�ʱ��Ͳ�����ͨ������gethonsbyname()ȥ������
				db[hostName] = ret;
			}
		} 
		//����Ѿ������ˣ�ֱ�Ӱ�ip����ȥ
		else
		{
			ret = it->second;
		}
		///�ͷ�����Դ
	pthread_mutex_unlock(&lock);
	return ret;
}
