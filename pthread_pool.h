#ifndef _PTHREAD_POOL_H_
#define _PTHREAD_POOL_H_
#include<stdio.h>
#include<signal.h>
#include<pthread.h>
#include<unistd.h>
#include<stdlib.h>
#include<string.h>
#define MAX_QUEUE 100
#define MAX_PTHREAD 100
#define MIN_PTHREAD 3
#define CREAT_COUNT 4
#define DESTORY_COUNT 4
#define FALSE 0
#define TRUE 1
typedef void*(*hand_t)(void*);
typedef struct {
	hand_t function;
	void*arg;
}task_t;
typedef struct  pthread_pool
{

		//用于锁整个结构的
		pthread_mutex_t lock;
		//用于判断队列是否为空，或者是否为满
		pthread_cond_t queue_not_full;
		pthread_cond_t queue_not_empty;
		//用于保存任务的队列
		task_t* queue;
		//对头和队尾，队列里面任务的个数，队列的容量
		int tail;
		int head;
		int queue_size;
		const int queue_capacity;
		//用于保存所有活着线程的tid
		pthread_t *tid;
		//管理线程的tid
		pthread_t adjusttid;
		//线程存活的个数，和工作线程的个数,即将要销毁的线程的个数
		int alive_count;
		int work_count;
		int exit_count;
		//控制工作线程个数变量的锁
		pthread_mutex_t work_lock;
		//线程池最多可以有多少个线程存活
		const int pthread_max;
		//线程池最低线程的存活数
		const int pthread_min;
		//管理线程在特定情况每次创建新线程的单位	
		const int creat_count;
		//管理线程在特定情况每次销毁新线程的单位	
		const int destory_count;
		//该线程池是否被关闭
		int shutdown;
} pthread_pool_t;
pthread_pool_t *pthreadpool_create();
int pthreadpool_addtask(pthread_pool_t *pool,task_t	ta);
int pthreadpool_destory(pthread_pool_t *pool);
#endif
