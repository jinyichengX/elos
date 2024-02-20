#include "el_pthread.h"
#include <string.h>
#include "el_timer.h"
#include "el_debug.h"
EL_PORT_STACK_TYPE g_psp;/* 私有psp的值 */
EL_PORT_STACK_TYPE **g_pthread_sp;/* 线程私有psp在内存中的地址 */

static EL_UINT g_pthread_pid = 0;/* 用于线程ID分配 */

static EL_UCHAR EL_OS_Scheduling = 0;/* 调度标志? */
/* 内核列表 */
LIST_HEAD_CREAT(*KERNEL_LIST_HEAD[EL_PTHREAD_STATUS_COUNT]);/* 内核列表指针 */
LIST_HEAD_CREAT(PRIO_LISTS[EL_MAX_LEVEL_OF_PTHREAD_PRIO]);/* 线程就绪列表00 */
LIST_HEAD_CREAT(PthreadToSuspendListHead);/* 线程挂起列表03 */
LIST_HEAD_CREAT(PthreadToDeleteListHead);/* 线程删除列表 */
LIST_HEAD_CREAT(PendListHead);/* 线程阻塞延时列表02 */
/* 内核相关宏 */
#define SZ_TickPending_t sizeof(TickPending_t)
#define PTHREAD_STATUS_SET(PRIV_PSP,STATE) (PTCB_BASE(PRIV_PSP)->pthread_state = STATE)/* 设置线程状态 */
#define PTHREAD_STATE_SET(PPTCB,STATE) (PPTCB->pthread_state = STATE)/* 设置线程状态(由ptcb*) */
#define PTHREAD_STATE_GET(PPTCB) (PPTCB->pthread_state)/* 读取线程状态(由ptcb*) */
#define PTHREAD_STATUS_GET(PRIV_PSP) (PTCB_BASE(PRIV_PSP)->pthread_state)/* 获取线程状态 */

#define PTHREAD_NEXT_RELEASE_TICK_GET(TSCB) (((TickPending_t *)TSCB.next)->TickSuspend_Count)/* 获取计时tick */
#define PTHREAD_NEXT_RELEASE_OVERFLOW_TICK_GET(TSCB) (((TickPending_t *)TSCB.next)->TickSuspend_OverFlowCount)/* 获取溢出tick */
#define PTHREAD_NEXT_RELEASE_PRIO_GET(TSCB) ((((TickPending_t *)TSCB.next)->Pthread)->pthread_prio)/* 获取阻塞头的优先级 */
#define PTHREAD_NEXT_RELEASE_PTCB_NODE_GET(TSCB) ((((TickPending_t *)TSCB.next)->Pthread)->pthread_node)/* 获取阻塞头的线程node */
#define OS_SCHED_DONOTHING()

/* 内核变量 */
static EL_PTHREAD_PRIO_TYPE first_start_prio = EL_MAX_LEVEL_OF_PTHREAD_PRIO;/* 第一个调度的线程的优先级 */
EL_UINT g_TickSuspend_OverFlowCount = 0;/* 系统tick溢出单位 */
EL_UINT g_TickSuspend_Count = 0;/* 一单位内系统tick */
static EL_PTCB_T EL_Default_Pthread;/* 系统默认线程，不可删除 */
static void *wtfstt[EL_PTHREAD_STATUS_COUNT] = {};
#if EL_CALC_CPU_USAGE_RATE
EL_FLOAT CPU_UsageRate = CPU_MAX_USAGE_RATE;/* cpu使用率 */
#endif

