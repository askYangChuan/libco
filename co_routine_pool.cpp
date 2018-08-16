#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "co_routine_pool.h"


#define DEF_ROUTINE_POOL_MAX 500
#define DEF_ROUTINE_POOL_ADD_SIZE 20

#define DEF_ROUTINE_POOL_RECOVERY_TIME		(600*1000)
#define DEF_ROUTINE_POOL_RECOVERY_MIN_TIME	(10*1000)
#define DEF_ROUTINE_POOL_RECOVERY_MAX	(2000)
#define DEF_ROUTINE_POOL_RECOVERY_ALLOC_LIMIT	(30000)




struct stCoRoutine_pool_t
{
	pthread_mutex_t lock;

	struct stlc_list_head coroutine_free;
	struct stlc_list_head coroutine_end;
	unsigned int alloc;
	unsigned int co_end;
	unsigned int co_free;
};


static stCoRoutine_pool_t *stCoRoutine_pool = NULL;


static void stCoRoutine_add(stCoRoutine_pool_t *pool, int size) 
{
	stCoRoutine_t *co;
	for(int i = 0; i < size; i++) {
		co = (stCoRoutine_t*)calloc(1, sizeof(stCoRoutine_t));		
		stlc_list_add_tail(&co->link, &pool->coroutine_free);
	}
	if(size > 0) {
		pool->alloc += size;
		pool->co_free += size;
	}
}

stCoRoutine_pool_t *stCoRoutine_pool_alloc(int size)
{
	stCoRoutine_pool_t *pool = (stCoRoutine_pool_t*)calloc(1, sizeof(stCoRoutine_pool_t));
	
	pthread_mutex_init(&pool->lock, NULL);
	STLC_INIT_LIST_HEAD(&pool->coroutine_free);
	STLC_INIT_LIST_HEAD(&pool->coroutine_end);


	if(size > DEF_ROUTINE_POOL_MAX) {
		size = DEF_ROUTINE_POOL_MAX;
	}

	stCoRoutine_add(pool, size);

	return pool;
}

void stCoRoutine_pool_free()
{
	stCoRoutine_pool_t *pool = stCoRoutine_pool;
	if(pool == NULL) {
		return;
	}

	stCoRoutine_t *pos, *n;

	pthread_mutex_lock(&pool->lock);

	stlc_list_for_each_entry_safe(pos, n, &pool->coroutine_free, link) {
		stlc_list_del(&pos->link);
		co_free(pos);
	}

	pthread_mutex_unlock(&pool->lock);
	pthread_mutex_destroy(&pool->lock);
	free(pool);
}

stCoRoutine_t *stCoRoutine_alloc()
{
	stCoRoutine_t *lp = NULL;
	stCoRoutine_pool_t *pool = stCoRoutine_pool;

	if(!pool) {
		return (stCoRoutine_t*)malloc( sizeof(stCoRoutine_t) );
	}

	pthread_mutex_lock(&pool->lock);

	if(stlc_list_empty(&pool->coroutine_free)) {
		stCoRoutine_add(pool, DEF_ROUTINE_POOL_ADD_SIZE);
	}
	
	lp = stlc_list_first_entry(&pool->coroutine_free, stCoRoutine_t, link);
	stlc_list_del(&lp->link);
	pool->co_free--;
	
	pthread_mutex_unlock(&pool->lock);
	return lp;
}

void stCoRoutine_release(stCoRoutine_t *co)
{
	if(!stCoRoutine_pool) {
		//return co_free(co);
		return ;
	}

	
	//co_release(co);
	pthread_mutex_lock(&stCoRoutine_pool->lock);

	stlc_list_add_tail(&co->link, &stCoRoutine_pool->coroutine_end);
	stCoRoutine_pool->co_end++;
	pthread_mutex_unlock(&stCoRoutine_pool->lock);
}

void stCoRoutine_pool_status()
{
	unsigned int alloc;
	unsigned int co_end;
	unsigned int co_free;

	if(!stCoRoutine_pool) {
		printf("not using pool\n");
		return;
	}


	pthread_mutex_lock(&stCoRoutine_pool->lock);
	alloc = stCoRoutine_pool->alloc;
	co_end = stCoRoutine_pool->co_end;
	co_free = stCoRoutine_pool->co_free;	
	pthread_mutex_unlock(&stCoRoutine_pool->lock);

	printf("alloc: %u, free_count: %u, end_count: %u\n", alloc, co_free, co_end);
}

static void *stCoRoutine_recovery(void *args)
{
	co_enable_hook_sys();
	stCoRoutine_t *pos, *n;
	int count = 0;
	int charge = 0;

	int timeout = DEF_ROUTINE_POOL_RECOVERY_TIME;

	for(;;) {
		poll(NULL, 0, timeout);

		count = 0;	
		charge = 0;
		pthread_mutex_lock(&stCoRoutine_pool->lock);
		stlc_list_for_each_entry_safe(pos, n, &stCoRoutine_pool->coroutine_end, link) {
			if(count++ > DEF_ROUTINE_POOL_RECOVERY_MAX) {
				break;
			}
			stlc_list_del(&pos->link);
			stCoRoutine_pool->co_end--;
			if(stCoRoutine_pool->co_free >= DEF_ROUTINE_POOL_RECOVERY_ALLOC_LIMIT) {
				stCoRoutine_pool->alloc--;
				co_free(pos);
				charge = 1;
			}else {
				co_release(pos);
				stCoRoutine_pool->co_free++;
				stlc_list_add_tail(&pos->link, &stCoRoutine_pool->coroutine_free);
			}
		}
		pthread_mutex_unlock(&stCoRoutine_pool->lock);
		if(charge)
			timeout = DEF_ROUTINE_POOL_RECOVERY_TIME;
	}


	return NULL;
}

int stCoRoutine_pool_init()
{
	stCoRoutine_t *co;
	

	if(!stCoRoutine_pool)
		stCoRoutine_pool = stCoRoutine_pool_alloc(DEF_ROUTINE_POOL_MAX);

	co_create(&co, NULL, stCoRoutine_recovery, NULL);
	co_resume(co);
	
	return 0;
}
