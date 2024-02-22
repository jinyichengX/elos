#include "el_speedpipe.h"
#include "el_debug.h"

extern LIST_HEAD_CREAT(*KERNEL_LIST_HEAD[EL_PTHREAD_STATUS_COUNT]);/* 内核列表指针 */

#define BUF_IS_VALID(P) ((P) != NULL &&\
		(P->SpeedPipeInfo).iptc_buff != NULL &&\
		(P->SpeedPipeInfo).iptc_size > 0)
#define BUF_MIN(x, y)   ((x) < (y) ? (x) : (y))
#define BUF_MAX(x, y)   ((x) > (y) ? (x) : (y))

/* 队列缓冲区是否就绪 */
EL_UCHAR EL_HighSpeedPipe_IsReady(speed_pipe_t* pipe)
{
	return BUF_IS_VALID(pipe);
}

/* 内存拷贝 */
static void EL_OS_MemCpy(EL_UCHAR *_tar, EL_UCHAR *_src, EL_UINT len) {
    if ((NULL == _tar) || (NULL == _src) || (0 == len)) return;
    EL_UINT temp = len/sizeof(EL_UINT),i;
    for(i=0; i<temp; i++) ((EL_UINT *)_tar)[i] = ((EL_UINT *)_src)[i];
    i *= sizeof(EL_UINT);
    for(;i<len;i++) _tar[i] = _src[i];
}

#define EL_HighSpeedPipe_PushPop_NoCheck(PTAR,PSRC,LEN)\
		EL_OS_MemCpy(PTAR, PSRC, LEN)
/**********************************************************************
 * 函数名称： EL_HighSpeedPipe_Init
 * 功能描述： 高速队列初始化
 * 输入参数： pipe ：已创建的队列对象
             PipeBuff ：队列首地址
             PipeSize ：队列大小
 * 输出参数： 无
 * 返 回 值： 0 ：初始化成功
             1 ：初始化失败
 * 修改日期        版本号     修改人	      修改内容
 * -----------------------------------------------
 * 2024/02/20	    V1.0	  jinyicheng	      创建
 ***********************************************************************/
EL_UCHAR EL_HighSpeedPipe_Init(speed_pipe_t * pipe, void * PipeBuff, EL_UINT PipeSize)
{
    if (pipe == NULL || PipeBuff == NULL || PipeSize == 0) {
        return 1;
    }

    pipe->p_func = (f_callback)NULL;/* 无回调 */
    pipe->SpeedPipeInfo.iptc_size = PipeSize;
    pipe->SpeedPipeInfo.iptc_buff = PipeBuff;
	pipe->SpeedPipe_rd = pipe->SpeedPipe_wr = 0;
	pipe->SpeedPipe_state = IDLE;
    INIT_LIST_HEAD(&pipe->Waiters_rd);
    INIT_LIST_HEAD(&pipe->Waiters_wr);
    return 0;
}
/**********************************************************************
 * 函数名称： EL_HighSpeedPipe_Create
 * 功能描述： 创建高速队列
 * 输入参数： PipeSize ：队列大小
 * 输出参数： 无
 * 返 回 值： 队列指针
 * 修改日期        版本号     修改人	      修改内容
 * -----------------------------------------------
 * 2024/02/20	    V1.0	  jinyicheng	      创建
 ***********************************************************************/
speed_pipe_t * EL_HighSpeedPipe_Create(EL_UINT PipeSize)
{
    ASSERT(PipeSize != 0);
    speed_pipe_t * new_pipe = NULL;
    /* 为高速队列分配空间 */
    new_pipe = (speed_pipe_t *)malloc(sizeof(speed_pipe_t) + PipeSize);
    if(new_pipe == NULL) return (speed_pipe_t *)0;
    EL_HighSpeedPipe_Init(new_pipe, (EL_UCHAR *)new_pipe+sizeof(speed_pipe_t), PipeSize);
    /* 到这里高速队列应该就绪 */
    ASSERT(EL_HighSpeedPipe_IsReady(new_pipe));
    return new_pipe;
}
/**********************************************************************
 * 函数名称： EL_HighSpeedPipe_Get_Free
 * 功能描述： 获取缓冲区中用于写入操作的可用大小
 * 输入参数： pipe ：已创建的队列对象
 * 输出参数： 无
 * 返 回 值： 队列可填充大小
 * 修改日期        版本号     修改人	      修改内容
 * -----------------------------------------------
 * 2024/02/20	    V1.0	  jinyicheng	      创建
 ***********************************************************************/
