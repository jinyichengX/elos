#include "el_mutex_lock.h"
#include "el_pthread.h"

/* 设置锁的所有者 */
#define SET_MUTEX_OWNER(P_MUT,P_PTCB)\
	   (P_MUT->Owner = P_PTCB)

/* 初始化信号量 */
void EL_Lite_Semaphore_Init(lite_sem_t * sem,\
					EL_UCHAR val)
{
    sem->Sem_value = val;
	sem->Max_value = val;
	INIT_LIST_HEAD(&sem->Waiters);
}

/* 初始化锁 */
void EL_Mutex_Lock_Init(mutex_lock_t* lock,\
					mutex_lock_attr_t lock_attr)
{
	if(NULL == lock) return;
	
	lock->Lock_nesting = 0;
	lock->Owner_orig_prio = 0xff;
	lock->Lock_attr = lock_attr;
	lock->Owner = (EL_PTCB_T *)0;
	EL_Lite_Semaphore_Init(&lock->Semaphore,1);
}

/* 获取信号量 */
EL_RESULT_T EL_Lite_Semaphore_Proberen(lite_sem_t * sem,\
					EL_UINT timeout_tick)
{
	Suspend_t *SuspendObj;
	
	OS_Enter_Critical_Check();
	EL_PTCB_T * Cur_ptcb = EL_GET_CUR_PTHREAD();
	while(sem->Sem_value == 0)
	{
		ASSERT(Cur_ptcb->pthread_state != EL_PTHREAD_SUSPEND);
		/* 将线程放入等待队列 */
		list_add_tail(&EL_GET_CUR_PTHREAD()->pthread_node,\
					&sem->Waiters);
		/* 挂起当前线程 */
		if (NULL == (SuspendObj = (Suspend_t *)malloc(SZ_Suspend_t))){
			OS_Exit_Critical_Check();
			return EL_RESULT_ERR;
		}
		SuspendObj->Pthread = Cur_ptcb;
		SuspendObj->PendingType = (void *)0;
		Cur_ptcb->block_holder = (void *)SuspendObj;
		EL_Pthread_PushToSuspendList(Cur_ptcb,\
					&SuspendObj->Suspend_Node);

		OS_Exit_Critical_Check();
		/* 执行一次线程切换 */
		PORT_PendSV_Suspend();
		/* 当线程恢复运行后要先进入临界区 */
		OS_Enter_Critical_Check();
	}
	sem->Sem_value --;
	OS_Exit_Critical_Check();

	return EL_RESULT_OK;
}

/* 获取锁 */
EL_RESULT_T EL_MutexLock_Take(mutex_lock_t* lock)
{
	/* 获取当前线程 */
	EL_PTCB_T *Cur_ptcb = EL_GET_CUR_PTHREAD();

	OS_Enter_Critical_Check();
	/* 如果是被销毁的锁则返回错误 */
	if(lock->Lock_attr & MUTEX_LOCK_INVALID){
		OS_Exit_Critical_Check();
		return EL_RESULT_ERR;
	}
	/* 如果锁的拥有者是当前线程,递归次数加一，防止死锁 */
	if(lock->Lock_attr & MUTEX_LOCK_NESTING){
		if(Cur_ptcb == lock->Owner){
			lock->Lock_nesting ++;
			OS_Exit_Critical_Check();
			return EL_RESULT_OK;
		}
	}
	/* 如果当前线程优先级大于锁拥有者线程的优先级，则使低优先级线程继承当前线程优先级 */
    if (lock->Owner && Cur_ptcb->pthread_prio > lock->Owner->pthread_prio) {
        /* 提升等待锁的线程的优先级至当前锁的持有者的优先级 */
		lock->Owner_orig_prio = lock->Owner->pthread_prio;
        EL_Pthread_Priority_Set(lock->Owner, Cur_ptcb->pthread_prio);
    }
	OS_Exit_Critical_Check();
	EL_Lite_Semaphore_Proberen(&lock->Semaphore,0);
	OS_Enter_Critical_Check();
	/* 获得了锁 */
	ASSERT(lock->Semaphore.Sem_value == 0);
	/* 设置锁的拥有者为当前线程 */
	SET_MUTEX_OWNER(lock,Cur_ptcb);

	lock->Owner_orig_prio = Cur_ptcb->pthread_prio;
	OS_Exit_Critical_Check();

	return EL_RESULT_OK;
}

/* 释放信号量 */
EL_RESULT_T EL_Lite_Semaphore_Verhogen(lite_sem_t * sem)
{
	LIST_HEAD * WaiterToRemove;

	OS_Enter_Critical_Check();
	/* 唤醒等待队列中的第一个线程 */
	if(!list_empty(&sem->Waiters)){
		ASSERT(((EL_PTCB_T *)sem->Waiters.next)\
		->pthread_state == EL_PTHREAD_SUSPEND);
		/* 释放第一个等待信号量的线程 */
		WaiterToRemove = sem->Waiters.next;
		list_del((LIST_HEAD *)sem->Waiters.next);
		EL_Pthread_Resume((EL_PTCB_T *)WaiterToRemove);
	}
	sem->Sem_value ++;
	ASSERT(sem->Sem_value <= sem->Max_value);
	OS_Exit_Critical_Check();

	return EL_RESULT_OK;
}

/* 释放锁 */
EL_RESULT_T EL_MutexLock_Release(mutex_lock_t* lock)
{
	/* 获取当前线程 */
	EL_PTCB_T *Cur_ptcb = EL_GET_CUR_PTHREAD();

	OS_Enter_Critical_Check();
	/* 如果是被销毁的锁则返回错误 */
	if(lock->Lock_attr & MUTEX_LOCK_INVALID){
		ASSERT(lock->Owner == (EL_PTCB_T *)0);
		OS_Exit_Critical_Check();
		return EL_RESULT_ERR;
	}
	/* 锁应当为当前线程所有，释放其他线程的锁会报错 */
	ASSERT(lock->Owner == Cur_ptcb);
	/* 如果递归次数大于1 */
	if(lock->Lock_attr & MUTEX_LOCK_NESTING){
		if((lock->Lock_nesting >= 1)&&\
			(lock->Owner == Cur_ptcb)){
			lock->Lock_nesting --;
			CPU_EXIT_CRITICAL_NOCHECK();
			return EL_RESULT_OK;
		}
	}
	ASSERT(lock->Lock_nesting == 0);
	/* 恢复当前线程原优先级 */
	if(lock->Owner_orig_prio != Cur_ptcb->pthread_prio){
		Cur_ptcb->pthread_prio = lock->Owner_orig_prio;
	}
	/* 重置锁的拥有者 */
	SET_MUTEX_OWNER(lock,(EL_PTCB_T *)0);
	OS_Exit_Critical_Check();
//	OS_Enter_Critical_Check();
//	ASSERT(lock->Semaphore.Sem_value == 1);
//	OS_Exit_Critical_Check();
	EL_Lite_Semaphore_Verhogen(&lock->Semaphore);

	return EL_RESULT_OK;
}

/* 销毁锁 */
void EL_Pthread_Mutex_Deinit(mutex_lock_t* lock)
{
	lock->Semaphore.Max_value = 0;
	lock->Semaphore.Sem_value = 0;
	lock->Lock_attr = MUTEX_LOCK_INVALID;
	lock->Owner = (EL_PTCB_T *)0;
	lock->Lock_nesting = 0;
	INIT_LIST_HEAD(&lock->Semaphore.Waiters);
}