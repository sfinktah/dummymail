#include        <stdio.h>
#include        <stdlib.h>
#include        <unistd.h>

#include        <netdb.h>
#include        <netinet/in.h>
#include        <sys/types.h>
#include        <sys/socket.h>
#include        <stdio.h>
#include        <time.h>
#include        <signal.h>
#include        <pthread.h>
#include        <string.h>
#include        <errno.h>

#include <pth.h>
#include "test_common.h"
#include "geturl.h"
#include "socket.h"

#define PORT 80
#define HTTPD_PORT 1081
#define BACKLOG 100		// listen() backlog
#define REAL_PORT 80
#define CONNECT_TIMEOUT 0
#define MAX_CONNECT_ATTEMPTS 10	// Try 10 different socks proxies before giving up
#define PACKET_BUFFER_SIZE 2048
#define IDLE_TIMEOUT 90
#define PROXY_URL "http://eggpudding.com/m3/socks2.txt"
#define MAX_PROXIES 100
#define MAX_OFFENDERS 100
#undef DUMP_OUTPUT

struct offenderstruct {
    unsigned long ip;
    int count;
    FILE *flog;
    char *spam;
};

struct proxystruct {
    char ip[16];
    int port;
};

/* Global Variables */

struct proxystruct *proxies;
int no_proxies;
int last_proxy_used;

struct offenderstruct *offenders;
int no_offenders;
int lastoff;

FILE *flog;

char *get_proxy();
void add_offender(unsigned long ip, char *spam);


/* the socket connection handler thread */
static void *handler(void *_arg)
{
    int fr;
    struct sockaddr_in here, there;

    int nret;
    int fd = (int) _arg;
    int attempt;
    time_t now;
    char *proxy;
    char *ct;
/*
    now = time(NULL);
    ct = ctime(&now);
    pth_write(fd, ct, strlen(ct));
*/

    struct sockaddr_in peer;
    int len = sizeof(struct sockaddr_in);

    if (getpeername(fd, (struct sockaddr *) &peer, &len) == 0) {
	printf("Connection received from %s\n", inet_ntoa(peer.sin_addr));
	add_offender(peer.sin_addr.s_addr, NULL);
    }

    for (attempt = 0; attempt < MAX_CONNECT_ATTEMPTS; attempt++) {

	if (!(proxy = get_proxy())) {
	    puts("No proxies!");
	    return NULL;
	}

	proxy = "192.168.1.1";

	printf("Attemping to connect to %s:%d\n", proxy, REAL_PORT);
	if ((fr = connect_socket(proxy, REAL_PORT, NULL)) == -1) {
	    printf("Connection failed\n");
	} else {
	    printf("Connection made to %s:%d\n", proxy, REAL_PORT);
	    if (link_sockets(fd, fr) == -1) {
#ifdef DEBUG
		printf("link_sockets() failed\n");
#endif
	    }
	    break;
	}
    }

    close(fd);
    return NULL;
}

/* the stderr time ticker thread */
static void *ticker(void *_arg)
{
    time_t now;
    char *ct;
    float load;

    for (;;) {
	pth_sleep(60);
	now = time(NULL);
	ct = ctime(&now);
	ct[strlen(ct) - 1] = '\0';
	pth_ctrl(PTH_CTRL_GETAVLOAD, &load);
	printf("ticker: time: %s, average load: %.2f\n", ct, load);
	get_proxies();
    }
}

void add_proxy(char *proxy, int port)
{
    int i;

    for (i = 0; i < MAX_PROXIES; i++) {
	if (!proxies[i].port) {
	    proxies[i].port = port;
	    strncpy(proxies[i].ip, proxy, 15);
	    proxies[i].ip[15] = 0;
	    break;
	}
    }

    // puts(proxy);
}

