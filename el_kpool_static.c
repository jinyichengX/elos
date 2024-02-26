#include "el_kpool_static.h"
#include "el_debug.h"
#include "port.h"
#include <stdio.h>
#include <string.h>
/* 内核对象内存池 */
EL_UCHAR EL_Pthread_Pendst_Pool[EL_Pthread_Pendst_Pool_Size];
EL_UCHAR EL_Pthread_Suspendst_Pool[EL_Pthread_Suspendst_Pool_Size];

/* 对齐方式 */
#if KPOOL_BYTE_ALIGNMENT == 16
#define KPOOL_BYTE_ALIGNMENT_MASK    ( 0x000f )
#elif KPOOL_BYTE_ALIGNMENT == 8
#define KPOOL_BYTE_ALIGNMENT_MASK    ( 0x0007 )
#elif KPOOL_BYTE_ALIGNMENT == 4
#define KPOOL_BYTE_ALIGNMENT_MASK    ( 0x0003 )
#endif

/* 检查内存对齐宏 */
#define IS_KPOOL_ALIGNED(BASE) (((EL_UINT)BASE & KPOOL_BYTE_ALIGNMENT_MASK) == 0)
/* 将内核对象块大小向上作4字节对齐 */
#define EL_KPOOL_BLKSZ_ALIGNED(BLK_SZ) ( (sizeof(LIST_HEAD) + BLK_SZ)+(KPOOL_BYTE_ALIGNMENT - 1) )\
                                        & ~( (EL_UINT)KPOOL_BYTE_ALIGNMENT_MASK )
#define EL_POOL_BLOCK_NODE_ADDR(POBJ)  ((EL_UCHAR *)POBJ-sizeof(LIST_HEAD))

/* 静态池初始化 */
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

    OS_Exit_Critical_Check();
    return EL_RESULT_OK;
}

/* 从指定静态池分配内存块 */
void * EL_stKpoolBlockAlloc(void *PoolSurf)
{
    if(PoolSurf == NULL) return NULL;
    EL_KPOOL_INFO_T * pKpoolInfo= (EL_KPOOL_INFO_T *)PoolSurf;
    EL_UCHAR *pBlkToAlloc;

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

/* 从指定静态池回收内存块 */
void EL_stKpoolBlockFree(void *PoolSurf,void *pBlkToFree)
{
    EL_KPOOL_INFO_T * pKpoolInfo= (EL_KPOOL_INFO_T *)PoolSurf;
    if((PoolSurf == NULL) || (pBlkToFree == NULL)) return;
    LIST_HEAD * pblk;

    OS_Enter_Critical_Check();

    pblk = (LIST_HEAD *)EL_POOL_BLOCK_NODE_ADDR(pBlkToFree);
    list_add_tail(pblk,&pKpoolInfo->ObjBlockList);
    pKpoolInfo->UsingBlockCnt --;

    OS_Exit_Critical_Check();
}

/* 清零静态池中的某一内存块 */
void EL_stKpoolClear(void * PoolSurf, void * pBlk)
{
    EL_KPOOL_INFO_T *pKpoolInfo = (EL_KPOOL_INFO_T *)PoolSurf;

    if (PoolSurf == NULL || pBlk == NULL) return;

	OS_Enter_Critical_Check();
    memset(pBlk, 0, pKpoolInfo->PerBlockSize - sizeof(LIST_HEAD));
	OS_Exit_Critical_Check();
}