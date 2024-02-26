#ifndef EL_KPOOL_STATIC_H
#define EL_KPOOL_STATIC_H

#include "el_type.h"
#include "el_klist.h"

/* �����ں������ڴ�ش�С */
#define EL_Pthread_Pendst_Pool_Size 512
#define EL_Pthread_Suspendst_Pool_Size 256

/* ��N�ֽڶ��� */
#define KPOOL_BYTE_ALIGNMENT 4

/* ��̬�ڴ�ؿ���ͷ */
typedef struct EL_KERNEL_POOL_STATIC_STRUCT
{
    LIST_HEAD ObjBlockList; /* �б�����ͷ */
    EL_UINT PerBlockSize;   /* ���ڴ���зֿ��С */
    EL_UINT TotalBlockNum;   /* ���ڴ�����ֿܷ���Ŀ */
    EL_UINT UsingBlockCnt;   /* ���ڴ������ʹ�÷ֿ���� */
}EL_KPOOL_INFO_T;

extern EL_RESULT_T EL_stKpoolInitialise(void * PoolSurf,EL_UINT PoolSize,EL_UINT PerBlkSz);
extern void * EL_stKpoolBlockAlloc(void *PoolSurf);
extern void EL_stKpoolBlockFree(void *PoolSurf,void *pBlkToFree);
extern void EL_stKpoolClear(void * PoolSurf, void * pBlk);

extern EL_UCHAR EL_Pthread_Pendst_Pool[EL_Pthread_Pendst_Pool_Size];
extern EL_UCHAR EL_Pthread_Suspendst_Pool[EL_Pthread_Suspendst_Pool_Size];
#endif