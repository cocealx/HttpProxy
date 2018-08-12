#include <windows.h>
#include <iostream>
//#include <process.h>

#include "DNS.h"

using namespace std;
#pragma comment(lib,"Ws2_32.lib")

#define MAX_HOST_NAME 512
#define MAXSIZE 65536
#define DEFAULTPORT 80

#define HEADLEN 7// http://
//#define GET_METHOD_LEN 4// GET_SPACE

int LISTENPORT = 8080;	// default port


struct RECVPARAM
{
	SOCKET ClientSocket;
	SOCKET ServerSocket;
};

BOOL InitHost(SOCKET *ServerSocket,char *HostName,int Port);
BOOL SendWebRequest(RECVPARAM *lpParameter, char *SendBuf, char *RecvBuf, int len);
BOOL InitSocket(void);
int RecvRequest(SOCKET s, char* buf, int bufSize);


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

int ConnectServer(SOCKET& s, char* recvBuf,int len)
{
	// 从	recvBuf 解析 host 和 port
	char strHost[MAX_HOST_NAME] = {0};
	char strPort[8] = {0};	// < 65535
	int port = 80;
	char* sp = (char*)(memchr(recvBuf+8,' ',len-8));
	if (!sp) {return -1;}
	char* pt = (char*)(memchr(recvBuf+8,':',sp-recvBuf-8 ));
	if (pt)
	{
		int l = sp-pt-1;
		if (l >= 8) { return -1; }
		memcpy(strPort,pt+1,l);
		port = atoi(strPort);
		memcpy(strHost,recvBuf+8,pt-recvBuf-8);
	} else {
		memcpy(strHost,recvBuf+8,sp-recvBuf-8);	
	}

	return InitHost(&s,strHost,port) - 1;
}

DWORD WINAPI ExcThread(LPVOID lpp)
{
	SOCKET s1 = ((RECVPARAM*)lpp)->ClientSocket;
	SOCKET s2 = ((RECVPARAM*)lpp)->ServerSocket;
	char buf[MAXSIZE];
	// 不断把s1的数据转移到s2;
	while(1)
	{
		int ret = recv(s1,buf,MAXSIZE,0);
		if (ret <=0 ) { return ret;}
		ret = SendData(s2,buf,ret);
		if (ret <=0 ) {return ret;}
	}
	return 0;
}

int	ExchangeData(RECVPARAM* svc)
{					
	RECVPARAM th1 = *svc;
	RECVPARAM th2 = {svc->ServerSocket,svc->ClientSocket};
	DWORD tid;
	
	HANDLE h1 = CreateThread(0,0,ExcThread,&th1,0,&tid);
	if (!h1) {goto err1;}
	HANDLE h2 = CreateThread(0,0,ExcThread,&th2,0,&tid);
	if (!h2) {	TerminateThread(h1,0x1); goto err2;}
	HANDLE hds[2] = {h1,h2};
	WaitForMultipleObjects(2,hds,0,INFINITE);

	shutdown(svc->ServerSocket,2);	// SD_BOTH
	shutdown(svc->ClientSocket,2);	// SD_BOTH

	WaitForSingleObject(h2,INFINITE);
	CloseHandle(h2);
	WaitForSingleObject(h1,INFINITE);
err2:
	CloseHandle(h1);
err1:
	return 0;
}


int PreResponse(RECVPARAM* svc)
{
	const char response[] = "HTTP/1.1 200 Connection established\r\n"
					"Proxy-agent: HTTP Proxy Lite /0.2\r\n\r\n";   // 这里是代理程序的名称，看你的是什么代理软件"
	int ret = SendData(svc->ClientSocket,response,sizeof(response)-1);
	if (ret <= 0){ return -2;}
	return 0;
}

DWORD WINAPI ProxyThread(LPVOID lpParameter)
{
	char RecvBuf[MAXSIZE] = {0};
	char SendBuf[MAXSIZE] = {0};
	int retval = RecvRequest(((RECVPARAM*)lpParameter)->ClientSocket,RecvBuf,MAXSIZE);
	//cout<<"得到Recvbuf是："<<endl<<RecvBuf<<endl;
	if(retval == SOCKET_ERROR || retval == 0) {goto error;}
	
	if ( strncmp("CONNECT ",RecvBuf,8) == 0) // CONNECT 代理
	{
		cout << "Request connection" << endl;
		if (ConnectServer( ((RECVPARAM*)lpParameter)->ServerSocket, RecvBuf, retval) < 0 ) {goto err2;}
		if (PreResponse((RECVPARAM*)lpParameter )< 0 ) {goto error;}
		ExchangeData((RECVPARAM*)lpParameter);	// 连接已经建立，只需交换数据
	}
	else	// 直接转发
	{
		SendWebRequest((RECVPARAM*)lpParameter, SendBuf, RecvBuf, retval);
	}

error:
	closesocket(((RECVPARAM*)lpParameter)->ServerSocket);
err2:
	closesocket(((RECVPARAM*)lpParameter)->ClientSocket);
	delete  lpParameter;
	return 0;
} 

