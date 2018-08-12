#include <iostream>
//#include <process.h>
#include "DNS.h"
#include"pthread_pool.h"
using namespace std;
#define MAX_HOST_NAME 512
#define MAXSIZE 65536
//Ĭ�϶˿ں�
#define DEFAULTPORT 80
#define INVALID_SOCKET -1

typedef int SOCKET;
#define HEADLEN 7  // http://
//#define GET_METHOD_LEN 4// GET_SPACE

int LISTENPORT = 8080;	// default port

////һ���ṹ���Ա�б�����Ŀͻ��˵��׽��ֺͽ�Ҫ���ʵķ���˵��׽���
struct RECVPARAM
{
	SOCKET ClientSocket;
	SOCKET ServerSocket;
};
SOCKET ProxyServer;
static pthread_pool_t*pool=NULL;

bool InitHost(SOCKET *ServerSocket,char *HostName,int Port);
bool SendWebRequest(RECVPARAM *lpParameter, char *SendBuf, char *RecvBuf, int len);
bool InitSocket(void);
int RecvRequest(SOCKET s, char* buf, int bufSize);

///��������
int SendData(SOCKET s, const char* buf, int bufSize)
{
	int pos = 0;
	while (pos < bufSize)
	{
		int ret = send(s, buf+pos,bufSize-pos,0);
		if (ret > 0) {
			pos += ret;
		} else {
			return ret;
		}
	}
	return pos;
}
//����connect����
int ConnectServer(SOCKET& s, char* recvBuf,int len)
{
	//////****************��������ĸ�ʽ*************////////
	//CONNECT www.web - tinker.com:80 HTTP / 1.1
	//Host : www.web - tinker.com : 80
	//	   Proxy - Connection : Keep - Alive
	//	   Proxy - Authorization : Basic *
	//	   Content - Length : 0
	// ��	recvBuf ���� host �� port

	//���������������
	char strHost[MAX_HOST_NAME] = {0};
	//���������ip
	char strPort[8] = {0};	// < 65535
	int port = 80;
	//���ܣ���buf��ָ�ڴ������ǰcount���ֽڲ����ַ�ch��
	//˵��������һ�������ַ�chʱֹͣ���ҡ�����ɹ�������ָ���ַ�ch��ָ�룻���򷵻�NULL��
	char* sp = (char*)(memchr(recvBuf+8,' ',len-8));
	if (!sp) {return -1;}
	char* pt = (char*)(memchr(recvBuf+8,':',sp-recvBuf-8 ));
	//���û�ж˿ںž�Ĭ����80
	if (pt)
	{
		int l = sp-pt-1;
		//����˿ڹ��󷵻ظ�1
		if (l >= 8) { return -1; }
		memcpy(strPort,pt+1,l);
		//��ö˿ں�
		port = atoi(strPort);
		memcpy(strHost,recvBuf+8,pt-recvBuf-8);
	} else {
		memcpy(strHost,recvBuf+8,sp-recvBuf-8);	
	}
	//��ѯ������Ӧ��ip,�����׽�����ͻ��˷���web������
	return InitHost(&s,strHost,port) - 1;
}
///////����һ���򵥵����ݽ���
void*  ExcThread(void * lpp)
{
	SOCKET s1 = ((RECVPARAM*)lpp)->ClientSocket;
	SOCKET s2 = ((RECVPARAM*)lpp)->ServerSocket;
	char buf[MAXSIZE];
	// ���ϰ�s1������ת�Ƶ�s2;
	while(1)
	{
		int ret = recv(s1,buf,MAXSIZE,0);
		if (ret <=0 ) { return (void*)ret;}
		ret = SendData(s2,buf,ret);
		if (ret <=0 ) {return (void*)ret;}
	}
	return 0;
}
////����ͻ��˺ͱ����ʵķ�����֮������ݽ���
int  ExchangeData(RECVPARAM* svc)
{					
	RECVPARAM th1 = *svc;
	RECVPARAM th2 = {svc->ServerSocket,svc->ClientSocket};
	///////����һ���򵥵����ݽ������ϰѿͻ��˵�����ת������Ҫ���ʵķ�����
	pthread_t tid1;
	if (pthread_create(&tid1,NULL,ExcThread,(void*)&th1)) {goto err1;}
	///////����һ���򵥵����ݽ������ϰѱ����ʵķ���������ת�������ʵĿͻ���
	//���h2�����߳�ʧ��
	pthread_t tid2;
	if (pthread_create(&tid2,NULL,ExcThread,(void*)&th2)) {
		pthread_cancel(tid1);

	}
	//�����ȴ������̹߳������
	pthread_join(tid1,NULL);
	pthread_join(tid2,NULL);
err1:
	shutdown(svc->ServerSocket,2);	// SD_BOTH
	shutdown(svc->ClientSocket,2);	// SD_BOTH
	return 0;
}

