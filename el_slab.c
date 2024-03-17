#include "el_slab.h"
#include "port.h"
#include <string.h>

/* 这里每个slab zone采用固定大小，可以自动调节 */
static EL_UINT g_SlabPoolSize[ELOS_SLAB_CHUNK_MAX_LEVEL] = {
    [ 0 ... ELOS_SLAB_CHUNK_MAX_LEVEL-1 ] = ELOS_PER_SLAB_ZONE_DATA_SIZE;
};
/* slab控制块管理的obj大小 */
static EL_UINT g_SlabObjSize[ELOS_SLAB_CHUNK_MAX_LEVEL] = {
};
/* 着色节点转移表 */
el_SlabCrRmp_t el_SlabCrRmpTab[4] = {
    {SLAB_CR_GREEN,SLAB_CR_YELLOW},{SLAB_CR_YELLOW,SLAB_CR_GREEN},
    {SLAB_CR_YELLOW,SLAB_CR_RED},{SLAB_CR_YELLOW,SLAB_CR_YELLOW},
    {SLAB_CR_RED,SLAB_CR_YELLOW},
};

/* 获取着色器长度 */
EL_UINT ELOS_SlabColorCntList(void *Slab_PoolSurf,LIST_HEAD* pList)
{

}

void ELOS_SlabDisprBitmapInitialise(el_SlabDispr_t * pDispr)
{
    el_SlabDispr_t * pSlabDispr = pDispr;
    if( pDispr == NULL ) return;
    pSlabDispr->bitmap[0] = 0;
}

void ELOS_SlabSDisprBitmapSetBit(  )
{

}
void ELOS_SlabSDisprBitmapClearBit(  )
{

}

void ELOS_SlabSDisprBitmapFindBitAndSet(  )
{

}
void ELOS_SlabSDisprBitmapFindBitAndClear(  )
{

}
/* 转移至深色节点，返回+1 */
EL_CHAR ELOS_SlabColorConvDeepIdxTake(el_SlabDispr_t * pSlabDispr)
{
    return 0;
}
/* 浅色节点，返回-1 */
EL_CHAR ELOS_SlabColorConvSallowIdxTake(el_SlabDispr_t * pSlabDispr)
{
    return 0; 
}
void SlabZoneResize(EL_UINT index,EL_UINT size)
{
    g_SlabPoolSize[index] = (index<=ELOS_SLAB_CHUNK_MAX_LEVEL-1)?\
                                size:ELOS_PER_SLAB_ZONE_DATA_SIZE;
}
void SlabZoneResizeAll(EL_UINT size)
{
    for(int idx=0; idx<ELOS_SLAB_CHUNK_MAX_LEVEL-1; idx++)
        SlabZoneResize(size);
}
/* slab控制组初始化 */
void ELOS_SlabPoolClassInitialise(void *Slab_PoolSurf, EL_UINT Slab_PoolSize)
{
    el_SlabPoolHead_t *p_SlabPool = (el_SlabPoolHead_t *)0;
    if(Slab_PoolSurf == NULL) return;

    if(Slab_PoolSize < ELOS_SLAB_CHUNK_MAX_LEVEL*sizeof(el_SlabPoolHead_t))
        return;

    p_SlabPool = (el_SlabPoolHead_t *)Slab_PoolSurf;
    OS_Enter_Critical_Check();
    /* 初始化slab控制头 */
    for( int idx = 0; idx < ELOS_SLAB_CHUNK_MAX_LEVEL; ++idx, p_SlabPool++ ){
        SLAB_POOL_HEAD_INIT(p_SlabPool);
        (*p_SlabPool).ckColorCnt = NULL;
        (*p_SlabPool).SlabObjSize = ELOS_SLAB_BASIC_OBJECT_SIZE << 1;
        (*p_SlabPool).SlabZoneSize = g_SlabPoolSize[idx];
        (*p_SlabPool).SlabZoneMountedCnt = (EL_UINT)0;
        ELOS_SlabDispenserAlloc(Slab_PoolSurf,(*p_SlabPool).SlabObjSize);
        (*p_SlabPool).SlabZoneMountedCnt ++;
    }

    SlabZoneResizeAll(ELOS_PER_SLAB_ZONE_DATA_SIZE);
    OS_Exit_Critical_Check();
    return;
}

