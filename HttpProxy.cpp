#include <iostream>
//#include <process.h>
#include "DNS.h"
#include"pthread_pool.h"
using namespace std;
#define MAX_HOST_NAME 512
#define MAXSIZE 65536
//默认端口号
#define DEFAULTPORT 80
#define INVALID_SOCKET -1

typedef int SOCKET;
#define HEADLEN 7  // http://
//#define GET_METHOD_LEN 4// GET_SPACE

int LISTENPORT = 8080;	// default port

////一个结构体成员有被代理的客户端的套接字和将要访问的服务端的套接字
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

///发送数据
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
//解析connect请求
int ConnectServer(SOCKET& s, char* recvBuf,int len)
{
	//////****************代理请求的格式*************////////
	//CONNECT www.web - tinker.com:80 HTTP / 1.1
	//Host : www.web - tinker.com : 80
	//	   Proxy - Connection : Keep - Alive
	//	   Proxy - Authorization : Basic *
	//	   Content - Length : 0
	// 从	recvBuf 解析 host 和 port

	//保存服务器主机名
	char strHost[MAX_HOST_NAME] = {0};
	//保存服务器ip
	char strPort[8] = {0};	// < 65535
	int port = 80;
	//功能：从buf所指内存区域的前count个字节查找字符ch。
	//说明：当第一次遇到字符ch时停止查找。如果成功，返回指向字符ch的指针；否则返回NULL。
	char* sp = (char*)(memchr(recvBuf+8,' ',len-8));
	if (!sp) {return -1;}
	char* pt = (char*)(memchr(recvBuf+8,':',sp-recvBuf-8 ));
	//如果没有端口号就默认是80
	if (pt)
	{
		int l = sp-pt-1;
		//如果端口过大返回负1
		if (l >= 8) { return -1; }
		memcpy(strPort,pt+1,l);
		//获得端口号
		port = atoi(strPort);
		memcpy(strHost,recvBuf+8,pt-recvBuf-8);
	} else {
		memcpy(strHost,recvBuf+8,sp-recvBuf-8);	
	}
	//查询域名对应的ip,建立套接字替客户端访问web服务器
	return InitHost(&s,strHost,port) - 1;
}
///////进行一个简单的数据交换
void*  ExcThread(void * lpp)
{
	SOCKET s1 = ((RECVPARAM*)lpp)->ClientSocket;
	SOCKET s2 = ((RECVPARAM*)lpp)->ServerSocket;
	char buf[MAXSIZE];
	// 不断把s1的数据转移到s2;
	while(1)
	{
		int ret = recv(s1,buf,MAXSIZE,0);
		if (ret <=0 ) { return (void*)ret;}
		ret = SendData(s2,buf,ret);
		if (ret <=0 ) {return (void*)ret;}
	}
	return 0;
}
////处理客户端和被访问的服务器之间的数据交换
int  ExchangeData(RECVPARAM* svc)
{					
	RECVPARAM th1 = *svc;
	RECVPARAM th2 = {svc->ServerSocket,svc->ClientSocket};
	///////进行一个简单的数据交换不断把客户端的数据转发给需要访问的服务器
	pthread_t tid1;
	if (pthread_create(&tid1,NULL,ExcThread,(void*)&th1)) {goto err1;}
	///////进行一个简单的数据交换不断把被访问的服务器数据转发给访问的客户端
	//如果h2创建线程失败
	pthread_t tid2;
	if (pthread_create(&tid2,NULL,ExcThread,(void*)&th2)) {
		pthread_cancel(tid1);

	}
	//阻塞等待两个线程工作完成
	pthread_join(tid1,NULL);
	pthread_join(tid2,NULL);
err1:
	shutdown(svc->ServerSocket,2);	// SD_BOTH
	shutdown(svc->ClientSocket,2);	// SD_BOTH
	return 0;
}

