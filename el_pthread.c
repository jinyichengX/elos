#include "el_pthread.h"
#include <string.h>
#include "el_timer.h"

EL_PORT_STACK_TYPE g_psp;/* 私有psp的值 */
EL_PORT_STACK_TYPE **g_pthread_sp;/* 线程私有psp在内存中的地址 */

TickSuspend_LIST_HEAD SuspendDelayListHead;/* 线程阻塞延时列表 */
PRIO_LIST_HEAD PRIO_LISTS[EL_MAX_LEVEL_OF_PTHREAD_PRIO];/* 线程就绪列表 */

#define SZ_TickSuspend_t sizeof(TickSuspend_t)
#define FPOS(type,field) ( (EL_PORT_STACK_TYPE)&((type *)0)->field )
#define STATICSP_POS FPOS(EL_PTCB_T,pthread_sp) /* 线程私有sp在线程控制块中的偏移 */
#define PTCB_BASE(PRIV_PSP) ( (EL_PTCB_T *)((EL_PORT_STACK_TYPE)PRIV_PSP-STATICSP_POS) )/* 线程控制块基地址（指针） */
#define PTHREAD_STATUS_SET(PRIV_PSP,STATE) (PTCB_BASE(PRIV_PSP)->pthread_state = STATE)/* 设置线程状态 */
#define PTHREAD_STATUS_GET(PRIV_PSP) (PTCB_BASE(PRIV_PSP)->pthread_state)/* 获取线程状态 */
#define PTHREAD_NEXT_RELEASE_TICK_GET(TSCB) (((TickSuspend_t *)TSCB.next)->TickSuspend_Count)/* 获取计时tick */
#define PTHREAD_NEXT_RELEASE_OVERFLOW_TICK_GET(TSCB) (((TickSuspend_t *)TSCB.next)->TickSuspend_OverFlowCount)/* 获取溢出tick */
#define PTHREAD_NEXT_RELEASE_PRIO_GET(TSCB) ((((TickSuspend_t *)TSCB.next)->Pthread)->pthread_prio)/* 获取阻塞头的优先级 */
#define PTHREAD_NEXT_RELEASE_PTCB_NODE_GET(TSCB) ((((TickSuspend_t *)TSCB.next)->Pthread)->pthread_node)/* 获取阻塞头的线程node */

EL_PTHREAD_PRIO_TYPE first_start_prio = 0;/* 第一个调度的线程的优先级 */
EL_UINT g_TickSuspend_OverFlowCount = 0;/* 系统tick溢出单位 */
EL_UINT g_TickSuspend_Count = 0;/* 一单位内系统tick */

EL_PTCB_T EL_Default_Pthread;/* 系统默认线程，不可删除 */
#if EL_CALC_CPU_USAGE_RATE
EL_FLOAT CPU_UsageRate = CPU_MAX_USAGE_RATE;/* cpu使用率 */
#endif
//CalcMemUsgRtLikely
/* 默认线程，当其他线程都处于阻塞状态时，会运行这个线程 */
void EL_DefultPthread(void)
{
	while(1){
		volatile int a = 0;
		/* 计算内存使用率 */
		
		/* 计算CPU使用率 */
		
		/* 休眠 */
	}
}
/* 初始化线程优先级列表 */
void EL_PrioListInitialise(void)
{
    for(EL_UINT prio_ind = 0;\
        prio_ind < EL_MAX_LEVEL_OF_PTHREAD_PRIO;prio_ind++)
        INIT_LIST_HEAD( &PRIO_LISTS[prio_ind] );
}

