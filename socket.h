#include        <stdio.h>
#include        <stdlib.h>
#include        <netdb.h>
#include        <sys/types.h>
#include        <sys/time.h>
#include        <sys/socket.h>
#include        <stdio.h>
#include        <time.h>
#include        <signal.h>
#include        <pthread.h>
#include        <string.h>
#include        <errno.h>
#include        <netinet/in.h>
#include        <unistd.h>

/* Override these later if necessary */

#define CONNECT_TIMEOUT 0
#define PACKET_BUFFER_SIZE 2048
#define IDLE_TIMEOUT 90
#ifndef MSG_NOSIGNAL 
#define MSG_NOSIGNAL 0
#endif

extern int connect_socket(const char *host, int port, const char *source);
extern int xfer_data(int s1, int s2);
extern int link_sockets(int s1, int s2);
extern struct sockaddr *getaddrbyany(const char *sp_name);
