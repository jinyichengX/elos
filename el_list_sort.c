#include "el_list_sort.h"
#include "el_pthread.h"
#include "el_type.h"

/* 列表节点升序插入 */
void EL_Klist_InsertSorted(struct list_head *new, struct list_head *head)
{
    EL_UINT tick,overtick;

    struct list_head *head_pos = head->next;
    /* 获取系统时基 */
    tick = ((TickPending_t *)new)->TickSuspend_Count;
    overtick = ((TickPending_t *)new)->TickSuspend_OverFlowCount;

    while(
		(!list_empty(head))\
        && ((( tick >= ((TickPending_t *)head_pos)->TickSuspend_Count )&&\
        ( overtick == ((TickPending_t *)head_pos)->TickSuspend_OverFlowCount ))\
        ||( overtick > ((TickPending_t *)head_pos)->TickSuspend_OverFlowCount ))
        )
    {
        head_pos = head_pos->next;
    }
    __list_add(new,head_pos->prev,head_pos);
}