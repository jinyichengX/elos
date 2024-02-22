#ifndef  PORT_H
#define PORT_H
#include "el_type.h"

//extern uint32_t SystemCoreClock;
#define CPU_CLOCK_FREQ 200000000

#ifdef __NVIC_PRIO_BITS
#define CPU_MAX_INT_PRIO_BITS	__NVIC_PRIO_BITS	/* 表示中断优先级的位数 */
#else
#define CPU_MAX_INT_PRIO_BITS	4	/* 表示中断优先级的位数，4位表示16个优先级 */
#endif
#define CPU_MAX_INT_PRIO_LEVEL  2^CPU_MAX_INT_PRIO_BITS /* 4位最大表示16个优先级 */
#define CPU_LOWEST_INT_PRIO_VAL (CPU_MAX_INT_PRIO_LEVEL<<(8-CPU_MAX_INT_PRIO_BITS))/* 最低优先级的寄存器值 */

/* 不带中断保护版本，避免在中断中使用 */
#define CPU_ENTER_CRITICAL_NOCHECK() __asm("cpsid i")
#define CPU_EXIT_CRITICAL_NOCHECK() __asm("cpsie i")

typedef unsigned char EL_PORT_UCHAR;
typedef unsigned short EL_PORT_USHORT;
typedef unsigned int EL_PORT_UINT;

typedef EL_PORT_UINT EL_PORT_REG_TYPE;
typedef EL_PORT_REG_TYPE EL_PORT_STACK_TYPE;

extern void PORT_CPU_Initialise(void);
extern void *PORT_Initialise_pthread_stack(EL_PORT_STACK_TYPE * _STACK,void *PHREAD_ENTRY);
extern void PORT_Start_Scheduler(void);
extern void PORT_PendSV_Suspend(void);
extern void portStartScheduler(void);
extern void OS_Enter_Critical_Check(void);
extern void OS_Exit_Critical_Check(void);

//extern EL_UINT g_critical_nesting;
/* 内核寄存器的定义 */
#endif // ! PORT_H.

