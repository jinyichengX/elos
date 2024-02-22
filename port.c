#include "port.h"
#include "el_pthread.h"
EL_UINT g_critical_nesting = 0;/* 临界段嵌套,防止发生下列临界段嵌套情况 */
//void function1() {
//    taskENTER_CRITICAL();
//    // 这里执行一些需要在临界区内执行的操作
//    function2(); // 调用另一个需要临界区内执行的函数
//    taskEXIT_CRITICAL();
//}

//void function2() {
//    taskENTER_CRITICAL();
//    // 这里执行一些需要在临界区内执行的操作
//    taskEXIT_CRITICAL();
//}

#define PORT_PendSV_Handler PendSV_Handler

extern EL_PORT_STACK_TYPE g_psp;
extern void EL_Pthread_SwicthContext(void);
#define HWSET_V4(ADDR) ( *((volatile EL_PORT_UINT *)(ADDR)) )/* 写寄存器 */
#ifndef xPSR_INITIALISED_DEFAULT
#define xPSR_INITIALISED_DEFAULT (EL_PORT_REG_TYPE)0x01000000
#endif

/* 定义一些系统级寄存器 */
#ifndef SCS_BASE
#define SCS_BASE            (0xE000E000UL)                            /*!< System Control Space Base Address */
#endif
#ifndef SysTick_BASE
#define SysTick_BASE        (SCS_BASE +  0x0010UL)                    /*!< SysTick Base Address */
#endif
#ifndef NVIC_BASE
#define NVIC_BASE           (SCS_BASE +  0x0100UL)                    /*!< NVIC Base Address */
#endif
#ifndef SCB_BASE
#define SCB_BASE            (SCS_BASE +  0x0D00UL)                    /*!< System Control Block Base Address */
#endif

/* SCB相关 */
#ifndef SCB_ICSR_BASE
#define SCB_ICSR_BASE (SCB_BASE+0X04)
#endif
#ifndef SCB_SHP_BASE
#define SCB_SHP_BASE (SCB_BASE+0X18)
#endif
#ifndef PENDSV_PRIO_SET_BASE
#define PENDSV_PRIO_SET_BASE (SCB_SHP_BASE+10*0X04)
#endif
#ifndef SYSTICK_PRIO_SET_BASE
#define SYSTICK_PRIO_SET_BASE (SCB_SHP_BASE+11*0X04)
#endif

/* Systick相关 */
#ifndef SYSTICK_CTRL_BASE
#define SYSTICK_CTRL_BASE SysTick_BASE
#endif
#ifndef SYSTICK_LOAD_BASE
#define SYSTICK_LOAD_BASE SysTick_BASE+0X04
#endif
#ifndef SYSTICK_VAL_BASE
#define SYSTICK_VAL_BASE SysTick_BASE+0X08
#endif
#ifndef SYSTICK_CALIB_BASE
#define SYSTICK_CALIB_BASE SysTick_BASE+0X0C
#endif

