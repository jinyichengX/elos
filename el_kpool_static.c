#include "el_kpool_static.h"
#include "el_pthread.h"
#include "el_debug.h"
#include "port.h"
#include <stdio.h>
#include <string.h>

#if KPOOL_BYTE_ALIGNMENT%4 != 0
#error "KPOOL_BYTE_ALIGNMENT macro must be divisible by 4"
#endif

/* 分配内核对象的静态内存池 */
EL_UCHAR EL_Pthread_Pendst_Pool[EL_Pthread_Pendst_Pool_Size];
EL_UCHAR EL_Pthread_Suspendst_Pool[EL_Pthread_Suspendst_Pool_Size];

/* 内存池对齐方式 */
#if KPOOL_BYTE_ALIGNMENT == 16
#define KPOOL_BYTE_ALIGNMENT_MASK    ( 0x000f )
#elif KPOOL_BYTE_ALIGNMENT == 8
#define KPOOL_BYTE_ALIGNMENT_MASK    ( 0x0007 )
#elif KPOOL_BYTE_ALIGNMENT == 4
#define KPOOL_BYTE_ALIGNMENT_MASK    ( 0x0003 )
#endif

/* 检查静态内存池合法性 */
#define IS_POOL_VALID(P) P->TotalBlockNum
/* 检查内存对齐宏 */
#define IS_KPOOL_ALIGNED(BASE) (((EL_UINT)BASE & KPOOL_BYTE_ALIGNMENT_MASK) == 0)
/* 将内核对象块大小向上作4字节对齐 */
#define EL_KPOOL_BLKSZ_ALIGNED(BLK_SZ) ( (sizeof(LIST_HEAD) + BLK_SZ)+(KPOOL_BYTE_ALIGNMENT - 1) )\
                                        & ~( (EL_UINT)KPOOL_BYTE_ALIGNMENT_MASK )
#define EL_POOL_BLOCK_NODE_ADDR(POBJ)  ((EL_UCHAR *)POBJ-sizeof(LIST_HEAD))
	
/**********************************************************************
 * 函数名称： EL_stKpoolInitialise
 * 功能描述： 静态池初始化
 * 输入参数： PoolSurf ：静态内存池池面指针
			 PoolSize ：静态内存池池深
			 PerBlkSz ：池中每个静态内存块大小
 * 输出参数： 无
 * 返 回 值： EL_RESULT_OK/EL_RESULT_ERR
 * 修改日期        版本号     修改人	      修改内容
 * -----------------------------------------------
 * 2024/02/25	    V1.0	  jinyicheng	      创建
 ***********************************************************************/
