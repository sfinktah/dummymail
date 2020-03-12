#include        <stdio.h>
#include        <stdlib.h>

#include "socket.h"
#ifdef PTH
#include "/usr/include/pth.h"
#endif

extern char* get_url(char* url);
extern char* get_url_proxied(char* url, char *proxy);

