#ifndef EL_SPEEDPIPE_H
#define EL_SPEEDPIPE_H
#include "el_type.h"
#include "el_list.h"
#include "el_pthread.h"

/* List of flags */
#define LWRB_FLAG_READ_ALL  ((uint16_t)0x0001)
#define LWRB_FLAG_WRITE_ALL ((uint16_t)0x0001)
typedef enum {
    LWRB_EVT_READ,  /*!< Read event */
    LWRB_EVT_WRITE, /*!< Write event */
    LWRB_EVT_RESET, /*!< Reset event */
} lwrb_evt_type_t;
#define EL_SPEEDPIPE_STATE_EMPTY 0X0000
#define EL_SPEEDPIPE_STATE_IDLE 0X0000
#define EL_SPEEDPIPE_STATE_WRITING 0X0001
#define EL_SPEEDPIPE_STATE_READING 0X0002
#define EL_SPEEDPIPE_STATE_BUSY\
		EL_SPEEDPIPE_STATE_WRITING|\
		EL_SPEEDPIPE_STATE_READING
#define EL_SPEEDPIPE_STATE_INVALID 0X0004
#define EL_SPEEDPIPE_STATE_FULL  0X0008

typedef enum EL_SPEEDPIPE_STATE{
	EMPTY = EL_SPEEDPIPE_STATE_EMPTY,
	IDLE = EL_SPEEDPIPE_STATE_IDLE,
	WRITING = EL_SPEEDPIPE_STATE_WRITING,
	READING = EL_SPEEDPIPE_STATE_READING,
	BUSY = EL_SPEEDPIPE_STATE_BUSY,
	INVALI = EL_SPEEDPIPE_STATE_INVALID,
	FULL = EL_SPEEDPIPE_STATE_FULL
}SPEEDPIPE_STATE_T;

typedef void (*f_callback)(void);

typedef enum EL_HIGHTSPEED_PIPE_IF_CALLBACK{
	EL_SPEEDPIPE_EVT_ON,
	EL_SPEEDPIPE_EVT_OFF
}callback_evt_t;

/* IPC参数 */
typedef struct IPC_BUFF_INFO_STRUCT{
	EL_UCHAR * iptc_buff;/* 指向ipc数据的指针 */
	EL_UINT iptc_size;/* ipc缓冲区大小 */
}ipc_info_t;

/* 高速队列 */
typedef struct EL_HIGHTSPEED_PIPE_STRUCT
{
	ipc_info_t SpeedPipeInfo;/* 高速队列基本参数 */ 
	EL_UINT SpeedPipe_rd;/* 读索引 */
	EL_UINT SpeedPipe_wr;/* 写索引 */
	LIST_HEAD Waiters_rd;/* 等待读此高速队列的线程 */
	LIST_HEAD Waiters_wr;/* 等待写此高速队列的线程 */
	SPEEDPIPE_STATE_T SpeedPipe_state;/* 高速队列状态 */
	callback_evt_t evt_on;/* 是否支持回调 */
	f_callback p_func;/* 回调事件 */
}speed_pipe_t;

extern EL_UCHAR EL_HighSpeedPipe_Init(speed_pipe_t * pipe, void * PipeBuff, EL_UINT PipeSize);
extern speed_pipe_t * EL_HighSpeedPipe_Create(EL_UINT PipeSize);
extern EL_UINT EL_HighSpeedPipe_NonAtomic_Push(speed_pipe_t * pipe, void * data,\
                                EL_UINT btw, EL_UINT * bw, EL_USHORT flags);
extern EL_UINT EL_HighSpeedPipe_NonAtomic_Pop(speed_pipe_t * pipe, void * data, EL_UINT btr,\
                                EL_UINT * br, EL_USHORT flags);
extern EL_UINT EL_HighSpeedPipe_NonAtomic_Peek(speed_pipe_t * pipe, EL_UINT skip_count, void * data, EL_UINT btp);
#endif