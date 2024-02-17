#ifndef EL_PTHREAD_H
#define EL_PTHREAD_H

#include "elightOS_config.h"
#include "el_list.h"
#include "el_type.h"
#include "el_phymm.h"
#include "port.h"

#ifndef CPU_MAX_USAGE_RATE
#define CPU_MAX_USAGE_RATE 100
#endif

#ifndef MIN_PTHREAD_STACK_SIZE
#define MIN_PTHREAD_STACK_SIZE 64
#endif

#if ((EL_MAX_LEVEL_OF_PTHREAD_PRIO <= 0)||\
	(EL_MAX_LEVEL_OF_PTHREAD_PRIO > 256))
    #error "EL_MAX_LEVEL_OF_PTHREAD_PRIO macro invalid"
#endif

#if EL_PTHREAD_NAME_MAX_LEN <= 0
    #error "pthread name is too short"
#endif

/* 线程状态类型 */
typedef enum {
	EL_PTHREAD_READY = 0, /* 线程就绪 */
	EL_PTHREAD_PENDING,	/* 线程阻塞 */
	EL_PTHREAD_SUSPEND,	/* 线程挂起 */
	EL_PTHREAD_RUNNING, /* 线程运行 */
	EL_PTHREAD_SLEEPING,/* 线程延时 */
	EL_PTHREAD_COMPOUND_BLOCK,/* 线程复合阻塞 */
	EL_PTHREAD_STATUS_COUNT,/* 线程状态计数 */
	EL_PTHREAD_DEAD		/* 死亡线程，不能恢复 */
}EL_PTHREAD_STATUS_T;

/* 阻塞类型 */
typedef enum{
	EL_PTHREAD_SLEEP_PEND,
	EL_PTHREAD_MUTEX_PEND
}PendType_t;

/* 线程控制块 */
typedef struct EL_PTHREAD_CONTROL_BLOCK
{
	LIST_HEAD_CREAT(pthread_node);
    EL_CHAR pthread_name[EL_PTHREAD_NAME_MAX_LEN];/* 线程名 */
    EL_PTHREAD_PRIO_TYPE pthread_prio;/* 线程优先级 */
    EL_PORT_STACK_TYPE *pthread_sp;/* 动态线程栈 */
    EL_PTHREAD_STATUS_T pthread_state;/* 线程状态 */
	EL_PORT_STACK_TYPE *pthread_stack_top;/* 线程栈栈顶 */
#if EL_CALC_PTHREAD_STACK_USAGE_RATE
	EL_STACK_TYPE pthread_stack_size;/* 线程栈大小 */
	EL_FLOAT pthread_stack_usage;/* 线程栈使用率 */
#endif
	EL_UINT pthread_id;/* 线程id */
	void * block_holder;/* 被持有对象的节点 */
}EL_PTCB_T;

typedef struct EL_OS_AbsolutePendingTickCount
{
	LIST_HEAD_CREAT(TickPending_Node);
	EL_UINT TickSuspend_OverFlowCount;/* 系统tick溢出单位 */
	EL_UINT TickSuspend_Count;/* 一单位内系统tick */
	EL_PTCB_T * Pthread;/* 被延时阻塞的线程 */
	PendType_t PendType;/* 阻塞类型 */
}TickPending_t;

typedef struct EL_OS_SuspendStruct
{
	LIST_HEAD_CREAT(Suspend_Node);
	EL_PTCB_T * Pthread;/* 被挂起的线程 */
	EL_UCHAR Suspend_nesting;/* 挂起嵌套计数 */
	void * PendingType;/* 是否正处于阻塞状态被挂起 */
}Suspend_t;

#define FPOS(type,field) ( (EL_PORT_STACK_TYPE)&((type *)0)->field )
#define STATICSP_POS FPOS(EL_PTCB_T,pthread_sp) /* 线程私有sp在线程控制块中的偏移 */
#define PTCB_BASE(PRIV_PSP) ( (EL_PTCB_T *)((EL_PORT_STACK_TYPE)PRIV_PSP-STATICSP_POS) )/* 线程控制块基地址（指针） */
#define EL_GET_CUR_PTHREAD() PTCB_BASE(g_pthread_sp)/* 获取当前线程控制块（指针） */
extern EL_RESULT_T EL_Pthread_Create(EL_PTCB_T *ptcb,const char * name,void *pthread_entry,EL_UINT pthread_stackSz,EL_PTHREAD_PRIO_TYPE pthread_prio);
extern void EL_OS_Start_Scheduler(void);
extern void EL_PrioListInitialise(void);
extern void EL_OS_Initialise(void);
extern void EL_Pthread_Sleep(EL_UINT TickToDelay);
extern EL_RESULT_T EL_Pthread_Suspend(EL_PTCB_T *PthreadToSuspend);
extern void EL_Pthread_Resume(EL_PTCB_T *PthreadToResume);
extern void EL_TickIncCount(void);
extern void EL_Pthread_Pend_Release(void);
extern EL_PTCB_T* EL_Pthread_DelSelf(EL_PTCB_T *PthreadToDel);
extern EL_PORT_STACK_TYPE g_psp;
extern EL_PORT_STACK_TYPE **g_pthread_sp;/* 线程私有psp */
extern EL_UINT g_TickSuspend_OverFlowCount;/* 系统tick溢出单位 */
extern EL_UINT g_TickSuspend_Count;/* 一单位内系统tick */
#if EL_CLAC_CPU_USAGE_RATE
extern EL_FLOAT CPU_UsageRate;
#endif
#endif