void add_offender(unsigned long ip, char *spam)
{
    int i;

    for (i = 0; i < MAX_OFFENDERS; i++) {
	if (offenders[i].ip == ip) {
	    if (offenders[i].spam)
		free(offenders[i].spam);
	    offenders[i].spam = spam ? strdup(spam) : NULL;
	    offenders[i].count++;

	    return;
	}
    }

    offenders[lastoff].ip = ip;
    offenders[lastoff].spam = spam ? strdup(spam) : NULL;


    if (++lastoff > MAX_OFFENDERS)
	lastoff = 0;

    /* Clear record */

    if (offenders[lastoff].ip) {
	offenders[lastoff].ip = offenders[lastoff].count = 0;
	if (offenders[lastoff].spam)
	    free(offenders[lastoff].spam);
    }

}

#define MAX_OUTPUT 4096
#define MAX_LINE 256
char *get_httpd_data(char *input)
{
    int i;
    char *output;
    char line[MAX_LINE];
    struct in_addr in;

    memset(line, 0, MAX_LINE);
    output = calloc(MAX_OUTPUT, 1);
    if (!output) {
	perror("calloc()");
	return NULL;
    }

    printf("Input: %s\n", input);

    strcat(output,
	   "<html>\n<body bgcolor=white text=black link=black vlink=grey alink=grey>\n");
    strcat(output,
	   "<font face=Arial><font size=+3><center>m3 High Performance Socks Proxy<br>\n");
    strcat(output,
	   "</font><font size=+1><b><i>Utilisation Data</i></b></font><br><br>\n");

    for (i = 0; i < MAX_OFFENDERS; i++) {
	if (offenders[i].ip) {
	    memcpy(&in, &(offenders[i].ip), 4);
	    snprintf(line, MAX_LINE - 1,
		     "<xa href='%s'><b>%s</b></a>&nbsp;&nbsp;(%d connects)<br>\n",
		     inet_ntoa(in), inet_ntoa(in), offenders[i].count);
	    strcat(output, line);
	    if (strlen(output) > MAX_OUTPUT - MAX_LINE)
		break;
	}
    }

    return output;

}

char *get_proxy()
{
    if (++last_proxy_used >= no_proxies)
	last_proxy_used = 0;

    return proxies[last_proxy_used].ip;
}

int get_proxies()
{
    char *proxy_page;
    char *proxy_ptr;
    char *p;
    char *tok, *tokptr;
    int proxies;

    last_proxy_used = 0;
    no_proxies = 0;

    proxy_ptr = proxy_page = get_url(PROXY_URL);
    if (!proxy_page) {
	printf("couldn't get proxy list\n");
	return -1;
    }

    tok = strtok_r(proxy_page, "\n", &tokptr);
    while (tok) {
	while (*tok == 13 || *tok == 10)
	    tok++;

	if ((p = strstr(tok, ":80"))) {
	    *p = 0;
	    add_proxy(tok, 80);
	    no_proxies++;
	}

	tok = strtok_r(NULL, "\n", &tokptr);
    }

    free(proxy_page);
    return 0;

}

int basic_init()
{
    printf("Getting proxies...\n");

    if ((proxies = calloc(MAX_PROXIES, sizeof(struct proxystruct))) == 0) {
	perror("malloc()");
	return -1;
    }

    if ((offenders = calloc(MAX_OFFENDERS, sizeof(struct offenderstruct)))
	== 0) {
	perror("malloc()");
	return -1;
    }

    if (get_proxies() == -1) {
	printf("Couldn't get proxies, terminating.\n");
	return -1;
    }

    srand(time(0));

    flog = fopen("stoxy.log", "a");
    if (!flog) {
	printf("Couldn't open log file for writing\n");
	return -1;
    }

    return 0;
}

#define MAXREQLINE 1024
static void *httpd(void *_arg)
{
    int fd = (int) ((long) _arg);
    char caLine[MAXREQLINE];
    char str[5192];
    char *httpd_data;
    char *p;
    int n;


    httpd_data = NULL;

    /* read request */
    for (;;) {
	n = pth_readline(fd, caLine, MAXREQLINE);
	if (n < 0) {
	    fprintf(stderr, "read error: errno=%d\n", errno);
	    close(fd);
	    return NULL;
	}
	if (n == 0)
	    break;
	if (n == 1 && caLine[0] == '\n')
	    break;
	caLine[n - 1] = NUL;

	if (!httpd_data && (p = strstr(caLine, "GET ")))
	    httpd_data = get_httpd_data(p + 4);
    }

    /* simulate a little bit of processing ;) */
    pth_yield(NULL);

    /* generate response */
    if (httpd_data) {
	sprintf(str, "HTTP/1.0 200 Ok\r\n"
		"Server: st0xy b0.0b\r\n"
		"Connection: close\r\n"
		"Content-type: text/plain\r\n" "\r\n%s\r\n", httpd_data);

	free(httpd_data);
	pth_write(fd, str, strlen(str));
    }

    /* close connection and let thread die */
    fprintf(stderr, "connection shutdown (fd: %d)\n", fd);
    close(fd);
    return NULL;
}