//////给客户端响应连接已经建立
int PreResponse(RECVPARAM* svc)
{
	const char response[] = "HTTP/1.1 200 Connection established\r\n"
		"Proxy-agent: HTTP Proxy Lite /0.2\r\n\r\n";   // 这里是代理程序的名称，看你的是什么代理软件"
	int ret = SendData(svc->ClientSocket,response,sizeof(response)-1);
	if (ret <= 0){ return -2;}
	return 0;
}
//////处理客户端请求的线程入口函数
void*  ProxyThread(void * lpParameter)
{
	char RecvBuf[MAXSIZE] = {0};
	char SendBuf[MAXSIZE] = {0};
	int retval = RecvRequest(((RECVPARAM*)lpParameter)->ClientSocket,RecvBuf,MAXSIZE);
	//*********************//
	//cout<<"得到Recvbuf是："<<endl<<RecvBuf<<endl;
	//*********************//
	if((retval==-1)||(retval==0)) {goto error;}
	//如果是connect方法
	if ( strncmp("CONNECT ",RecvBuf,8) == 0) // CONNECT 代理
	{
		cout << "Request connection" << endl;
		//解析客户端连接请求，建立套接字
		if (ConnectServer( ((RECVPARAM*)lpParameter)->ServerSocket, RecvBuf, retval) < 0 ) {goto err2;}
		//////给客户端响应连接已经建立
		if (PreResponse((RECVPARAM*)lpParameter )< 0 ) {goto error;}
		////处理客户端和被访问的服务器之间的数据交换
		ExchangeData((RECVPARAM*)lpParameter);	// 连接已经建立，只需交换数据
	}
	//如果不是connect方法直接转发
	else	// 直接转发
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


//查询域名对应的ip,建立套接字替客户端访问web服务器
bool InitHost(SOCKET *ServerSocket,char *HostName,int Port)
{
	sockaddr_in Server;
	Server.sin_family = AF_INET;
	Server.sin_port = htons(Port);
	/////通过主机名查看缓存或者通过域名解析
	Server.sin_addr.s_addr = QueryIPByHostName(HostName);

	//建立一个套接字，替客户端进行访问web网站
	*ServerSocket = socket(AF_INET,SOCK_STREAM,0);
	if (*ServerSocket==INVALID_SOCKET)
		return false;
	////连接
	if (connect(*ServerSocket,(struct sockaddr *)&Server,sizeof(Server))<0)
	{
		close(*ServerSocket);
		return false;
	}

	//cout << "CONNECT " << HostName << ":" << Port << endl;

	return true;
}
///从客户端接受代理请求，并读取请求内容
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

	char * line = strstr(RecvBuf,"\r\n");	// 一定有 因为request head 已经判定了\r\n

	char * UrlBegin = strchr(RecvBuf,' ');
	if (!UrlBegin) {return 0;}
	char * PathBegin = strchr(UrlBegin+1+HEADLEN,'/');
	if (!PathBegin) {return 0;}

	//方法 http://baike.baidu.com:80/item/strchr/10985184?fr=aladdin

	char * PortBegin = (char*)(memchr(UrlBegin+1+HEADLEN,':',PathBegin-UrlBegin-HEADLEN) );
	char * HostEnd = PathBegin;
	if (PortBegin)	// 有端口
	{
		HostEnd = PortBegin;
		char BufPort[64] = {0};
		memcpy(BufPort,PortBegin+1,PathBegin-PortBegin-1);
		Port = atoi(BufPort);
	}
	//拷贝主机名
	memcpy(HostName,UrlBegin+1+HEADLEN, HostEnd-UrlBegin-1-HEADLEN);

	//Trace 用来记录客户端的请求
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
	//显示HTTP请求行	
	//cout << "HTTP " << HostName <<":"<<Port<<endl;

	if(!InitHost(&(lpParameter->ServerSocket),HostName,Port)) return 0;
	//cout << "recvbuf" << RecvBuf << endl;
	memcpy(SendBuf,RecvBuf, UrlBegin-RecvBuf+1 ); // 填写method
	memcpy(SendBuf+(UrlBegin-RecvBuf)+1,PathBegin,RecvBuf+DataLen-PathBegin);	// 填写剩余内容

	char * HTTPTag = strstr(SendBuf+(UrlBegin-RecvBuf)+1," HTTP/1.1\r\n");
	//if (HTTPTag) { HTTPTag[8] = '0'; }	// 强制使用http/1.0 防止Keep-Alive

	size_t TotalLen = UrlBegin+1+DataLen-PathBegin;

	//cout<<"转发到web的是："<<endl<<SendBuf<<endl;

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
	//初始化DNS缓存
	InitDNS(filename);

	//创建监听套接字
	if(InitSocket()==false)
	{
		cout<<"InitSocket failed";
		return -1;
	}

	SOCKET AcceptSocket = INVALID_SOCKET;
	RECVPARAM *lpParameter;
	//创建一个线程池
	pool = pthreadpool_create();
	task_t task;
	task.function=ProxyThread;
	printf("DNS File: %s\r\nListening Port: %d\r\n\r\n\r\n",filename,LISTENPORT);
	while(1)
	{
		//接受连接请求
		AcceptSocket = accept(ProxyServer, NULL, NULL);
		lpParameter = new RECVPARAM;
		lpParameter->ClientSocket = AcceptSocket;
		task.arg=(void*)lpParameter;
		//添加到线程池的任务队列
		pthreadpool_addtask(pool,task);
	}
	return 0;
}

bool InitSocket(void)
{

	//创建套接字
	ProxyServer= socket(AF_INET, SOCK_STREAM, 0);
	if(INVALID_SOCKET==ProxyServer)
	{
		perror("socket");
		return false;
	}
	Server.sin_family = AF_INET;
	Server.sin_port = htons(LISTENPORT);
	Server.sin_addr.s_addr = INADDR_ANY;
	//绑定一个套接字
	if(bind(ProxyServer,(struct sockaddr*)&Server, sizeof(Server))<0)
	{
		perror("bind");
		return false;
	}
	//监听套接字
	if(listen(ProxyServer,SOMAXCONN)<0)
	{
		perror("listen");
		return false;
	}
	return true;
}