//////���ͻ�����Ӧ�����Ѿ�����
int PreResponse(RECVPARAM* svc)
{
	const char response[] = "HTTP/1.1 200 Connection established\r\n"
		"Proxy-agent: HTTP Proxy Lite /0.2\r\n\r\n";   // �����Ǵ����������ƣ��������ʲô�������"
	int ret = SendData(svc->ClientSocket,response,sizeof(response)-1);
	if (ret <= 0){ return -2;}
	return 0;
}
//////����ͻ���������߳���ں���
void*  ProxyThread(void * lpParameter)
{
	char RecvBuf[MAXSIZE] = {0};
	char SendBuf[MAXSIZE] = {0};
	int retval = RecvRequest(((RECVPARAM*)lpParameter)->ClientSocket,RecvBuf,MAXSIZE);
	//*********************//
	//cout<<"�õ�Recvbuf�ǣ�"<<endl<<RecvBuf<<endl;
	//*********************//
	if((retval==-1)||(retval==0)) {goto error;}
	//�����connect����
	if ( strncmp("CONNECT ",RecvBuf,8) == 0) // CONNECT ����
	{
		cout << "Request connection" << endl;
		//�����ͻ����������󣬽����׽���
		if (ConnectServer( ((RECVPARAM*)lpParameter)->ServerSocket, RecvBuf, retval) < 0 ) {goto err2;}
		//////���ͻ�����Ӧ�����Ѿ�����
		if (PreResponse((RECVPARAM*)lpParameter )< 0 ) {goto error;}
		////����ͻ��˺ͱ����ʵķ�����֮������ݽ���
		ExchangeData((RECVPARAM*)lpParameter);	// �����Ѿ�������ֻ�轻������
	}
	//�������connect����ֱ��ת��
	else	// ֱ��ת��
	{
		SendWebRequest((RECVPARAM*)lpParameter, SendBuf, RecvBuf, retval);
	}

error:
	close(((RECVPARAM*)lpParameter)->ServerSocket);
err2:
	close(((RECVPARAM*)lpParameter)->ClientSocket);
	delete (RECVPARAM*)lpParameter;
	return 0;
} 


//��ѯ������Ӧ��ip,�����׽�����ͻ��˷���web������
bool InitHost(SOCKET *ServerSocket,char *HostName,int Port)
{
	sockaddr_in Server;
	Server.sin_family = AF_INET;
	Server.sin_port = htons(Port);
	/////ͨ���������鿴�������ͨ����������
	Server.sin_addr.s_addr = QueryIPByHostName(HostName);

	//����һ���׽��֣���ͻ��˽��з���web��վ
	*ServerSocket = socket(AF_INET,SOCK_STREAM,0);
	if (*ServerSocket==INVALID_SOCKET)
		return false;
	////����
	if (connect(*ServerSocket,(struct sockaddr *)&Server,sizeof(Server))<0)
	{
		close(*ServerSocket);
		return false;
	}

	//cout << "CONNECT " << HostName << ":" << Port << endl;

	return true;
}
///�ӿͻ��˽��ܴ������󣬲���ȡ��������
int RecvRequest(SOCKET s, char* buf, int bufSize)
{
	int len = 0;
	char * prf = buf;
	while (len<bufSize)
	{
		int ret = recv(s,buf+len,bufSize-len,0);
		if (ret > 0) {
			len += ret;
		} else {
			return ret;
		}

		// Find End Tag: \r\n\r\n
		if ( len > 4 )
		{
			if ( strstr(prf,"\r\n\r\n") )
			{
				break;
			} 
			else 
			{
				prf = buf + len - 4;
			}
		}
	}
	return len;
}