#ifndef SysTick_LOAD_RELOAD_Msk
#define SysTick_LOAD_RELOAD_Msk            (0xFFFFFFUL /*<< SysTick_LOAD_RELOAD_Pos*/)    /*!< SysTick LOAD: RELOAD Mask */
#endif
#ifndef SysTick_CTRL_COUNTFLAG_Pos
#define SysTick_CTRL_COUNTFLAG_Pos         16U                                            /*!< SysTick CTRL: COUNTFLAG Position */
#endif
#ifndef SysTick_CTRL_COUNTFLAG_Msk
#define SysTick_CTRL_COUNTFLAG_Msk         (1UL << SysTick_CTRL_COUNTFLAG_Pos)            /*!< SysTick CTRL: COUNTFLAG Mask */
#endif
#ifndef SysTick_CTRL_CLKSOURCE_Pos
#define SysTick_CTRL_CLKSOURCE_Pos          2U                                            /*!< SysTick CTRL: CLKSOURCE Position */
#endif
#ifndef SysTick_CTRL_CLKSOURCE_Msk
#define SysTick_CTRL_CLKSOURCE_Msk         (1UL << SysTick_CTRL_CLKSOURCE_Pos)            /*!< SysTick CTRL: CLKSOURCE Mask */
#endif
#ifndef SysTick_CTRL_TICKINT_Pos
#define SysTick_CTRL_TICKINT_Pos            1U                                            /*!< SysTick CTRL: TICKINT Position */
#endif
#ifndef SysTick_CTRL_TICKINT_Msk
#define SysTick_CTRL_TICKINT_Msk           (1UL << SysTick_CTRL_TICKINT_Pos)              /*!< SysTick CTRL: TICKINT Mask */
#endif
#ifndef SysTick_CTRL_ENABLE_Pos
#define SysTick_CTRL_ENABLE_Pos             0U                                            /*!< SysTick CTRL: ENABLE Position */
#endif
#ifndef SysTick_CTRL_ENABLE_Msk
#define SysTick_CTRL_ENABLE_Msk            (1UL /*<< SysTick_CTRL_ENABLE_Pos*/)           /*!< SysTick CTRL: ENABLE Mask */
#endif

/* 定义一些系统调用号 */
#ifndef SVC_STRART_SCHED
#define SVC_STRART_SCHED 0/* 系统启动调用号 */
#endif
#ifndef SVC_FS
#define SVC_FS           1  /* 文件系统访问调用号 */
#endif
#ifndef SVC_OTHER
#define SVC_OTHER	     2
#endif

/* 初始化系统时钟 */
static int PORT_OS_TickInitialise(EL_PORT_UINT ticks)
{
  if ((ticks - 1UL) > SysTick_LOAD_RELOAD_Msk)
  {
    return (1UL);                                                   /* Reload value impossible */
  }

  HWSET_V4(SYSTICK_LOAD_BASE)  = (EL_UINT)(ticks - 1UL);                         /* set reload register */
  HWSET_V4(SYSTICK_VAL_BASE)   = 0UL;                                             /* Load the SysTick Counter Value */
  HWSET_V4(SYSTICK_CTRL_BASE)  = SysTick_CTRL_CLKSOURCE_Msk |
                   SysTick_CTRL_TICKINT_Msk   |
                   SysTick_CTRL_ENABLE_Msk;                         /* Enable SysTick IRQ and SysTick Timer */
  return 0;
}

/* 设置线程切换最低优先级 */
#define PORT_Set_Swicth_Pthread_lowest_priority() (HWSET_V4(PENDSV_PRIO_SET_BASE) = CPU_LOWEST_INT_PRIO_VAL)
/* 设置Systick最低优先级 */
#define PORT_Set_OS_Tick_lowest_priority() (HWSET_V4(SYSTICK_PRIO_SET_BASE) = CPU_LOWEST_INT_PRIO_VAL)

/* 硬件相关的设置 */
void PORT_CPU_Initialise(void)
{
    /* 设置线程切换最低优先级 */
    PORT_Set_Swicth_Pthread_lowest_priority();
    /* 设置Systick最低优先级 */
    PORT_Set_OS_Tick_lowest_priority();
    /* 初始化系统时钟 */
    PORT_OS_TickInitialise(CPU_CLOCK_FREQ/1000);//除以1000就是1ms为一个时间片，除以500就是2ms为一个时间片，以此类推
}

/* 初始化线程栈 */
void * PORT_Initialise_pthread_stack(EL_PORT_STACK_TYPE * _STACK,void *PHREAD_ENTRY)
{
    EL_PORT_STACK_TYPE * P_STK = _STACK;
    *P_STK = xPSR_INITIALISED_DEFAULT;

    P_STK --;
    *P_STK = (EL_PORT_STACK_TYPE)PHREAD_ENTRY;
	for(int i = 0;i<6;i++){
		P_STK --;*P_STK = 0;
	}
	P_STK -= 8;
	return (void *)P_STK;
}

