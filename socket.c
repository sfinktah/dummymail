#include "socket.h"
#include "pth_compat.h"

#ifdef FLOG
extern FILE *flog;
#endif

struct sockaddr *getaddrbyany(const char *sp_name)
{
	/*
	   struct hostent *sp_he;
	   int i, len;

	   len = strlen(sp_name);

	   if (len < 7)
	   return -1;

	   for (i=0; i<len; i++) {
	   if (isalpha(sp_name[i])) {
	   for (sp_he=(void *)i=0; !sp_he&&i<3; i++) {
	   sp_he = gethostbyname(sp_name);
	   }

	   return sp_he ? *(long *) *sp_he->h_addr_list : -1;
	   }
	   }
	   return inet_addr(sp_name);

	 */


	struct addrinfo *result;
	char hostname[NI_MAXHOST];
	int error;

	if (error = getaddrinfo(sp_name, NULL, NULL, &result))
	{
		fprintf(stderr, "error using getaddrinfo on '%s': %s\n", sp_name, gai_strerror(error));
		return NULL;
	}

	if (result)
	{
		return(result->ai_addr);

		// Not reached
		if (error = getnameinfo(result->ai_addr, sizeof(struct sockaddr), hostname, sizeof(hostname), NULL,0,0))
		{
			fprintf(stderr, "error using getnameinfo on %s: %s\n", sp_name, gai_strerror(error));
		}
	}
}

int connect_socket(const char *host, int port, const char *source)
{
	int sock;
	int one = 1;
	struct sockaddr_in here, there, *socktmp;

	memset(&here, 0, sizeof(here));
	memset(&there, 0, sizeof(there));

	if (source) {
		memcpy(&here, getaddrbyany(source), sizeof(here));
	} else {
		here.sin_addr.s_addr = INADDR_ANY;
		here.sin_family = AF_INET;
	}

	here.sin_port = 0;

	socktmp = (struct sockaddr_in *)getaddrbyany(host);
	if (socktmp == NULL)	/* This will block if it's not a dotted quad */
	{
		printf("Couldn't resolve %s\n", host);
		return -1;
	}

	there.sin_family = AF_INET;
	memcpy(&there, socktmp, sizeof(there));
	there.sin_port = htons(port);

	if ((sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
		perror("socket");
		sleep(CONNECT_TIMEOUT);
		return (-1);
	}
	/*
	   if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (void *)&one, sizeof(one)) == -1)
	   {
	   perror("setsockopt()");
	   return -1;
	   }
	 */
	if (bind(sock, (struct sockaddr *) &here, sizeof(here)) == -1) {
		perror("bind");
		sleep(CONNECT_TIMEOUT);
		close(sock);
		return (-1);
	}

	if (pth_connect(sock, (struct sockaddr *) &there, sizeof(there)) == -1) {
		perror("connect");
		sleep(CONNECT_TIMEOUT);
		close(sock);
		return (-1);
	}

	return sock;
}

int xfer_data(int s1, int s2)
{
	char *buffer;
	int read, written;

#ifdef DEBUG
	printf("xfer %d->%d\n", s1, s2);
#endif

	if (!(buffer = (char *)malloc(PACKET_BUFFER_SIZE))) {
		perror("malloc()");
		return -1;
	}

	read = pth_recv(s1, buffer, PACKET_BUFFER_SIZE, MSG_NOSIGNAL);
	if (read == 0) {
#ifdef DEBUG
		printf("socket closed\n");
#endif
		free(buffer);
		return -1;
	}
	if (read == -1) {
		perror("recv()");
		free(buffer);
		return -1;
	} 

	written = pth_send(s2, buffer, read, MSG_NOSIGNAL);

	if (written == 0) {
		printf("no bytes written\n");
		free(buffer);
		return -1;
	} 
	if (written == -1) {
		perror("send()");
		free(buffer);
		return -1;
	}
	if (written != read) {
		printf("read %d only wrote %d\n", read, written);
		free(buffer);
		return -1;
	}

#ifdef DEBUG
	printf("wrote %d bytes\n", written);
#endif

	if (written < PACKET_BUFFER_SIZE) {
		buffer[written] = 0;
#ifdef DUMP_OUTPUT
		if (!strncmp(buffer, "RCPT TO:", 8))
			printf("%s", buffer);
		else {
			printf("%s", buffer);
		}
#endif

#ifdef FLOG
		if (flog) 
			fputs(buffer, flog);
#endif
	}



	free(buffer);
	return written;
}

int link_sockets(int s1, int s2)
{
	fd_set readset, writeset, errorset;
	struct timeval tv;
	int result;


	for (;;) {
		FD_ZERO(&readset);
		FD_ZERO(&writeset);

		FD_SET(s1, &readset);
		FD_SET(s1, &writeset);
		FD_SET(s1, &errorset);

		FD_SET(s2, &readset);
		FD_SET(s2, &writeset);
		FD_SET(s2, &errorset);

		tv.tv_sec = IDLE_TIMEOUT;
		tv.tv_usec = 0;

		result = pth_select((s1>s2?s1:s2)+1, &readset, /* writeset, errorset, */ NULL, NULL, &tv);

		if (!result) {
			printf("timeout in link_sockets()\n");
			close(s1);
			close(s2);
			return 0;
		}

		if (FD_ISSET(s1, &readset)) {
			if (xfer_data(s1, s2) == -1) {
				close(s1);
				close(s2);
				return -1;
			}
		} /* else */
		if (FD_ISSET(s2, &readset)) {
			if (xfer_data(s2, s1) == -1) {
				close(s1);
				close(s2);
				return -1;
			}
		}
	}

}

