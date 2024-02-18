#ifndef EL_MUTEX_LOCK_H
#define EL_MUTEX_LOCK_H

#include "el_pthread.h"
#include "el_debug.h"
#include "el_type.h"

/* 锁属性 */
#define MUTEX_LOCK_NORMAL 0X00/* 普通锁 */
#define MUTEX_LOCK_NESTING 0X01/* 递归锁 */
#define MUTEX_LOCK_INVALID 0X02/* 失能锁 */
#define MUTEX_LOCK_OTHER1 0X04/* 其他锁 */
#define MUTEX_LOCK_DEFAULT MUTEX_LOCK_NORMAL

typedef enum {MUTEX_UNLOCKED,MUTEX_LOCKED}MUTEX_LOCK_STATUS_T;

/* 信号量 */
typedef struct EL_OS_SEMAPHORE_STRUCT{
    EL_UCHAR Sem_value;/* 信号量计数 */
    LIST_HEAD Waiters;/* 等待列表 */
	EL_UCHAR Max_value;/* 最大计数 */
}lite_sem_t;

/* 锁 */
typedef struct EL_OS_MUTEX_LOCK_STRUCT {
    EL_PTCB_T * Owner;/* 锁的持有者线程 */
    lite_sem_t Semaphore;/* 父对象 */
    EL_UCHAR Lock_attr;/* 锁属性 */
    EL_UCHAR Lock_nesting;/* 嵌套层数 */
}mutex_lock_t;
#define MUTEX_LOCK_CREAT(lock) mutex_lock_t (lock)/* 创建锁对象 */

extern void EL_Lite_Semaphore_Init(lite_sem_t * sem,EL_UCHAR val);
extern void EL_Mutex_Lock_Init(mutex_lock_t* lock,\
					EL_UCHAR lock_attr);
extern EL_RESULT_T EL_Lite_Semaphore_Proberen(lite_sem_t * sem,EL_UINT timeout_tick);
extern EL_RESULT_T EL_MutexLock_Take(mutex_lock_t* lock);

#endif