extern EL_UINT g_critical_nesting; 
//CalcMemUsgRtLikely
/* 空闲线程，当其他线程都处于挂起或阻塞态时，会运行这个线程 */
void EL_DefultPthread(void)
{
	while(1)
	{
		/* 做一些内存清理工作 */
		if(!list_empty(&PthreadToDeleteListHead))
		{
			/* 堆内存的所有操作都不可重入 */
			OS_Enter_Critical_Check();
			free((void *)(((EL_PTCB_T *)(PthreadToDeleteListHead.next))->pthread_stack_top));
			list_del(PthreadToDeleteListHead.next);
			OS_Exit_Critical_Check();
		}
		/* 让出CPU */
		PORT_PendSV_Suspend();
	}
}
/* 初始化线程优先级列表 */
void EL_PrioListInitialise(void)
{
    for(EL_UINT prio_ind = 0;\
	prio_ind < EL_MAX_LEVEL_OF_PTHREAD_PRIO;prio_ind++)
		INIT_LIST_HEAD( &PRIO_LISTS[prio_ind] );
}

/* 系统初始化 */
void EL_OS_Initialise(void)
{
	/* 初始化线程就绪列表 */
	(void)EL_PrioListInitialise();
	/* 初始化阻塞延时列表 */
	INIT_LIST_HEAD( &PendListHead );
	/* 初始化线程删除列表 */
	INIT_LIST_HEAD( &PthreadToDeleteListHead );
	/* 初始化线程挂起列表 */
	INIT_LIST_HEAD( &PthreadToSuspendListHead );
	/* 列表索引初始化 */
	KERNEL_LIST_HEAD[EL_PTHREAD_READY] = PRIO_LISTS;
	KERNEL_LIST_HEAD[EL_PTHREAD_PENDING] = &PendListHead;
	KERNEL_LIST_HEAD[EL_PTHREAD_SUSPEND] = &PthreadToSuspendListHead;
	g_critical_nesting = 0;
	/* 创建空闲线程 */
	EL_Pthread_Create(&EL_Default_Pthread,"IDLE",EL_DefultPthread,\
	MIN_PTHREAD_STACK_SIZE+EL_CFG_DEFAULT_PTHREAD_STACK_SIZE,\
	EL_MAX_LEVEL_OF_PTHREAD_PRIO-1);
}

/* 创建线程，只支持在主线程创建 */
EL_RESULT_T EL_Pthread_Create(EL_PTCB_T *ptcb,const char * name,void *pthread_entry,\
	EL_UINT pthread_stackSz,EL_PTHREAD_PRIO_TYPE prio)
{
    /* 初始化线程名 */
    for(int i = 0;i<(EL_PTHREAD_NAME_MAX_LEN<strlen(name)?EL_PTHREAD_NAME_MAX_LEN:strlen(name));i++)
		ptcb->pthread_name[i] = name[i];
    /* 分配线程栈 */
	EL_STACK_TYPE *PSTACK,*PSTACK_TOP = NULL;
    PSTACK = (EL_STACK_TYPE *)malloc(pthread_stackSz+MIN_PTHREAD_STACK_SIZE);
	if(NULL == PSTACK)	return EL_RESULT_ERR;
    /* 将线程栈底向下作4字节对齐 */
	PSTACK_TOP = PSTACK = (EL_STACK_TYPE *)((EL_STACK_TYPE)PSTACK+pthread_stackSz);
    PSTACK = (EL_STACK_TYPE *)((EL_STACK_TYPE)PSTACK&(~((EL_STACK_TYPE)0X3)));

    /* 初始化线程栈 */
    ptcb->pthread_sp = (EL_PORT_STACK_TYPE *)PORT_Initialise_pthread_stack(PSTACK,pthread_entry);

    /* 线程控制块其他参数 */
    ptcb->pthread_prio = (prio>=(EL_MAX_LEVEL_OF_PTHREAD_PRIO-1))?\
							(EL_MAX_LEVEL_OF_PTHREAD_PRIO-1):prio;
	ptcb->pthread_state = EL_PTHREAD_READY;
	ptcb->pthread_stack_top = PSTACK_TOP;/* 栈顶 */
#if EL_CALC_PTHREAD_STACK_USAGE_RATE
	ptcb->pthread_stack_size = pthread_stackSz;/* 栈大小 */
	ptcb->pthread_stack_usage = (EL_FLOAT)sizeof(EL_STACK_TYPE)*16*100/pthread_stackSz;/* 栈使用率 */
#endif
	ptcb->pthread_id = g_pthread_pid++;
	ptcb->block_holder = NULL;
    /* 将线程添加至就绪表中 */
	list_add_tail(&ptcb->pthread_node,&PRIO_LISTS[ptcb->pthread_prio]);
	
	/* 更新第一个启动的线程优先级 */
	if(first_start_prio >= ptcb->pthread_prio) 
		first_start_prio = ptcb->pthread_prio;
    return EL_RESULT_OK;
}