/* 在slab组分配新的slab分配器 */
el_SlabDispr_t * ELOS_SlabDispenserAlloc(void *Slab_PoolSurf)
{
    el_SlabPoolHead_t *pSlabHead = (el_SlabPoolHead_t*)Slab_PoolSurf;
    el_SlabDispr_t * SlabDisprToAlloc = NULL;

    if(Slab_PoolSurf == NULL) return NULL;
    EL_UINT ZoneSizeNeed = sizeof(el_SlabDispr_t) + ELOS_PER_SLAB_ZONE_DATA_SIZE;

    SlabDisprToAlloc = (el_SlabDispr_t *)malloc(ZoneSizeNeed);
    if( SlabDisprToAlloc == NULL ) return (SlabDisprToAlloc *)0;

    memset( (void *)SlabDisprToAlloc, 0, ZoneSizeNeed);
    /* slab分配器初始化 */
    SLAB_DISPENSER_INIT(SlabDisprToAlloc);
    /* 添加到绿着色器列表 */
    list_add_tail( &SlabDisprToAlloc->SlabNode, &pSlabHead->ckGreenListHead);
    /* slab数据池面指针初始化 */
    SlabDisprToAlloc->obj_pool_data = (void *)( SlabDisprToAlloc + 1 );
    SlabDisprToAlloc->SlabObjCnt = pSlabHead->SlabZoneSize/pSlabHead->SlabObjSize;
    SlabDisprToAlloc->SlabObjUsed = 0;
    OS_SLAB_DIPSR_OWNER_SET( SlabDisprToAlloc,pSlabHead );
    //SlabDisprToAlloc->NotifyControlModifyCr = ELOS_SlabMvColorNodeToAnother;

    ELOS_SlabDisprBitmapInitialise(SlabDisprToAlloc);
    return SlabDisprToAlloc;
}
/* 在slab组释放一个slab分配器 */
EL_UINT * ELOS_SlabDispenserFree(el_SlabDispr_t * pSlabDispr)
{
    el_SlabPoolHead_t *pSlabHead;
    el_SlabDispr_t * SlabDisprToAlloc = NULL;
    if(pSlabDispr == NULL) return NULL;

    OS_Enter_Critical_Check();
    pSlabHead = (el_SlabPoolHead_t *)pSlabDispr->SlabOwner;
    pSlabHead->SlabZoneMountedCnt --;
    free((void *)pSlabDispr);
    OS_Exit_Critical_Check();
    return (EL_UINT)EL_RESULT_OK;
}
/* 向slab分组添加新的slab块 */
EL_UINT ELOS_SlabGroupAdd(el_SlabPoolHead_t *pSlabHead, EL_UINT obj_size)
{
    el_SlabDispr_t * SlabDisprToAlloc = NULL;
    for(int idx = 0 ; idx < ELOS_SLAB_CHUNK_MAX_LEVEL ; ++i)
    {
        if( (pSlabHead ++)->SlabObjSize == obj_size){
            pSlabHead --;
            SlabDisprToAlloc = ELOS_SlabDispenserAlloc((void *)pSlabHead);
        }
    }
    return (SlabDisprToAlloc == NULL)?EL_RESULT_ERR:EL_RESULT_OK;
}

EL_UINT ELOS_SlabMvColorNodeToAnother(el_SlabDispr_t * pSlabDispr,\
                fELOS_SlabColorConvIdxTake SlabColorConvIdxTake)
{
    LIST_HEAD * pListNode;EL_CHAR cvIdx;
    el_SlabPoolHead_t * pSlabDipsrOwner;
    if(pSlabDispr == NULL)  return (EL_UINT)EL_RESULT_ERR;

    pSlabDipsrOwner = OS_SLAB_DIPSR_OWNER_GET(pSlabDispr);/* 获取slab分配器的持有者 */

    pListNode = pSlabDispr->SlabNode;/* slab分配器节点 */

    OS_Enter_Critical_Check();
    /* 从原着色器列表删除 */
    list_del( pListNode );
    cvIdx = SlabColorConvIdxTake(pSlabDispr);
    /* 添加到新的着色器列表 */
    list_add_tail( pListNode, pSlabDipsrOwner->ckYellowListHead );
    /* 更新着色器数目 */
    pSlabDispr->SlabObjUsed += cvIdx;
    OS_Exit_Critical_Check();

    return (EL_UINT)EL_RESULT_OK; 
}

