#ifndef EL_MUTEX_LOCK_H
#define EL_MUTEX_LOCK_H

#include "el_pthread.h"
#include "el_debug.h"
#include "el_type.h"

/* 锁属性 */
#define MUTEX_LOCK_NORMAL 0X00	/* 普通锁 */
#define MUTEX_LOCK_NESTING 0X01	/* 递归锁 */
#define MUTEX_LOCK_INVALID 0X02	/* 失能锁 */
#define MUTEX_LOCK_OTHER1 0X04	/* 其他锁 */
#define MUTEX_LOCK_DEFAULT MUTEX_LOCK_NORMAL

/* 锁属性 */
typedef enum MUTEX_LOCK_ATTRIBUTE{
	NORMAL = MUTEX_LOCK_NORMAL,
	NESTING = MUTEX_LOCK_NESTING,
	INVALID = MUTEX_LOCK_INVALID,
	OTHER1 = MUTEX_LOCK_OTHER1,
	DEFAULT = MUTEX_LOCK_DEFAULT
}mutex_lock_attr_t;

/* 信号量 */
typedef struct EL_OS_SEMAPHORE_STRUCT{
    EL_UCHAR Sem_value;	/* 计数量 */
	EL_UCHAR Max_value;	/* 最大计数 */
	LIST_HEAD Waiters;	/* 等待列表 */
}lite_sem_t;

/* 锁 */
typedef struct EL_OS_MUTEX_LOCK_STRUCT {
    EL_PTCB_T * Owner;		/* 持有者 */
    EL_UCHAR Lock_attr;		/* 锁属性 */
	lite_sem_t Semaphore;	/* 父对象 */
    EL_UCHAR Lock_nesting;	/* 嵌套层数 */
	EL_UCHAR Owner_orig_prio;     /* 原优先级 */
}mutex_lock_t;

#define MUTEX_LOCK_CREAT(lock) mutex_lock_t (lock)/* 创建锁对象 */

extern void EL_Mutex_Lock_Init(mutex_lock_t* lock,\
					mutex_lock_attr_t lock_attr);
extern EL_RESULT_T EL_MutexLock_Take(mutex_lock_t* lock);
extern EL_RESULT_T EL_MutexLock_Release(mutex_lock_t* lock);
#endif