EL_RESULT_T EL_stKpoolInitialise(void * PoolSurf,EL_UINT PoolSize,EL_UINT PerBlkSz)
{
    EL_KPOOL_INFO_T * pKpoolInfo= (EL_KPOOL_INFO_T *)PoolSurf;
    int i;EL_UCHAR * blk_list;
    /* 传参检查 */
    if((PoolSurf == NULL) || (PoolSize <= sizeof(EL_KPOOL_INFO_T))\
	|| (PerBlkSz == 0))
        return EL_RESULT_ERR;
    /* 检查对齐，编译器应该会自动对齐 */
    if (!IS_KPOOL_ALIGNED(PoolSurf)) return EL_RESULT_ERR;
	/* 这里需要进入临界区是因为为了用户和内核空间统一管理 */
    /* 避免用户在多线程环境中使用静态内存分配方式时导致的竞态问题 */
    OS_Enter_Critical_Check();
    /* 静态内存池控制头初始化 */
    pKpoolInfo->PerBlockSize = EL_KPOOL_BLKSZ_ALIGNED(PerBlkSz);
    pKpoolInfo->TotalBlockNum = (PoolSize - sizeof(EL_KPOOL_INFO_T))/\
								pKpoolInfo->PerBlockSize;
    pKpoolInfo->UsingBlockCnt = (EL_UINT)0;
    
    if(pKpoolInfo->TotalBlockNum == (EL_UINT)0){
        OS_Exit_Critical_Check();
        return EL_RESULT_ERR;
    }
    /* 初始化静态内存池控制头的链表头 */
    INIT_LIST_HEAD(&pKpoolInfo->ObjBlockList);
    /* 将所有可用的内存块节点通过链表连接起来 */
    blk_list = (EL_UCHAR *)PoolSurf+sizeof(EL_KPOOL_INFO_T);
	list_add_tail((LIST_HEAD *)blk_list,&pKpoolInfo->ObjBlockList);
    for(i = 0; i < pKpoolInfo->TotalBlockNum-1; ++i){
        blk_list += pKpoolInfo->PerBlockSize;
        list_add_tail((LIST_HEAD *)blk_list,&pKpoolInfo->ObjBlockList);
    }
    /* 初始化等待列表 */
    INIT_LIST_HEAD(&pKpoolInfo->WaitersToTakePool);
    OS_Exit_Critical_Check();
	
    return EL_RESULT_OK;
}
/**********************************************************************
 * 函数名称： EL_stKpoolBlockAlloc
 * 功能描述： 从指定静态池分配内存块
 * 输入参数： PoolSurf ：静态内存池
 * 输出参数： 无
 * 返 回 值： 可用静态内存块地址
 * 修改日期        版本号     修改人	      修改内容
 * -----------------------------------------------
 * 2024/02/25	    V1.0	  jinyicheng	      创建
 ***********************************************************************/
void * EL_stKpoolBlockAlloc(void *PoolSurf,EL_UINT TicksToWait)
{
    if(PoolSurf == NULL) return NULL;
    EL_KPOOL_INFO_T * pKpoolInfo= (EL_KPOOL_INFO_T *)PoolSurf;
    EL_UCHAR *pBlkToAlloc;
	ASSERT(IS_POOL_VALID(pKpoolInfo));
	
    OS_Enter_Critical_Check();
    if(pKpoolInfo->TotalBlockNum == pKpoolInfo->UsingBlockCnt){
        ASSERT(list_empty(&pKpoolInfo->ObjBlockList));
        OS_Exit_Critical_Check();
        return NULL;
    }
    /* 释放第一个节点 */
	pBlkToAlloc = (EL_UCHAR *)(pKpoolInfo->ObjBlockList.next + 1); 
    list_del(pKpoolInfo->ObjBlockList.next);
    pKpoolInfo->UsingBlockCnt ++;
    OS_Exit_Critical_Check();
	
    return (void *)pBlkToAlloc;
}
/**********************************************************************
 * 函数名称： EL_stKpoolBlockAllocWait
 * 功能描述： 从指定静态池超时等待分配内存块
 * 输入参数： PoolSurf ：静态内存池
             TicksToWait ：超时等待时长
 * 输出参数： 无
 * 返 回 值： 可用静态内存块地址
 * 修改日期        版本号     修改人	      修改内容
 * -----------------------------------------------
 * 2024/03/02	    V1.0	  jinyicheng	      创建
 ***********************************************************************/
