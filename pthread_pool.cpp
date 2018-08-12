#include"pthread_pool.h"
static void*workpthread(void*arg);
static void* adjustpthread(void*arg);
pthread_pool_t *pthreadpool_create()
{

	pthread_pool_t *pool=(pthread_pool_t *)malloc(sizeof(pthread_pool_t));
	do{
		if(pool==NULL)
		{
			perror("malloc");
			break;
		}
		pool->queue=(task_t*)malloc(sizeof(task_t)*MAX_QUEUE);
		if(pool->queue==NULL)
			break;
		memset(pool->queue,0,sizeof(task_t)*MAX_QUEUE);
		pool->tid=(pthread_t*)malloc(sizeof(pthread_t)*MAX_PTHREAD);
		if(pool->tid==NULL)
			break;
		memset(pool->tid,0,sizeof(pthread_t)*MAX_PTHREAD);
		(*(int*)&pool->pthread_max)=MAX_PTHREAD;
		(*(int*)&pool->pthread_min)=MIN_PTHREAD;
		(*(int*)&pool->creat_count)=CREAT_COUNT;
		(*(int*)&pool->destory_count)=DESTORY_COUNT;
		pool->alive_count=MIN_PTHREAD;
		pool->work_count=0;
		pool->exit_count=0;
		(*(int*)&pool->queue_capacity)=MAX_QUEUE;
		pool->queue_size=0;
		pool->head=0;
		pool->tail=0;
		pool->shutdown=FALSE;
		if(pthread_mutex_init(&pool->lock,NULL)!=0||
 				pthread_mutex_init(&pool->work_lock,NULL)!=0||
				pthread_cond_init(&pool->queue_not_full,NULL)!=0||
				pthread_cond_init(&pool->queue_not_empty,NULL)!=0)
		{
			perror("pthread_init");
			break;
		}
		int i;
		for( i=0;i<MIN_PTHREAD;i++)
		{
			pthread_create(&pool->tid[i],NULL,workpthread,pool);
			pthread_detach(pool->tid[i]);
		}
		pthread_create(&pool->adjusttid,NULL,adjustpthread,pool);
		pthread_detach(pool->adjusttid);
		return pool;
	//相当于goto语句	
	}while(0);
	pthreadpool_destory(pool);
	return NULL;
}
//工作线程的工作流程
void*  workpthread(void*arg)
{
	pthread_pool_t *pool=(pthread_pool_t*)arg;
	for(;;)
	{
		pthread_mutex_lock(&pool->lock);
		while(pool->queue_size==0&&!pool->shutdown)
		{
			//printf("thread 0x%x  is waiting\n",(unsigned int)pthread_self());
			pthread_cond_wait(&pool->queue_not_empty,&pool->lock);
			if(pool->shutdown)
			{
			 	//printf("thread 0x%x  is exit\n",(unsigned int)pthread_self());
				pthread_mutex_unlock(&pool->lock);
				pthread_exit(NULL);
			}		
		////如果有要销毁的线程，且当前存活的线程数大于最小的线程的存活数，让线程自己退出
			if(pool->exit_count>0)
			{
				 if(pool->alive_count>pool->pthread_min)
				{
			 		//printf("thread 0x%x  is exit\n",(unsigned int)pthread_self());
					--pool->alive_count;
					//必须释放掉自己的锁
					pthread_mutex_unlock(&pool->lock);
					pthread_exit(NULL);
				}
			}
		}

		task_t  ta=pool->queue[pool->head];
		pool->head=(pool->head+1)%pool->queue_capacity;
		--pool->queue_size;
		pthread_mutex_unlock(&pool->lock);


		/////////忙碌线程加1
		pthread_mutex_lock(&pool->work_lock);
		++pool->work_count;
		pthread_mutex_unlock(&pool->work_lock);
		////////处理任务
		//printf("thread 0x%x  is working\n",(unsigned int)pthread_self());
		(*ta.function)(ta.arg);
		////////通知添加任务的线程，队列已经不为满了
		pthread_cond_broadcast(&pool->queue_not_full);
		/////////忙碌线程减1
		pthread_mutex_lock(&pool->work_lock);
		--pool->work_count;
		pthread_mutex_unlock(&pool->work_lock);
	}
	pthread_exit(NULL);
	return NULL;
}
//管理线程的流程
void*  adjustpthread(void*arg)
{
	pthread_pool_t *pool=(pthread_pool_t*)arg;
	for(;;)
	{
		pthread_mutex_lock(&pool->lock);
		if(pool->shutdown)
		{
			pthread_mutex_unlock(&pool->lock);
			break;
		}
		int alive_count=pool->alive_count;
		int work_count=pool->alive_count;
		int queue_size=pool->queue_size;
		pthread_mutex_unlock(&pool->lock);
		
		if(queue_size>alive_count&&alive_count<pool->pthread_max)
		{
			int i=0;
			int j=0;
			pthread_mutex_lock(&pool->lock);
			for(i=0;pool->alive_count<pool->pthread_max&&
					j<pool->creat_count&&
					i<pool->pthread_max;i++)
			{
				if(pool->tid[i]==0||pthread_kill(pool->tid[i],0))
				{
					pthread_create(&pool->tid[i],NULL,workpthread,(void*)pool);
					pthread_detach(pool->tid[i]);
					++pool->alive_count;
					++j;
				}
			}
			pthread_mutex_unlock(&pool->lock);
		}
		if(alive_count>pool->pthread_min&&3*pool->work_count<pool->alive_count)
		{
			pthread_mutex_lock(&pool->lock);
			pool->exit_count=pool->destory_count;
			pthread_mutex_unlock(&pool->lock);
			int i=0;
			for(i=0;i<pool->destory_count;i++)
			{
				pthread_cond_signal(&pool->queue_not_empty);
			}
		}
	}
}
//往线程池添加任务
int pthreadpool_addtask(pthread_pool_t *pool,task_t	ta)
{
	pthread_mutex_lock(&pool->lock);
	while(pool->queue_size==pool->queue_capacity)
	{
		//printf("addtask thread 0x%x  is waiting\n",(unsigned int)pthread_self());
		pthread_cond_wait(&pool->queue_not_full,&pool->lock);	
	
	}
	if(pool->shutdown)
	{
		pthread_mutex_unlock(&pool->lock);
	//	pthread_exit(NULL);
	    return -1;
	}	
	//printf("addtaskthread 0x%x  is addtask\n",(unsigned int)pthread_self());
	(pool->queue[pool->tail]).arg=ta.arg;
	(pool->queue[pool->tail]).function=ta.function;
	pool->tail=(pool->tail+1)%pool->queue_capacity;
	++pool->queue_size;
	pthread_mutex_unlock(&pool->lock);
	pthread_cond_signal(&pool->queue_not_empty);


}
int pthreadpool_destory(pthread_pool_t *pool)
{
	if(pool==NULL)
	return -1;
	///删除管理线程
	pool->shutdown=TRUE;
	int i=0;
	for(i=0;i<pool->alive_count;++i)
		pthread_cond_broadcast(&pool->queue_not_empty);
	free(pool->queue);
	free(pool->tid);
	if(pool->tid)
	{
		pthread_mutex_lock(&pool->lock);
		pthread_mutex_destroy(&pool->lock);
		pthread_mutex_lock(&pool->work_lock);
		pthread_mutex_destroy(&pool->work_lock);
		pthread_cond_destroy(&pool->queue_not_full);
		pthread_cond_destroy(&pool->queue_not_empty);
	}
	free(pool);
	return 0;
}