EL_UINT EL_HighSpeedPipe_Get_Free(speed_pipe_t * pipe)
{
    EL_UINT PipeSize, i_wr, i_rd;
    if (!BUF_IS_VALID(pipe)) return 0;

    i_wr = pipe->SpeedPipe_wr;
    i_rd = pipe->SpeedPipe_rd;
    if (i_wr >= i_rd) PipeSize = pipe->SpeedPipeInfo.iptc_size - (i_wr - i_rd);
    else PipeSize = i_rd - i_wr;

    return PipeSize - 1;
}
/**********************************************************************
 * 函数名称： EL_HighSpeedPipe_Get_Full
 * 功能描述： 获取缓冲区中当前可用的字节数
 * 输入参数： pipe ：已创建的队列对象
 * 输出参数： 无
 * 返 回 值： 队列可读取数据长度
 * 修改日期        版本号     修改人	      修改内容
 * -----------------------------------------------
 * 2024/02/20	    V1.0	  jinyicheng	      创建
 ***********************************************************************/
EL_UINT EL_HighSpeedPipe_Get_Full(speed_pipe_t * pipe)
{
    EL_UINT PipeSize, i_wr, i_rd;

    if (!BUF_IS_VALID(pipe)) return 0;
    i_wr = pipe->SpeedPipe_wr;
    i_rd = pipe->SpeedPipe_rd;

    if (i_wr >= i_rd) PipeSize = i_wr - i_rd;
    else PipeSize = pipe->SpeedPipeInfo.iptc_size - (i_rd - i_wr);

    return PipeSize;
}
/**********************************************************************
 * 函数名称： EL_HighSpeedPipe_NonAtomic_Push
 * 功能描述： 向队列非原子地写数据
 * 输入参数： pipe ：已创建的队列对象
             data ：数据首地址
             btw ：需要填充的数据长
 * 输出参数： 无
 * 返 回 值： 无
 * 修改日期        版本号     修改人	      修改内容
 * -----------------------------------------------
 * 2024/02/20	    V1.0	  jinyicheng	      创建
 ***********************************************************************/
EL_UINT EL_HighSpeedPipe_NonAtomic_Push(speed_pipe_t * pipe, void * data,\
                                EL_UINT btw, EL_UINT * bw, EL_USHORT flags)
{
    EL_UINT tocopy, free, buff_w_ptr;
    EL_UCHAR * d = data;

    if (!BUF_IS_VALID(pipe) || data == NULL || btw == 0) {
        return 0;
    }

    /* 计算可供写入的最大字节数 */
    //free = EL_HighSpeedPipe_Get_Free(pipe);
    /* 如果没有内存，或者如果用户想要写入所有数据但没有足够的空间，请提前退出 */
    //if (free == 0 || (free < btw && flags & LWRB_FLAG_WRITE_ALL)) {
    //    return 0;
    //}
    //btw = BUF_MIN(free, btw);
    buff_w_ptr = pipe->SpeedPipe_wr;

    /* 将数据写入缓冲区的线性部分 */
    tocopy = BUF_MIN(pipe->SpeedPipeInfo.iptc_size - buff_w_ptr, btw);
    EL_HighSpeedPipe_PushPop_NoCheck(&pipe->SpeedPipeInfo.iptc_buff[buff_w_ptr], d, tocopy);
    buff_w_ptr += tocopy;
    btw -= tocopy;
    /* 将数据写入缓冲区的开头（溢出部分） */
    if (btw > 0) {
        EL_HighSpeedPipe_PushPop_NoCheck(pipe->SpeedPipeInfo.iptc_buff, &d[tocopy], btw);
        buff_w_ptr = btw;
    }
    /* 检查缓冲区末尾 */
    if (buff_w_ptr >= pipe->SpeedPipeInfo.iptc_size) {
        buff_w_ptr = 0;
    }
    /* 将最终值写入实际运行变量，这是为了确保没有读取操作可以访问中间数据 */
    pipe->SpeedPipe_wr = buff_w_ptr;
    //BUF_SEND_EVT(buff, LWRB_EVT_WRITE, tocopy + btw);
    if (bw != NULL) {
        *bw = tocopy + btw;
    }
    return 1;
}
/**********************************************************************
 * 函数名称： EL_HighSpeedPipe_NonAtomic_Pop
 * 功能描述： 从队列非原子地读数据
 * 输入参数： pipe ：已创建的队列对象
             data ：数据首地址
             btw ：需要读出的数据长
 * 输出参数： 无
 * 返 回 值： 无
 * 修改日期        版本号     修改人	      修改内容
 * -----------------------------------------------
 * 2024/02/20	    V1.0	  jinyicheng	      创建
 ***********************************************************************/