/* 从slab区分配一个obj */
void * ELOS_SlabObjAlloc(el_SlabDispr_t * pSlabDispr)
{
    void * pObj;
    EL_UINT ObjIndex;
    
    if( pSlabDispr == NULL ) return NULL;

    OS_Enter_Critical_Check();
    ASSERT( pSlabDispr->SlabObjCnt > pSlabDispr->SlabObjUsed );
    /* 从位图表中找出 */
    ObjIndex = (pSlabDispr->SlabObjCnt == pSlabDispr->SlabObjUsed)?0:\
                ELOS_SlabSDisprBitmapFindBitAndSet(NULL);
    if(ObjIndex == (EL_UINT)0){
        /* 重新分配一个slab区块 */
        if( EL_RESULT_ERR == ELOS_SlabGroupAdd(el_SlabPoolHead_t *pSlabHead, EL_UINT obj_size)){
            return NULL;
        }
        ELOS_SlabObjAlloc( el_SlabDispr_t * pSlabDispr);
    }
    pObj = pSlabDispr->obj_pool_data + ObjIndex * pSlabDispr->SlabObjCnt;
    /* 更新着色器列表 */
    ELOS_SlabMvColorNodeToAnother(pSlabDispr,ELOS_SlabColorConvDeepIdxTake);
    OS_Exit_Critical_Check();
    return pObj;
}
/* 从slab区释放一个obj */
void ELOS_SlabFreeObject(el_SlabDispr_t * pSlabDispr, void *pObj)
{
    if((pSlabDispr == NULL)||(pObj == NULL)) return;
    ELOS_SlabSDisprBitmapFindBitAndClear(  );
    pSlabDispr->SlabObjUsed --;
}

/* 分配一个obj,必须为slab分组首地址 */
void *ELOS_SlabMemAlloc(void * SlabPoolSurf,EL_UINT NeedAllocSize)
{
    el_SlabPoolHead_t *SlabHead = (el_SlabPoolHead_t *)SlabPoolSurf;
    LIST_HEAD_CREAT( *pSlabZoneToAllocHeadNode );
    void * pObj = NULL;
    if( (SlabPoolSurf== NULL) || (NeedAllocSize == 0) ) return NULL;

    /* 采用最小适配算法 */
    for(int s_idx = 0 ; s_idx < ELOS_SLAB_CHUNK_MAX_LEVEL; s_idx++){
        SlabHead += s_idx;
        /* 保证内存使用率大于50% */
        if( (SlabHead->SlabObjSize >= NeedAllocSize) &&\
             (SlabHead->SlabObjSize <= 2*NeedAllocSize) ){
            /* 优先从黄链分配，接着是绿链，不够就分配新的slab区块 */
            pSlabZoneToAllocHeadNode = &SlabHead->ckYellowListHead;
            if( list_empty( pSlabZoneToAllocHeadNode ) ){
                /* 黄链没有就从绿链分配 */
                pSlabZoneToAllocHeadNode = &SlabHead->ckGreenListHead;
                if( list_empty( pSlabZoneToAllocHeadNode ) ){
                    /* 没有绿链就从系统内存申请 */
                    if((EL_UINT)EL_RESULT_ERR ==  ELOS_SlabGroupAdd(SlabHead, SlabHead->SlabObjSize)){
                        return (void *)0;
                    }else{
                        pObj = ELOS_SlabObjAlloc( SlabHead->ckGreenListHead );
                        ASSERT( pObj != (void *)0 );
                        return (void *)pObj;
                    }
                }
            }
            pObj = ELOS_SlabObjAlloc( SlabHead->ckYellowListHead );
        }else continue;
    }
    return (void *)pObj;
}

/* 释放一个obj */
void ELOS_SlabMemFree(void *pObj)
{
    EL_UINT itemIndex;EL_UCHAR ** SlabDisprAddr;
    if(pObj == NULL) return;
    EL_UINT ObjIdx;
    /* 获取obj所在slab控制块，slab分配器首地址 */
    ELOS_GetObjSlabDisprAddr(EL_UCHAR *plabDisprAddr,&ObjIdx);

    //EL_UINT itemSize = pSlabHead->SlabObjSize;/* 获取obj大小 */
    //itemIndex = pSlabHead->SlabZoneSize/itemSize;/* 获取该obj所在位图的索引 */
    ELOS_SlabMvColorNodeToAnother(pSlabDispr);
}

/* 由obj获取所在slab分配器地址 */
void * ELOS_GetObjSlabDisprAddr(EL_UCHAR *plabDisprAddr,EL_UINT *pObjIdx)
{
    
    *pObjIdx = 0;/* obj所在的slab索引 */
}