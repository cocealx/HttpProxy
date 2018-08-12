#include<stdlib.h>
#include<stdio.h>
#include<unistd.h>
#include<string.h>
#include<pthread.h>
#include<fcntl.h>
#include<sys/socket.h>
#include<sys/types.h>
#include<netinet/in.h>
#include<sys/sendfile.h>
#include<signal.h>
#include<sys/epoll.h>
#include <map>
#include <string>
#include <fstream>
typedef unsigned int ULONG; 
#define SOCKET_ERROR
int InitDNS(const char* fileName);
ULONG QueryIPByHostName(const char * hostName);