EL_UINT EL_HighSpeedPipe_NonAtomic_Pop(speed_pipe_t * pipe, void * data, EL_UINT btr,\
                                EL_UINT * br, EL_USHORT flags)
{
    EL_UINT tocopy, full, buff_r_ptr;
    EL_UCHAR * d = data;

    if (!BUF_IS_VALID(pipe) || data == NULL || btr == 0) {
        return 0;
    }

    /* 计算可读出的最大字节数 */
    //full = EL_HighSpeedPipe_Get_Full(pipe);
    //if (full == 0 || (full < btr && (flags & LWRB_FLAG_READ_ALL))) {
    //    return 0;
    //}
    btr = BUF_MIN(full, btr);
    buff_r_ptr = pipe->SpeedPipe_rd;

    /* 将数据读出缓冲区的线性部分 */
    tocopy = BUF_MIN(pipe->SpeedPipeInfo.iptc_size - buff_r_ptr, btr);
    EL_HighSpeedPipe_PushPop_NoCheck(d, &pipe->SpeedPipeInfo.iptc_buff[buff_r_ptr], tocopy);
    buff_r_ptr += tocopy;
    btr -= tocopy;
    /* 将数据从缓冲区的开头读出（溢出部分） */
    if (btr > 0) {
        EL_HighSpeedPipe_PushPop_NoCheck(&d[tocopy], pipe->SpeedPipeInfo.iptc_buff, btr);
        buff_r_ptr = btr;
    }
    /* 检查缓冲区末尾 */
    if (buff_r_ptr >= pipe->SpeedPipeInfo.iptc_size) {
        buff_r_ptr = 0;
    }

    /* 将最终值写入实际运行变量，这是为了确保没有读取操作可以访问中间数据 */
    pipe->SpeedPipe_rd = buff_r_ptr;
    //BUF_SEND_EVT(buff, LWRB_EVT_READ, tocopy + btr);
    if (br != NULL) {
        *br = tocopy + btr;
    }
    return 1;
}
/**********************************************************************
 * 函数名称： EL_HighSpeedPipe_NonAtomic_Peek
 * 功能描述： 偷取队列数据，不改变读索引
 * 输入参数： pipe ：已创建的队列对象
             skip_count ：从都索引开始跳过的数据长
             data ：数据首地址
             btp ：需要读出的数据长
 * 输出参数： 无
 * 返 回 值： 无
 * 修改日期        版本号     修改人	      修改内容
 * -----------------------------------------------
 * 2024/02/20	    V1.0	  jinyicheng	      创建
 ***********************************************************************/