bool SendWebRequest(RECVPARAM *lpParameter, char *SendBuf, char *RecvBuf, int DataLen)
{
	char HostName[MAX_HOST_NAME] = {0};
	int Port = DEFAULTPORT;

	char * line = strstr(RecvBuf,"\r\n");	// һ���� ��Ϊrequest head �Ѿ��ж���\r\n

	char * UrlBegin = strchr(RecvBuf,' ');
	if (!UrlBegin) {return 0;}
	char * PathBegin = strchr(UrlBegin+1+HEADLEN,'/');
	if (!PathBegin) {return 0;}

	//���� http://baike.baidu.com:80/item/strchr/10985184?fr=aladdin

	char * PortBegin = (char*)(memchr(UrlBegin+1+HEADLEN,':',PathBegin-UrlBegin-HEADLEN) );
	char * HostEnd = PathBegin;
	if (PortBegin)	// �ж˿�
	{
		HostEnd = PortBegin;
		char BufPort[64] = {0};
		memcpy(BufPort,PortBegin+1,PathBegin-PortBegin-1);
		Port = atoi(BufPort);
	}
	//����������
	memcpy(HostName,UrlBegin+1+HEADLEN, HostEnd-UrlBegin-1-HEADLEN);

	//Trace ������¼�ͻ��˵�����
	char lineBuf[0x1000] = {0};
	int leng = line-RecvBuf;
	if (leng < sizeof(lineBuf) ) 
	{
		memcpy(lineBuf,RecvBuf,leng);
	} else {
		const static int lenc = 50;
		memcpy(lineBuf,RecvBuf,lenc);
		strcpy(lineBuf+lenc," ... ");
		memcpy(lineBuf+lenc+5,line-16,16);
	}
	//cout << "XXXXXXXXXXXXXXX" << endl;
	//cout << "\t"<<lineBuf<<endl;
	//cout << "XXXXXXXXXXXXXXX" << endl;
	//��ʾHTTP������	
	//cout << "HTTP " << HostName <<":"<<Port<<endl;

	if(!InitHost(&(lpParameter->ServerSocket),HostName,Port)) return 0;
	//cout << "recvbuf" << RecvBuf << endl;
	memcpy(SendBuf,RecvBuf, UrlBegin-RecvBuf+1 ); // ��дmethod
	memcpy(SendBuf+(UrlBegin-RecvBuf)+1,PathBegin,RecvBuf+DataLen-PathBegin);	// ��дʣ������

	char * HTTPTag = strstr(SendBuf+(UrlBegin-RecvBuf)+1," HTTP/1.1\r\n");
	//if (HTTPTag) { HTTPTag[8] = '0'; }	// ǿ��ʹ��http/1.0 ��ֹKeep-Alive

	size_t TotalLen = UrlBegin+1+DataLen-PathBegin;

	//cout<<"ת����web���ǣ�"<<endl<<SendBuf<<endl;

	if( SendData(lpParameter->ServerSocket,SendBuf,TotalLen) <= 0){ return 0; }

	ExchangeData( lpParameter );
	return true;
}

struct sockaddr_in Server;
const char *filename = "host.txt";
extern map<string,unsigned int> db;
void exithandler(int arg)
{
	pthreadpool_destory(pool);
	map<string,unsigned int>::iterator it=db.begin();
	std::ofstream fout(filename);
	while(it!=db.end())
	{
		fout<<it->second<<" "<<(it->first).c_str()<<endl;
		++it;
	}
	fout<<flush;
	fout.close();
	printf("process exit\n");
	exit(0);
}
void sigpipehandler(int arg)
{

}
	

int main(int argc, char* argv[])
{
	cout 
		<< "HTTP Proxy Lite" << endl
		<< "Version: 0.2" << endl
		<< "By tk-bear" << endl << endl
		<< "Usage:  HttpProxy [-p port] [-f dnsfile]    \n\teg."<<argv[0]<<"-p 8000 -f ./dns.txt" << endl
		<< "-----DNS File Pattern------"<< endl
		<< "127.0.0.1	tk.bear.com" << endl
		<< "0.0.0.0		domain" << endl
		<< "---------------------------" << endl
		<<endl;
	struct sigaction act;

	act.sa_handler=sigpipehandler;
	sigemptyset(&act.sa_mask);
	act.sa_flags=0;
	sigaction(SIGPIPE,&act,NULL);
	
	act.sa_handler=exithandler;
	sigemptyset(&act.sa_mask);
	act.sa_flags=0;
	sigaction(SIGINT,&act,NULL);
	for (int i=1;i<argc-1;i++)
	{
		if ( strcmp("-p",argv[i])==0 )
		{
			LISTENPORT = atoi(argv[i+1]);
		} 
		else if (strcmp("-f",argv[i])==0)
		{
			filename = argv[i+1];
		}
	}
	//��ʼ��DNS����
	InitDNS(filename);

	//���������׽���
	if(InitSocket()==false)
	{
		cout<<"InitSocket failed";
		return -1;
	}

	SOCKET AcceptSocket = INVALID_SOCKET;
	RECVPARAM *lpParameter;
	//����һ���̳߳�
	pool = pthreadpool_create();
	task_t task;
	task.function=ProxyThread;
	printf("DNS File: %s\r\nListening Port: %d\r\n\r\n\r\n",filename,LISTENPORT);
	while(1)
	{
		//������������
		AcceptSocket = accept(ProxyServer, NULL, NULL);
		lpParameter = new RECVPARAM;
		lpParameter->ClientSocket = AcceptSocket;
		task.arg=(void*)lpParameter;
		//��ӵ��̳߳ص��������
		pthreadpool_addtask(pool,task);
	}
	return 0;
}

bool InitSocket(void)
{

	//�����׽���
	ProxyServer= socket(AF_INET, SOCK_STREAM, 0);
	if(INVALID_SOCKET==ProxyServer)
	{
		perror("socket");
		return false;
	}
	Server.sin_family = AF_INET;
	Server.sin_port = htons(LISTENPORT);
	Server.sin_addr.s_addr = INADDR_ANY;
	//��һ���׽���
	if(bind(ProxyServer,(struct sockaddr*)&Server, sizeof(Server))<0)
	{
		perror("bind");
		return false;
	}
	//�����׽���
	if(listen(ProxyServer,SOMAXCONN)<0)
	{
		perror("listen");
		return false;
	}
	return true;
}