void * EL_stKpoolBlockAllocWait(void *PoolSurf,EL_UINT TicksToWait)
{
    if(PoolSurf == NULL) return NULL;
    EL_KPOOL_INFO_T * pKpoolInfo= (EL_KPOOL_INFO_T *)PoolSurf;
    EL_PTCB_T *Cur_ptcb = EL_GET_CUR_PTHREAD();
    EL_UCHAR *pBlkToAlloc;
	ASSERT(IS_POOL_VALID(pKpoolInfo));

    OS_Enter_Critical_Check();
    /* 等待空闲内存池，这里只能自旋等待，因为多个内存池与高速队列不同，是由多线程共享的 */
    while(pKpoolInfo->TotalBlockNum == pKpoolInfo->UsingBlockCnt){
        if(TicksToWait == (EL_UINT)0){
            ASSERT(list_empty(&pKpoolInfo->ObjBlockList));
            OS_Exit_Critical_Check();
            return NULL;
        }
        else if(TicksToWait != 0xffffffff){
            OS_Exit_Critical_Check();
            EL_Pthread_Sleep(TicksToWait);/* 线程休眠 */
            OS_Enter_Critical_Check();
            TicksToWait = (EL_UINT)0;
            continue;
        }
        /* 将线程放入等待列表，设置挂起态 */
        list_add_tail(&Cur_ptcb->pthread_node,&pKpoolInfo->WaitersToTakePool);
        PTHREAD_STATE_SET(Cur_ptcb,EL_PTHREAD_SUSPEND);

        OS_Exit_Critical_Check();
        PORT_PendSV_Suspend();
        OS_Enter_Critical_Check();
    }
    /* 有可用静态内存块，释放第一个节点 */
	pBlkToAlloc = (EL_UCHAR *)(pKpoolInfo->ObjBlockList.next + 1); 
    list_del(pKpoolInfo->ObjBlockList.next);
    pKpoolInfo->UsingBlockCnt ++;
    OS_Exit_Critical_Check();

    return (void *)pBlkToAlloc;
}
/**********************************************************************
 * 函数名称： EL_stKpoolBlockFree
 * 功能描述： 从指定静态池回收内存块
 * 输入参数： PoolSurf ：静态内存池
			 pBlkToFree ：需要释放的静态内存块首地址
 * 输出参数： 无
 * 返 回 值： 无
 * 修改日期        版本号     修改人	      修改内容
 * -----------------------------------------------
 * 2024/02/25	    V1.0	  jinyicheng	      创建
 ***********************************************************************/
void EL_stKpoolBlockFree(void *PoolSurf,void *pBlkToFree)
{
    EL_KPOOL_INFO_T * pKpoolInfo= (EL_KPOOL_INFO_T *)PoolSurf;
    EL_PTCB_T *Cur_ptcb = EL_GET_CUR_PTHREAD();
    if((PoolSurf == NULL) || (pBlkToFree == NULL)) return;
    LIST_HEAD * pblk;
	ASSERT(IS_POOL_VALID(pKpoolInfo));
	
    OS_Enter_Critical_Check();
    pblk = (LIST_HEAD *)EL_POOL_BLOCK_NODE_ADDR(pBlkToFree);
    list_add_tail(pblk,&pKpoolInfo->ObjBlockList);
    pKpoolInfo->UsingBlockCnt --;
    /* 唤醒所有请求内存池的线程 */
    while(!list_empty(&pKpoolInfo->WaitersToTakePool)){
        list_del(pKpoolInfo->WaitersToTakePool.next);
        /* 添加至就绪列表 */
        list_add_tail(&Cur_ptcb->pthread_node,\
            KERNEL_LIST_HEAD[EL_PTHREAD_READY]+Cur_ptcb->pthread_prio);
        PTHREAD_STATE_SET(Cur_ptcb,EL_PTHREAD_READY);
    }
    OS_Exit_Critical_Check();
}
/**********************************************************************
 * 函数名称： EL_stKpoolClear
 * 功能描述： 清零静态池中的某一内存块
 * 输入参数： PoolSurf ：静态内存池
			 pBlk ：需要清零内存的静态内存块首地址
 * 输出参数： 无
 * 返 回 值： 无
 * 修改日期        版本号     修改人	      修改内容
 * -----------------------------------------------
 * 2024/02/25	    V1.0	  jinyicheng	      创建
 ***********************************************************************/
void EL_stKpoolClear(void * PoolSurf, void * pBlk)
{
    EL_KPOOL_INFO_T *pKpoolInfo = (EL_KPOOL_INFO_T *)PoolSurf;

    if (PoolSurf == NULL || pBlk == NULL) return;

	OS_Enter_Critical_Check();
    memset(pBlk, 0, pKpoolInfo->PerBlockSize - sizeof(LIST_HEAD));
	OS_Exit_Critical_Check();
}