EL_UINT EL_HighSpeedPipe_NonAtomic_Peek(speed_pipe_t * pipe, EL_UINT skip_count, void * data, EL_UINT btp)
{
    EL_UINT full, tocopy, r;
    EL_UCHAR * d = data;

    if (!BUF_IS_VALID(pipe) || data == NULL || btp == 0) {
        return 0;
    }
    /* 计算可读出的最大字节数 */
    full = EL_HighSpeedPipe_Get_Full(pipe);
    if (skip_count >= full) {
        return 0;
    }
    r = pipe->SpeedPipe_rd;
    r += skip_count;
    full -= skip_count;
    if (r >= pipe->SpeedPipeInfo.iptc_size) {
        r -= pipe->SpeedPipeInfo.iptc_size;
    }
    /* 检查跳过后可供读取的最大字节数 */
    btp = BUF_MIN(full, btp);
    if (btp == 0) {
        return 0;
    }
    /* 从缓冲区的线性部分读取数据 */
    tocopy = BUF_MIN(pipe->SpeedPipeInfo.iptc_size - r, btp);
    EL_HighSpeedPipe_PushPop_NoCheck(d, &pipe->SpeedPipeInfo.iptc_buff[r], tocopy);
    btp -= tocopy;

    /* 从缓冲区（溢出部分）开始读取数据 */
    if (btp > 0) {
        EL_HighSpeedPipe_PushPop_NoCheck(&d[tocopy], pipe->SpeedPipeInfo.iptc_buff, btp);
    }
    return tocopy + btp;
}
/**********************************************************************
 * 函数名称： EL_HighSpeedPipe_Push
 * 功能描述： 高速队列写消息
 * 输入参数： pipe ：已创建的队列对象
             data ：数据首地址
             Mess_len ：需要填充的数据长
             TicksToWait ：若无可填充空间，等待的时间片长
 * 输出参数： 无
 * 返 回 值： EL_RESULT_OK/EL_RESULT_ERR
 * 修改日期        版本号     修改人	      修改内容
 * -----------------------------------------------
 * 2024/02/20	    V1.0	  jinyicheng	      创建
 ***********************************************************************/
EL_RESULT_T EL_HighSpeedPipe_Push(speed_pipe_t * pipe, void * data,EL_UINT Mess_len, EL_UINT TicksToWait)
{
    /* 获取当前线程 */
	EL_PTCB_T *Cur_ptcb = EL_GET_CUR_PTHREAD();
    Suspend_t *SuspendObj;/* 队列满，非阻塞等待 */
    EL_UINT PipeFree_field_len = 0;/* 队列未满区域长度 */
    EL_PTCB_T * ptcbToNotify = NULL;
    if(!Mess_len) return EL_RESULT_ERR;
    OS_Enter_Critical_Check();

    if(TicksToWait == 0xffffffff){/* 一直阻塞等待有足够的空闲空间 */
        while((PipeFree_field_len = EL_HighSpeedPipe_Get_Free(pipe)) < Mess_len){
            //ASSERT(Cur_ptcb->pthread_state != EL_PTHREAD_SUSPEND);
#if (EL_HIGHSPPEDPIPE_SPIN_PEND != 0)
            /* 将线程放入等待列表 */
            if(list_empty(&pipe->Waiters_wr)){
                list_add_tail(&EL_GET_CUR_PTHREAD()->pthread_node,\
                            &pipe->Waiters_wr);
                PTHREAD_STATE_SET(Cur_ptcb,EL_PTHREAD_SUSPEND);
            }
#else
#endif
            OS_Exit_Critical_Check();
		    PORT_PendSV_Suspend();
		    OS_Enter_Critical_Check();
        }
        /* 从等待列表移除 */
        ASSERT(!list_empty(&pipe->Waiters_wr));
        list_del(pipe->Waiters_wr.next);
        PTHREAD_STATE_SET(Cur_ptcb,EL_PTHREAD_READY);
    }
    /* 队列已满 */
    else if((PipeFree_field_len = EL_HighSpeedPipe_Get_Free(pipe)) < Mess_len){
        if(TicksToWait == 0) {/* 如果不超时等待 */
            OS_Exit_Critical_Check();
            PORT_PendSV_Suspend();return EL_RESULT_ERR;
        }
        OS_Exit_Critical_Check();
        EL_Pthread_Sleep(TicksToWait);/* 线程休眠 */
        OS_Enter_Critical_Check();
    }

    /* 写空闲队列 */
    EL_HighSpeedPipe_NonAtomic_Push(pipe,data,Mess_len,NULL,LWRB_FLAG_WRITE_ALL);
    /* 唤醒读列表中的线程 */
    if(!list_empty(&pipe->Waiters_rd)){
#if (EL_HIGHSPPEDPIPE_SPIN_PEND != 0)
        ptcbToNotify = (EL_PTCB_T *)(&(pipe->Waiters_rd.next));
        list_del(pipe->Waiters_rd.next);
        PTHREAD_STATE_SET(ptcbToNotify,EL_PTHREAD_READY);
//		list_add( (EL_PTCB_T *)pipe->Waiters_rd.next->pthread_node, KERNEL_LIST_HEAD[EL_PTHREAD_READY]+ptcbToNotify->pthread_prio);
#else
#endif
    }
    OS_Exit_Critical_Check();
    return EL_RESULT_OK;
}
/**********************************************************************
 * 函数名称： EL_HighSpeedPipe_Pop
 * 功能描述： 高速队列读消息
 * 输入参数： pipe ：已创建的队列对象
             data ：数据首地址
             Mess_len ：需要填充的数据长
             TicksToWait ：若无可读取数据，等待的时间片长
 * 输出参数： 无
 * 返 回 值： EL_RESULT_OK/EL_RESULT_ERR
 * 修改日期        版本号     修改人	      修改内容
 * -----------------------------------------------
 * 2024/02/20	    V1.0	  jinyicheng	      创建
 ***********************************************************************/
