#include "pth_compat.h"
#include "geturl.h"

#undef DEBUG

char* _get_url(char* url, char *proxy)
{
	char *host, *document, *postdata, *page, *pageptr;
	char *p, *q;
	int sock;
	int data;
	int memsize;
	int read;

	if (!strncmp(url, "http://", 7))
		url += 7;

	q = strchr(url, '/');
	if (!q || !(document = strdup(q)))
		return NULL;

	if (!(host = strdup(url))) {
		free(document);
		return NULL;
	}

	if (!(q = strchr(host, '/'))) {
		free(document);
		free(host);
		return NULL;
	}

	*q = 0;

	if (!(postdata = (char *)malloc((strlen(url) + 256 + 128) & 0xffffff00))) {
		free(document);
		free(host);
		return NULL;
	}

	if (proxy) 
		sprintf(postdata, "GET http://%s%s HTTP/1.0\r\n\r\n", host, document);
	else
		sprintf(postdata, "GET %s HTTP/1.0\r\nHost: %s\r\n\r\n",document, host);

#ifdef DEBUG
	puts(postdata);
#endif

	free(document);

	if (!proxy)
		sock = connect_socket(host, 80, NULL);
	else
		sock = connect_socket(proxy, 80, NULL);

	if (sock == -1) {
		free(postdata);
		return NULL;
	}

	if (pth_send(sock, postdata, strlen(postdata), MSG_NOSIGNAL) < strlen(postdata)) {
		free(postdata);
		free(host);
		return NULL;
	}

	free(host);
	free(postdata);

	memsize = 2048;
	pageptr = page = (char *)malloc(memsize);

	if (!page) {
		return NULL;
	}

	do {
		read = pth_recv(sock, pageptr, 2048, MSG_NOSIGNAL);
		if (read > 0) {
			pageptr += read;
			memsize += read;
			page = (char *)realloc(page, memsize);
		}

	} while (page && read > 0);

	if (!page) {
		return NULL;
	}

	*pageptr = 0;
#ifdef DEBUG
	 puts(page);
#endif

	return page;
}

char* get_url(char* url)
{
	_get_url(url, NULL);
}

char* get_url_proxied(char* url, char *proxy)
{
	_get_url(url, proxy);
}