/* 启动线程调度 */
void EL_OS_Start_Scheduler(void)
{
	/* 系统心跳和其他设置 */
	PORT_CPU_Initialise();
	/* 获取第一个线程的私有堆栈指针 */
	g_psp = (EL_PORT_STACK_TYPE)(((EL_PTCB_T *)(PRIO_LISTS[first_start_prio].next))->pthread_sp);
	g_pthread_sp = &(((EL_PTCB_T *)(PRIO_LISTS[first_start_prio].next))->pthread_sp);
	list_del(&(((EL_PTCB_T *)(PRIO_LISTS[first_start_prio].next))->pthread_node));
	PTHREAD_STATUS_SET(g_pthread_sp,EL_PTHREAD_RUNNING);
	/* 设置处理器模式，启动第一个线程 */
	PORT_Start_Scheduler();
}

#if EL_CALC_PTHREAD_STACK_USAGE_RATE
/* 计算线程栈使用率 */
void EL_Update_Pthread_Stack_Usage_Percentage(void)
{
	PTCB_BASE(g_pthread_sp)->pthread_stack_usage = \
		(EL_FLOAT)((EL_PORT_STACK_TYPE)(PTCB_BASE(g_pthread_sp)\
		->pthread_stack_top)-g_psp)*100\
		/(PTCB_BASE(g_pthread_sp)->pthread_stack_size);
}
#endif

/* 寻找下一个就绪线程 */
#define EL_SearchForNextReadyPthread() do {\
for(EL_PTHREAD_PRIO_TYPE pth_prio = 0;\
pth_prio < EL_MAX_LEVEL_OF_PTHREAD_PRIO;pth_prio++){\
if(!list_empty(&PRIO_LISTS[pth_prio])){\
	g_psp = (EL_PORT_STACK_TYPE)(((EL_PTCB_T *)(PRIO_LISTS[pth_prio].next))->pthread_sp);\
	g_pthread_sp = &(((EL_PTCB_T *)(PRIO_LISTS[pth_prio].next))->pthread_sp);\
	list_del(&(((EL_PTCB_T *)(PRIO_LISTS[pth_prio].next))->pthread_node));\
	PTHREAD_STATUS_SET(g_pthread_sp,EL_PTHREAD_RUNNING);\
	break;\
}\
}\
}while(0);

/* 线程切换，只在pendSV中进行 */
void EL_Pthread_SwicthContext(void)
{
	/* 执行线程切换 */
	if(EL_PTHREAD_RUNNING == PTHREAD_STATUS_GET(g_pthread_sp)){
		/* 将上文线程插入到对应的优先级列表中 */
		list_add_tail( &((PTCB_BASE(g_pthread_sp)->pthread_node)),\
					&PRIO_LISTS[(PTCB_BASE(g_pthread_sp))->pthread_prio] );
		PTHREAD_STATUS_SET(g_pthread_sp,EL_PTHREAD_READY);
	}else if((EL_PTHREAD_PENDING == PTHREAD_STATUS_GET(g_pthread_sp))||\
	(EL_PTHREAD_SUSPEND == PTHREAD_STATUS_GET(g_pthread_sp))){
		OS_SCHED_DONOTHING();/* 如果是被阻塞或挂起的线程，什么都不做 */
	}else {OS_SCHED_DONOTHING();}
    /* 找出优先级最高的线程，最高优先级线程轮转执行 */
	EL_SearchForNextReadyPthread();
}