/* 创建线程 */
int EL_Pthread_Create(EL_PTCB_T *ptcb,const char * name,void *pthread_entry,\
	EL_UINT pthread_stackSz,EL_PTHREAD_PRIO_TYPE pthread_prio)
{
	if(pthread_stackSz < MIN_PTHREAD_STACK_SIZE) return -1;

    /* 初始化线程名 */
    for(int i = 0;i<(EL_PTHREAD_NAME_MAX_LEN<strlen(name)?EL_PTHREAD_NAME_MAX_LEN:strlen(name));i++)
		ptcb->pthread_name[i] = *(name+i);

    /* 分配线程栈 */
	EL_STACK_TYPE *PSTACK,*PSTACK_TOP = NULL;

    PSTACK = (EL_STACK_TYPE *)malloc(pthread_stackSz);

	if(NULL == PSTACK)	return -1;

    /* 将线程栈底向下作4字节对齐 */
	PSTACK = (EL_STACK_TYPE *)((EL_STACK_TYPE)PSTACK+pthread_stackSz);
    PSTACK_TOP = PSTACK = (EL_STACK_TYPE *)((EL_STACK_TYPE)PSTACK&(~((EL_STACK_TYPE)0X3)));

    /* 初始化线程栈 */
    ptcb->pthread_sp = (EL_PORT_STACK_TYPE *)PORT_Initialise_pthread_stack(PSTACK,pthread_entry);

    /* 线程控制块其他参数 */
    ptcb->pthread_prio = pthread_prio;
	ptcb->pthread_state = EL_PTHREAD_READY;
#if EL_CALC_PTHREAD_STACK_USAGE_RATE
	ptcb->pthread_stack_size = pthread_stackSz;/* 栈大小 */
	ptcb->pthread_stack_usage = (EL_FLOAT)sizeof(EL_STACK_TYPE)*16*100/pthread_stackSz;/* 栈使用率 */
	ptcb->pthread_stack_top = PSTACK_TOP;/* 栈顶 */
#endif

    /* 将线程添加至就绪表中 */
	list_add_tail(&ptcb->pthread_node,&PRIO_LISTS[pthread_prio]);
	
	/* 更新第一个启动的线程优先级 */
	if(first_start_prio >= pthread_prio) first_start_prio = pthread_prio;

    return 0;
}

void EL_OS_Initialise(void)
{
	/* 初始化线程就绪列表 */
	EL_PrioListInitialise();

	/* 初始化阻塞延时列表 */
	INIT_LIST_HEAD( &SuspendDelayListHead );

	/* 创建一个空闲任务 */
	EL_Pthread_Create(&EL_Default_Pthread,"DEFUALT",EL_DefultPthread,\
					MIN_PTHREAD_STACK_SIZE,EL_MAX_LEVEL_OF_PTHREAD_PRIO-1);
}

/* 启动线程调度 */
void EL_OS_Start(void)
{
	/* 系统心跳和其他设置 */
	PORT_CPU_Initialise();

	/* 获取第一个线程的私有堆栈指针 */
	g_psp = (EL_PORT_STACK_TYPE)(((EL_PTCB_T *)(PRIO_LISTS[first_start_prio].next))->pthread_sp);/* 设置全局psp为第一个线程的私有psp */
	g_pthread_sp = &(((EL_PTCB_T *)(PRIO_LISTS[first_start_prio].next))->pthread_sp);/* 获取全局线程私有psp的地址 */

	list_del(&(((EL_PTCB_T *)(PRIO_LISTS[first_start_prio].next))->pthread_node));		/* 从线程就绪列表删除 */

	PTHREAD_STATUS_SET(g_pthread_sp,EL_PTHREAD_RUNNING);/* 设置为运行态 */

	/* 设置处理器模式，启动第一个线程 */
	PORT_Start_Scheduler();
}

#if EL_CALC_PTHREAD_STACK_USAGE_RATE
/* 计算线程栈使用率 */
void EL_Update_Pthread_Stack_Usage_PercentRate(void)
{
	PTCB_BASE(g_pthread_sp)->pthread_stack_usage = \
		(EL_FLOAT)((EL_PORT_STACK_TYPE)(PTCB_BASE(g_pthread_sp)\
		->pthread_stack_top)-g_psp)*100\
		/(PTCB_BASE(g_pthread_sp)->pthread_stack_size);
}
#endif

/* 寻找下一个就绪线程 */
#define EL_SearchForNextReadyPthread()

/* 线程切换，只在pendSV中进行 */
void EL_Pthread_SwicthContext(void)
{
	/* 执行线程切换 */
	if(EL_PTHREAD_RUNNING == PTHREAD_STATUS_GET(g_pthread_sp)){
		/* 将上文线程插入到对应的优先级列表中 */
		list_add_tail( &((PTCB_BASE(g_pthread_sp)->pthread_node)), \
					&PRIO_LISTS[(PTCB_BASE(g_pthread_sp))->pthread_prio] );
		PTHREAD_STATUS_SET(g_pthread_sp,EL_PTHREAD_READY);
	}else if(EL_PTHREAD_PENDING == PTHREAD_STATUS_GET(g_pthread_sp)){
		/* 如果是被阻塞的线程，什么都不做 */
	}

    /* 找出优先级最高的线程，最高优先级线程轮转执行 */
	for(EL_PTHREAD_PRIO_TYPE pth_prio = 0;pth_prio< EL_MAX_LEVEL_OF_PTHREAD_PRIO;pth_prio++){
		if(!list_empty(&PRIO_LISTS[pth_prio])){

			//list_move_tail(PRIO_LISTS[pth_prio].next,&PRIO_LISTS[pth_prio]);
			g_psp  = (EL_PORT_STACK_TYPE)(((EL_PTCB_T *)(PRIO_LISTS[pth_prio].next))->pthread_sp);
			g_pthread_sp = &(((EL_PTCB_T *)(PRIO_LISTS[pth_prio].next))->pthread_sp);/* 获取全局线程私有psp的地址 */
			list_del(&(((EL_PTCB_T *)(PRIO_LISTS[pth_prio].next))->pthread_node));		/* 从线程就绪列表删除 */

			/* 设置为运行态 */
			PTHREAD_STATUS_SET(g_pthread_sp,EL_PTHREAD_RUNNING);

			break;
		}
	}
}


