#ifndef ELIGHTOS_CONFIG_H
#define ELIGHTOS_CONFIG_H

/* 定义最多优先级 */
#define EL_MAX_LEVEL_OF_PTHREAD_PRIO 8

/* 定义线程名长度 */
#define EL_PTHREAD_NAME_MAX_LEN 10

/* 计算CPU使用率 */
#define EL_CLAC_CPU_USAGE_RATE 0
#if EL_CALC_CPU_USAGE_RATE
#define EL_CALC_CPU_USAGE_RATE_STAMP_TICK 50
#endif

/* 查看线程栈使用率 */
#define EL_CALC_PTHREAD_STACK_USAGE_RATE 1

#define EL_CFG_DEFAULT_PTHREAD_STACK_SIZE 64

/* 高速队列自旋阻塞模式 */
#define EL_HIGHSPPEDPIPE_SPIN_PEND 1

#endif