/* 更新系统时基和阻塞释放 */
void EL_TickIncCount(void)
{
	/* 更新系统时基 */
	EL_UINT g_TickSuspend_Count_Temp = g_TickSuspend_Count;
	g_TickSuspend_Count ++;
	
	if(g_TickSuspend_Count < g_TickSuspend_Count_Temp)
		g_TickSuspend_OverFlowCount ++;
}

/* 线程阻塞释放 */
void EL_Pthread_Pend_Release(void)
{
	if(list_empty(&PendListHead)) return;
	/* 所有阻塞超时线程全部放入对应优先级的就绪列表 */
	while((((g_TickSuspend_OverFlowCount==PTHREAD_NEXT_RELEASE_OVERFLOW_TICK_GET(PendListHead))&&\
		(g_TickSuspend_Count>=PTHREAD_NEXT_RELEASE_TICK_GET(PendListHead)))\
		||(g_TickSuspend_OverFlowCount>PTHREAD_NEXT_RELEASE_OVERFLOW_TICK_GET(PendListHead)))\
		&&(!list_empty(&PendListHead)))
	{
		void *p_NodeToDel = (void *)PendListHead.next;
		/* 设置为就绪态 */
		((TickPending_t *)PendListHead.next)->Pthread->pthread_state = EL_PTHREAD_READY;
		/* 放入就绪列表 */
		list_add_tail(&PTHREAD_NEXT_RELEASE_PTCB_NODE_GET(PendListHead),\
					&PRIO_LISTS[PTHREAD_NEXT_RELEASE_PRIO_GET(PendListHead)]);
		/* 删除并释放阻塞头 */
		list_del(PendListHead.next);
		free(p_NodeToDel);
	}
}

/* 线程非阻塞休眠 */
void EL_Pthread_Sleep(EL_UINT TickToDelay)
{
	/* 升序插入tick */
	if(!TickToDelay) return;
	if(EL_PTHREAD_RUNNING != PTHREAD_STATUS_GET(g_pthread_sp)) return;/* 不在运行态就退出，一般不会在这退出 */
	TickPending_t *PendingObj;
	
	OS_Enter_Critical_Check();/* 避免多个线程使用删除列表 */
	EL_UINT temp_abs_tick = g_TickSuspend_Count;
	EL_UINT temp_overflow_tick = g_TickSuspend_OverFlowCount;
	if(g_TickSuspend_Count + TickToDelay < g_TickSuspend_Count)
		temp_overflow_tick ++;
	temp_abs_tick = temp_abs_tick + TickToDelay;
	
	if (NULL == (PendingObj = (TickPending_t *)malloc(SZ_TickPending_t))){
		OS_Exit_Critical_Check();
		return;/* 没有足够的管理空间，线程不可延时，退出，继续运行当前线程 */
	}
	PendingObj->TickSuspend_Count = temp_abs_tick;
	PendingObj->TickSuspend_OverFlowCount = temp_overflow_tick;
	PendingObj->Pthread = (EL_PTCB_T *)(PTCB_BASE(g_pthread_sp));
	PendingObj->Pthread->block_holder = (void *)(&PendingObj->TickPending_Node);
	
	/* 切换状态并放入阻塞列表 */
	list_add_inorder(&PendingObj->TickPending_Node,KERNEL_LIST_HEAD[EL_PTHREAD_PENDING]);/* 升序放入阻塞延时列表 */
	OS_Exit_Critical_Check();
	PTHREAD_STATE_SET(PendingObj->Pthread,EL_PTHREAD_PENDING);
	/* 执行一次线程切换 */
	PORT_PendSV_Suspend();
}

/* 将线程放入挂起列表 */
void EL_Pthread_PushToSuspendList(EL_PTCB_T * ptcb,LIST_HEAD * SuspendList)
{
	list_add_tail(SuspendList,KERNEL_LIST_HEAD[EL_PTHREAD_SUSPEND]);
	PTHREAD_STATE_SET(ptcb,EL_PTHREAD_SUSPEND);
}