void critical_nesting_inc(void)
{
	CPU_ENTER_CRITICAL_NOCHECK();
	
	g_critical_nesting++;
}

void critical_nesting_dec(void)
{
    g_critical_nesting--;

    if(0 == g_critical_nesting)
    {
        CPU_EXIT_CRITICAL_NOCHECK();
    }
}
/* 安全进入临界区 */
__asm void OS_Enter_Critical_Check(void)
{
	extern critical_nesting_inc
	push {lr}
	bl critical_nesting_inc
	pop {lr}
	bx lr
}
/* 安全退出临界区 */
__asm void OS_Exit_Critical_Check(void)
{
	extern critical_nesting_dec
	push {lr}
	bl critical_nesting_dec
	pop {lr}
	bx lr
}
__asm void PORT_SystemCall_Handler(void)
{
    /* 获取系统调用号：开始 */
	tst lr,#0x4 /*测试LR(EXC_RETURN)的比特4*/
	ite eq /*如果为0*/
	mrseq r0,msp /*则使用的是主堆栈，故把MSP的值取出*/
	mrsne r0,psp /*否则, 使用的是进程堆栈，故把MSP的值取出*/
	ldr r1,[r0,#24] /*从栈中读取中断返回前PC的值*/
	ldrb r0,[r1,#-2] /*从SVC指令中读取立即数(thumb指令立即数占8位)放到R0*/
    /* 获取系统调用号：结束 */
    /* 运行对应的系统服务 */
    bx lr
    nop
}

/* 悬起pensv异常 */
void PORT_PendSV_Suspend(void)
{
	*(unsigned int *)0xe000ed04 |= (1<<28);
}


/* 在这里执行线程切换 */
__asm void PORT_PendSV_Handler(void)
{
	extern g_psp
	extern g_pthread_sp
	extern EL_Pthread_SwicthContext
	extern EL_Update_Pthread_Stack_Usage_Percentage
	PRESERVE8;
	cpsid i
	/* 保护上个线程的现场 */
	ldr r1,=g_pthread_sp
	ldr r0,[r1]
	mrs r1,psp
	isb
	stmdb.w r1!,{r4-r11}
	str r1,[r0]
	/* 线程切换 */
	mov r0,lr
	push {lr}/* 保护exc_return */
	dsb
	isb
#if EL_CALC_PTHREAD_STACK_USAGE_RATE
	/* 计算栈使用率 */
	bl EL_Update_Pthread_Stack_Usage_Percentage
#endif
	bl EL_Pthread_SwicthContext
	pop {lr}
	
	ldr r1,=g_psp
	ldr r0,[r1]
	ldmia.w r0!,{r4-r11}
	msr psp,r0
	
	isb
	cpsie i
    /* 异常返回 */
	bx lr
}

/* 手动启动第一个线程，不通过异常返回 */
__asm void PORT_Start_Scheduler(void)
{
	extern g_psp
	ldr r0,=g_psp
	ldr r0,[r0]
	msr psp,r0
	mov r0, #0x2  //设置CONTROL寄存器，让用户程序使用PSP
	msr control,r0 //切入特权线程使用PSP
	
	pop {r4-r11}
	pop {r0-r3,r12}
	pop {lr}
	pop {lr}
	pop {r0}
	msr xpsr,r0
	bx lr
	//nop//加nop是为了避免added 2 bytes of padding at address警告
}

//svc_handler:
//    /* 保存寄存器状态 */

//    /* 读取SVC号 */
//    ldr r7, [lr, #-4]
//    ldr r7, [r7, #-4]

//    /* 根据SVC号分发到相应的处理函数 */
//    cmp r7, #FAT32_READ_FILE
//    beq handle_read_file
//    cmp r7, #FAT32_WRITE_FILE
//    beq handle_write_file
//    /* 其他操作... */

//    /* 恢复寄存器状态 */
//    bx lr

//handle_read_file:
//    /* 处理读取文件的逻辑 */

//handle_write_file:
//    /* 处理写入文件的逻辑 */

