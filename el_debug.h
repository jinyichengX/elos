#ifndef EL_DEBUG_H
#define EL_DEBUG_H

#include "port.h"

#define CPU_INT_ON() CPU_ENTER_CRITICAL_NOCHECK()
#define ASSERT_DEBUG_ON 1

#if ASSERT_DEBUG_ON
#define ASSERT(x) if(x) ;else{CPU_INT_ON();while(1){}};
#else
#define ASSERT(x) ((void)0)
#endif

#endif