EL_RESULT_T EL_HighSpeedPipe_Pop(speed_pipe_t * pipe, void * data,EL_UINT Mess_len, EL_UINT TicksToWait)
{
    /* 获取当前线程 */
	EL_PTCB_T *Cur_ptcb = EL_GET_CUR_PTHREAD();
    TickPending_t * TickPend;/* 队列满，非阻塞等待 */
    EL_UINT PipeFull_field_len = 0;/* 队列未读区域长度 */
        EL_PTCB_T * ptcbToNotify = NULL;
    if(!Mess_len) return EL_RESULT_ERR;
    OS_Enter_Critical_Check();

        if(TicksToWait == 0xffffffff){/* 一直阻塞等待有足够的空闲空间 */
        while((PipeFull_field_len = EL_HighSpeedPipe_Get_Full(pipe)) < Mess_len){
            //ASSERT(Cur_ptcb->pthread_state != EL_PTHREAD_SUSPEND);
#if (EL_HIGHSPPEDPIPE_SPIN_PEND != 0)
            /* 将线程放入等待列表 */
            if(list_empty(&pipe->Waiters_rd)){
                list_add_tail(&EL_GET_CUR_PTHREAD()->pthread_node,\
                            &pipe->Waiters_rd);
                PTHREAD_STATE_SET(Cur_ptcb,EL_PTHREAD_SUSPEND);
            }
#else
#endif
            OS_Exit_Critical_Check();
		    PORT_PendSV_Suspend();
		    OS_Enter_Critical_Check();
        }
        /* 从等待列表移除 */
        ASSERT(!list_empty(&pipe->Waiters_rd));
        list_del(pipe->Waiters_rd.next);
        PTHREAD_STATE_SET(Cur_ptcb,EL_PTHREAD_READY);
    }
    /* 队列已满 */
    else if((PipeFull_field_len = EL_HighSpeedPipe_Get_Free(pipe)) < Mess_len){
        if(TicksToWait == 0) {/* 如果不超时等待 */
            OS_Exit_Critical_Check();
            PORT_PendSV_Suspend();return EL_RESULT_ERR;
        }
        OS_Exit_Critical_Check();
        EL_Pthread_Sleep(TicksToWait);/* 线程休眠 */
        OS_Enter_Critical_Check();
    }

    /* 写空闲队列 */
    EL_HighSpeedPipe_NonAtomic_Pop(pipe,data,Mess_len,NULL,LWRB_FLAG_WRITE_ALL);
    /* 唤醒读列表中的线程 */
    if(!list_empty(&pipe->Waiters_wr)){
#if (EL_HIGHSPPEDPIPE_SPIN_PEND != 0)
        ptcbToNotify = (EL_PTCB_T *)(&(pipe->Waiters_wr.next));
        list_del(pipe->Waiters_wr.next);
        PTHREAD_STATE_SET(ptcbToNotify,EL_PTHREAD_READY);
//		list_add(&ptcb->pthread_node,KERNEL_LIST_HEAD[EL_PTHREAD_READY]+ptcbToNotify->pthread_prio);
#else
#endif
    }
    OS_Exit_Critical_Check();
    return EL_RESULT_OK;
}