static void *httpd_accept(void *_arg)
{

    pth_attr_t attr;
    struct sockaddr_in sar;
    struct protoent *pe;
    struct sockaddr_in peer_addr;
    int peer_len;
    int sa, sw;
    int port;
    int one = 1;

    pe = getprotobyname("tcp");

    sa = socket(AF_INET, SOCK_STREAM, pe->p_proto);
    if (sa == -1) {
	perror("socket()");
	return NULL;
    }

    if (setsockopt
	(sa, SOL_SOCKET, SO_REUSEADDR, (void *) &one, sizeof(one)) == -1) {
	perror("setsockopt()");
	return NULL;
    }

    sar.sin_family = AF_INET;
    sar.sin_addr.s_addr = INADDR_ANY;
    sar.sin_port = htons(HTTPD_PORT);

    printf("Binding to port %d\n", HTTPD_PORT);
    if (bind(sa, (struct sockaddr *) &sar, sizeof(struct sockaddr_in)) ==
	-1) {
	perror("bind()");
	return NULL;
    }

    listen(sa, 10);

    attr = pth_attr_new();
    pth_attr_set(attr, PTH_ATTR_NAME, "httpd");

    for (;;) {
	peer_len = sizeof(peer_addr);
	sw = pth_accept(sa, (struct sockaddr *) &peer_addr, &peer_len);
	pth_spawn(attr, httpd, (void *) sw);
    }
}

/* the main thread/procedure */
int main(int argc, char *argv[])
{
    pth_attr_t attr;
    struct sockaddr_in sar;
    struct protoent *pe;
    struct sockaddr_in peer_addr;
    int peer_len;
    int sa, sw;
    int port;
    int one = 1;

    if (basic_init() == -1)
	return -1;

    /* PTH STARTS HERE */

    pth_init();
    signal(SIGPIPE, SIG_IGN);

    /* Spawn the ticker */

    attr = pth_attr_new();
    pth_attr_set(attr, PTH_ATTR_NAME, "ticker");
    pth_attr_set(attr, PTH_ATTR_STACK_SIZE, 64 * 1024);
    pth_attr_set(attr, PTH_ATTR_JOINABLE, FALSE);
    pth_spawn(attr, ticker, NULL);

    /* Spawn the HTTPD server */

    pth_attr_set(attr, PTH_ATTR_NAME, "httpd_accept");
    pth_spawn(attr, httpd_accept, NULL);

    pe = getprotobyname("tcp");

    sa = socket(AF_INET, SOCK_STREAM, pe->p_proto);
    if (sa == -1) {
	perror("socket()");
	return -1;
    }

    if (setsockopt
	(sa, SOL_SOCKET, SO_REUSEADDR, (void *) &one, sizeof(one)) == -1) {
	perror("setsockopt()");
	return -1;
    }

    sar.sin_family = AF_INET;
    sar.sin_addr.s_addr = INADDR_ANY;
    sar.sin_port = htons(PORT);

    printf("Binding to port %d\n", PORT);
    if (bind(sa, (struct sockaddr *) &sar, sizeof(struct sockaddr_in)) ==
	-1) {
	perror("bind()");
	return -1;
    }

    listen(sa, BACKLOG);

    pth_attr_set(attr, PTH_ATTR_NAME, "handler");
    for (;;) {
	peer_len = sizeof(peer_addr);
	sw = pth_accept(sa, (struct sockaddr *) &peer_addr, &peer_len);
	pth_spawn(attr, handler, (void *) sw);
    }
}