BOOL InitHost(SOCKET *ServerSocket,char *HostName,int Port)
{
	sockaddr_in Server;
	Server.sin_family = AF_INET;
	Server.sin_port = htons(Port);


	Server.sin_addr.s_addr = 	QueryIPByHostName(HostName);

	*ServerSocket = socket(AF_INET,SOCK_STREAM,0);
	if (*ServerSocket == INVALID_SOCKET)
		return FALSE;
	if (connect(*ServerSocket, (const SOCKADDR *)&Server,sizeof(Server)) == SOCKET_ERROR)
	{
		closesocket(*ServerSocket);
		return FALSE;
	}

	cout << "CONNECT " << HostName << ":" << Port << endl;

	return TRUE;
}

int RecvRequest(SOCKET s, char* buf, int bufSize)
{
	int len = 0;
	char * prf = buf;
	while (len<bufSize)
	{
		int ret = recv(s,buf,bufSize-len,0);
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
			} else {
				prf = buf + len - 4;
			}
		}
	}
	return len;
}

BOOL SendWebRequest(RECVPARAM *lpParameter, char *SendBuf, char *RecvBuf, int DataLen)
{
	char HostName[MAX_HOST_NAME] = {0};
	int Port = DEFAULTPORT;

	char * line = strstr(RecvBuf,"\r\n");	// 一定有 因为request head 已经判定了\r\n

	char * UrlBegin = strchr(RecvBuf,' ');
	if (!UrlBegin) {return 0;}
	char * PathBegin = strchr(UrlBegin+1+HEADLEN,'/');
	if (!PathBegin) {return 0;}

	
	char * PortBegin = (char*)(memchr(UrlBegin+1+HEADLEN,':',PathBegin-UrlBegin-HEADLEN) );
	char * HostEnd = PathBegin;
	if (PortBegin)	// 有端口
	{
		HostEnd = PortBegin;
		char BufPort[64] = {0};
		memcpy(BufPort,PortBegin+1,PathBegin-PortBegin-1);
		Port = atoi(BufPort);
	}
	memcpy(HostName,UrlBegin+1+HEADLEN, HostEnd-UrlBegin-1-HEADLEN);

	//Trace
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
	cout << "\t"<<lineBuf<<endl;
	// 显示HTTP请求行	
	//cout << "HTTP " << HostName <<":"<<Port<<endl;

	if(!InitHost(&(lpParameter->ServerSocket),HostName,Port)) return 0;

	memcpy(SendBuf,RecvBuf, UrlBegin-RecvBuf+1 ); // 填写method
	memcpy(SendBuf+(UrlBegin-RecvBuf)+1,PathBegin,RecvBuf+DataLen-PathBegin);	// 填写剩余内容
	
	char * HTTPTag = strstr(SendBuf+(UrlBegin-RecvBuf)+1," HTTP/1.1\r\n");
	if (HTTPTag) { HTTPTag[8] = '0'; }	// 强制使用http/1.0 防止Keep-Alive

	size_t TotalLen = UrlBegin+1+DataLen-PathBegin;

	//cout<<"转发到web的是："<<endl<<SendBuf<<endl;

	if( SendData(lpParameter->ServerSocket,SendBuf,TotalLen) <= 0){ return 0; }

	ExchangeData( lpParameter );
	return TRUE;
}


WSADATA wsaData;
SOCKET ProxyServer;
struct sockaddr_in Server;

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

	const char *filename = "host.txt";
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

	InitDNS(filename);

	if(InitSocket()==FALSE)
	{
		cout<<"InitSocket failed";
		return -1;
	}

	SOCKET AcceptSocket = INVALID_SOCKET;
	RECVPARAM *lpParameter;
	DWORD dwThreadID;
	printf("DNS File: %s\r\nListening Port: %d\r\n\r\n\r\n",filename,LISTENPORT);
	while(1)
	{
		AcceptSocket = accept(ProxyServer, NULL, NULL);
		lpParameter = new RECVPARAM;
		lpParameter->ClientSocket = AcceptSocket;
		DWORD tid;
		HANDLE hThread = CreateThread(NULL, 0, ProxyThread,lpParameter, 0, &tid);
		CloseHandle(hThread);
	}
	return 0;
}

BOOL InitSocket(void)
{
	if(WSAStartup(MAKEWORD(2,2),&wsaData)!=0)
	{
		cout<<"WSAStartup Failed: "<<WSAGetLastError()<<endl;
		return FALSE;
	}
	ProxyServer= socket(AF_INET, SOCK_STREAM, 0);
	if(INVALID_SOCKET==ProxyServer)
	{
		cout<<"Create Socket Failed: "<<WSAGetLastError()<<endl;
		return FALSE;
	}
	Server.sin_family = AF_INET;
	Server.sin_port = htons(LISTENPORT);
	Server.sin_addr.S_un.S_addr = INADDR_ANY;
	if(bind(ProxyServer, (LPSOCKADDR)&Server, sizeof(Server)) == SOCKET_ERROR)
	{
		printf("Bind Error: %d\n",WSAGetLastError());
		return FALSE;
	}
	if(listen(ProxyServer, SOMAXCONN) == SOCKET_ERROR)
	{
		printf("Listen Error: %d\n",WSAGetLastError());
		return FALSE;
	}
	return TRUE;
}

