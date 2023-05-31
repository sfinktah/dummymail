#define USE_PTH

#ifndef USE_PTH
#define pth_send send
#define pth_recv recv
#define pth_select select
#define pth_connect connect
#else
#include <pth.h>
#endif