/* 挂起线程，允许用户线程挂起自己或其他同级线程 */
EL_RESULT_T EL_Pthread_Suspend(EL_PTCB_T *PthreadToSuspend)
{
	EL_PTCB_T *ptcb;
	Suspend_t *SuspendObj;
	ptcb = (PthreadToSuspend != NULL)?PthreadToSuspend:EL_GET_CUR_PTHREAD();
	/* 挂起线程相关的同步互斥机制 */
	if(PthreadToSuspend->PendFlags & EL_PTHREAD_SEMAP_WAIT){/* 待添加 */;}
	else if(PthreadToSuspend->PendFlags & EL_PTHREAD_MUTEX_WAIT){/* 待添加 */;}
	else if(PthreadToSuspend->PendFlags & EL_PTHREAD_SPEEDPIPE_WAIT){/* 待添加 */;}
	/* 将其放入线程挂起列表 */
	OS_Enter_Critical_Check();/* 避免多个线程使用挂起列表 */
	/* 不允许挂起空闲线程 */
	if(ptcb == &EL_Default_Pthread) return EL_RESULT_ERR;
	if(ptcb->pthread_state == EL_PTHREAD_SUSPEND) return EL_RESULT_ERR;/* 已经挂起，能否改为嵌套机制 */
	/* 如果挂起的是其他线程 */
	if(PTHREAD_STATE_GET(ptcb) == EL_PTHREAD_DEAD){
		OS_Exit_Critical_Check();/* 如果是死亡线程直接退出 */
		return EL_RESULT_OK;
	}
	if (NULL == (SuspendObj = (Suspend_t *)malloc(SZ_Suspend_t))){
		OS_Exit_Critical_Check();
		return EL_RESULT_ERR;/* 没有足够的管理空间，不可挂起，退出，继续运行当前线程 */
	}
	SuspendObj->Pthread = ptcb;
	SuspendObj->Suspend_nesting ++;
	SuspendObj->PendingType = (void *)0;
	/* 如果是本线程，则所有阻塞类事件已经结束，直接放入挂起列表 */
	if(ptcb == EL_GET_CUR_PTHREAD()){
		ptcb->block_holder = (void *)SuspendObj;
		EL_Pthread_PushToSuspendList(ptcb,&SuspendObj->Suspend_Node);
		OS_Exit_Critical_Check();
		PORT_PendSV_Suspend();
		
		return EL_RESULT_OK;
	}
	if(ptcb->pthread_state == EL_PTHREAD_READY){
		list_del(&ptcb->pthread_node);/* 从就绪列表删除 */
		ptcb->block_holder = (void *)&SuspendObj->Suspend_Node;
	}else if(ptcb->pthread_state == EL_PTHREAD_PENDING){
		/* 挂起策略1，阻塞延时无效 */
		list_del((LIST_HEAD *)ptcb->block_holder);/* 从阻塞列表删除 */
		SuspendObj->PendingType = (void *)ptcb->block_holder;
		ptcb->block_holder = (void *)&SuspendObj->Suspend_Node;
		/* 挂起策略2，阻塞延时有效 */
	}
	/* 加入挂起列表 */
	list_add_tail(&SuspendObj->Suspend_Node,(KERNEL_LIST_HEAD[EL_PTHREAD_SUSPEND]));/* 加入阻塞列表 */
	PTHREAD_STATE_SET(ptcb,EL_PTHREAD_SUSPEND);/* 设为挂起态 */
	OS_Exit_Critical_Check();
	return EL_RESULT_OK;
}