/* 更新系统时基和阻塞释放 */
void EL_TickIncCount(void)
{
	/* 更新系统时基 */
	EL_UINT g_TickSuspend_Count_Temp = g_TickSuspend_Count;
	g_TickSuspend_Count ++;
	
	if(g_TickSuspend_Count < g_TickSuspend_Count_Temp)
		g_TickSuspend_OverFlowCount ++;
	
	/* 所有阻塞超时线程全部放入对应优先级的就绪列表 */
	while((((g_TickSuspend_OverFlowCount==PTHREAD_NEXT_RELEASE_OVERFLOW_TICK_GET(SuspendDelayListHead))&&\
		(g_TickSuspend_Count>=PTHREAD_NEXT_RELEASE_TICK_GET(SuspendDelayListHead)))\
		||(g_TickSuspend_OverFlowCount>PTHREAD_NEXT_RELEASE_OVERFLOW_TICK_GET(SuspendDelayListHead)))\
		&&(!list_empty(&SuspendDelayListHead)))
	{
		void *p_NodeToDel = (void *)SuspendDelayListHead.next;
		/* 设置为就绪态 */
		((TickSuspend_t *)SuspendDelayListHead.next)->Pthread->pthread_state = EL_PTHREAD_READY;
		/* 放入就绪列表 */
		list_add_tail(&PTHREAD_NEXT_RELEASE_PTCB_NODE_GET(SuspendDelayListHead),\
					&PRIO_LISTS[PTHREAD_NEXT_RELEASE_PRIO_GET(SuspendDelayListHead)]);
		/* 删除并释放阻塞头 */
		list_del(SuspendDelayListHead.next);
		free(p_NodeToDel);
	}
}

/* 非阻塞延时 */
void EL_SuspendDelay(EL_UINT TickToDelay)
{
	/* 升序插入tick */
	if(!TickToDelay) return;

	if(EL_PTHREAD_RUNNING != PTHREAD_STATUS_GET(g_pthread_sp)) return;/* 不在运行态就退出，一般不会在这退出 */

	EL_UINT temp_abs_tick = g_TickSuspend_Count;
	EL_UINT temp_overflow_tick = g_TickSuspend_OverFlowCount;

	if(g_TickSuspend_Count + TickToDelay < g_TickSuspend_Count)
		temp_overflow_tick += 1;
	temp_abs_tick = temp_abs_tick + TickToDelay;

	/* 按升序插入阻塞延时列表 */
	TickSuspend_t *SuspendObj;
	if (NULL == (SuspendObj = (TickSuspend_t *)malloc(SZ_TickSuspend_t)))
		return;/* 没有足够的管理空间，线程不可延时，退出，继续运行此线程 */

	SuspendObj->TickSuspend_Count = temp_abs_tick;
	SuspendObj->TickSuspend_OverFlowCount = temp_overflow_tick;
	SuspendObj->Pthread = (EL_PTCB_T *)(PTCB_BASE(g_pthread_sp));
	
	/* 切换状态并放入阻塞列表 */
	PTHREAD_STATUS_SET(g_pthread_sp,EL_PTHREAD_PENDING);
	list_add_inorder(&SuspendObj->TickSuspend_Node,&SuspendDelayListHead);/* 升序放入阻塞延时列表 */

	/* 执行一次线程切换 */
	PORT_PendSV_Suspend();
}

/* 线程调度上锁 */
void EL_Sched_Lock()
{
	
}

/* 销毁线程 */
void EL_Pthread_Del(EL_PTCB_T *PtcbToDel)
{
	if(NULL == PtcbToDel) 
	{
		/* 如果在就绪列表，就从就绪列表删除 */
		/* 否则从 */
	}
	/* 销毁线程相关的IPC和同步互斥机制等 */
	
	/* 执行一次线程切换 */
	PORT_PendSV_Suspend();
}