/* 解除线程挂起状态,线程自己不能解除自己的挂起态 */
void EL_Pthread_Resume(EL_PTCB_T *PthreadToResume)
{
	EL_PTCB_T *ptcb = PthreadToResume;
	if(ptcb == EL_GET_CUR_PTHREAD()) return;
	OS_Enter_Critical_Check();
	/* 如果不是已经挂起的线程，退出 */
	if(ptcb->pthread_state != EL_PTHREAD_SUSPEND) {
		OS_Exit_Critical_Check();return;
	}
	/* 查看挂起前的状态并恢复，并从挂起列表删除 */
	void * p_rec_node = (container_of(ptcb->block_holder,Suspend_t,Suspend_Node))->PendingType;
	if(p_rec_node != NULL){/* 阻塞态 */
		list_add_inorder((LIST_HEAD *)p_rec_node,KERNEL_LIST_HEAD[EL_PTHREAD_PENDING]);
	}else{/* 就绪态 */
		list_add_tail(&ptcb->pthread_node,\
		KERNEL_LIST_HEAD[EL_PTHREAD_READY]+ptcb->pthread_prio);
	}
	list_del((LIST_HEAD *)ptcb->block_holder);
	free(ptcb->block_holder);
	if(p_rec_node != NULL) 	ptcb->block_holder = p_rec_node;
	OS_Exit_Critical_Check();
	return;
}

///* 调度上锁 */
//void EL_Pthread_SuspendAll()
//{
//	
//}

///* 调度解锁 */
//void EL_Pthread_ResumeAll()
//{
//	
//}

/* 销毁线程,不允许销毁其他同级线程！没有返回则说明删除成功，如果有返回值必然为NULL表示删除失败 */
EL_PTCB_T* EL_Pthread_DelSelf(EL_PTCB_T *PthreadToDel)
{
	int loop;
	/* 不需要进入临界区，运行态的任务的唯一性已经保证了原子性 */
	EL_PTCB_T *ptcb = EL_GET_CUR_PTHREAD();
	if(PthreadToDel == NULL);
	else if(PthreadToDel != ptcb) return NULL;
	
	/* 销毁线程相关的同步互斥机制 */
	if(PthreadToDel->PendFlags & EL_PTHREAD_SEMAP_WAIT){/* 待添加 */;}
	else if(PthreadToDel->PendFlags & EL_PTHREAD_MUTEX_WAIT){/* 待添加 */;}
	else if(PthreadToDel->PendFlags & EL_PTHREAD_SPEEDPIPE_WAIT){/* 待添加 */;}
	/* 将其放入线程删除列表 */
	OS_Enter_Critical_Check();/* 避免多个线程使用删除列表 */
	list_add_tail(&ptcb->pthread_node,&PthreadToDeleteListHead);
	OS_Exit_Critical_Check();
	
	/* 标记为死亡线程，调度器不会将其放入就绪列表 */
	ptcb->pthread_state = EL_PTHREAD_DEAD;
	
	/* 让出CPU */
	PORT_PendSV_Suspend();
	/* 运行到这里就是失败 */
	return NULL;
}

/* 修改线程优先级 */
void EL_Pthread_Priority_Set(EL_PTCB_T *PthreadToModify,EL_PTHREAD_PRIO_TYPE new_prio)
{
	EL_PTCB_T *ptcb = EL_GET_CUR_PTHREAD();
	if(ptcb->pthread_prio == new_prio) return;
	
	OS_Enter_Critical_Check();/* 避免多个线程使用删除列表 */
	if((PthreadToModify == ptcb)||(PthreadToModify == NULL)){/* 如果是当前线程 */
		ASSERT(ptcb->pthread_state == EL_PTHREAD_RUNNING);
		ptcb->pthread_prio = new_prio;/* 直接改变优先级 */
		OS_Exit_Critical_Check(); return;
	}
	/* 如果是其他线程 */
	if(PthreadToModify->pthread_state == EL_PTHREAD_READY){/* 如果已处于就绪表中 */
		/* 从原就绪表删除放入新就绪表 */
		list_del(&PthreadToModify->pthread_node);/* 从原就绪列表删除 */
		list_add_tail(&PthreadToModify->pthread_node,\
		KERNEL_LIST_HEAD[EL_PTHREAD_READY]+PthreadToModify->pthread_prio);/* 加入到新的就绪列表 */
	}else{ /* 其他情况下什么也不用做 */; }
	PthreadToModify->pthread_prio = new_prio;
	OS_Exit_Critical_Check();
	return;
}

