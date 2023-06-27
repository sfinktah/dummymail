
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>

#include <pth.h>
#include "socket.h"
#include "smtp.h"
#include "test_common.h"
#include "deliver_message.h"
#ifdef RBL
#include "rbl.h"
#endif

#define MAX_LINE_LENGTH 1024
#define GREETING "220 mail Service Ready\r\n"
#define BUSY "450 Too many connections, please try again later\n"
#define DEFAULT_PORT 25
#define DEBUG
// #define MEGADEBUG
// #define MEGAMEGADEBUG
#define SMTP_PORT 25
#define GENERIC_PORT SMTP_PORT
#define GENERIC_HANDLER smtpd_handler
#define GENERIC_HANDLER_NAME "smtpd_handler"

#define GENERIC_HOST "mail.spamtrak.org"
// #ifndef GENERIC_HOST
//   #define GENERIC_HOST INADDR_ANY 
// #endif

#define MAX_THREADS FD_SETSIZE 
#define MAX_LINE 4096
#define TIMEOUT 300 /* seconds */
#define MAX_MESSAGE_SIZE (1 * 1024 * 1024)
#define INCOMING "/var/spool/virtual"

struct Address {
    char ip[INET6_ADDRSTRLEN];
    int  port;
};

static void *generic_accept(void *_arg);

void remove_square_brackets(char *str) {
    size_t len = strlen(str);
    if (len >= 2 && str[0] == '[' && str[len - 1] == ']') {
        memmove(str, str + 1, len - 2);
        str[len - 2] = '\0';
    }
}

int parse_conf_file(pth_attr_t attr, const char *filename) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        printf("Failed to open the configuration file %s.\n", filename);
        return 1;
    }

    char line[MAX_LINE_LENGTH];
    char *token;
    char *saveptr;

    while (fgets(line, MAX_LINE_LENGTH, file) != NULL) {
        // Remove leading/trailing whitespace and newlines
        // char *trimmed_line = strtok_r(line, " \t\n", &saveptr);
        char *trimmed_line = strtok_r(line, "\n", &saveptr);

        // Skip comment lines and empty lines
        if (trimmed_line == NULL || trimmed_line[0] == '#' || trimmed_line[0] == '\0') {
			  /*printf("Skipping configuration line: %s\n", line);*/
			  continue;
        }

        // Look for the 'Listen' directive
        if (strncmp(trimmed_line, "Listen", 6) == 0) {
			  /*printf("Parsing configuration line: %s\n", trimmed_line);*/
            // Extract the address and port
           token = strtok_r(trimmed_line, " \t\n", &saveptr);  // Skip the 'Listen' keyword
			  /*printf("token(1): %s\n", token);*/
           token = strtok_r(NULL, " \t\n", &saveptr);  // Get the address and port
			  /*printf("token(2): %s\n", token);*/
           if (token != NULL) {
                char *address = token;
                struct Address addr;
                strncpy(addr.ip, address, INET6_ADDRSTRLEN);
                addr.port = DEFAULT_PORT;

                // Check if port is specified
                char *port_delimiter = strrchr(token, ':');
                if (port_delimiter != NULL && port_delimiter > token && port_delimiter[-1] != ':') {
                    // Port is specified
                    *port_delimiter = '\0';  // Null terminate the address
                    char *port_str = port_delimiter + 1;
                    addr.port = atoi(port_str);
						  strncpy(addr.ip, address, INET6_ADDRSTRLEN);
                }

					 remove_square_brackets(addr.ip);

                printf("Address: %s, Port: %d\n", addr.ip, addr.port);
                pth_spawn(attr, generic_accept, (void*)&addr);
					 // Yield, because we're going to overwrite addr
					 pth_yield(NULL);
                // Extract the IP version (IPv4 or IPv6) and handle accordingly
            }
        }
		  else {
			  /*printf("Skipping (2) configuration line: %s\n", line);*/
		  }
    }

    fclose(file);
    return 0;
}

char* get_absolute_path(const char* relative_path) {
    char* absolute_path = malloc(MAX_LINE_LENGTH * sizeof(char));
    if (absolute_path == NULL) {
        perror("Memory allocation failed");
        return NULL;
    }

    if (realpath(relative_path, absolute_path) == NULL) {
        perror("Failed to get absolute path");
        free(absolute_path);
        return NULL;
    }

    return absolute_path;
}



int do_parse_conf_file(pth_attr_t attr, const char* argv0) {

    char executable_path[MAX_LINE_LENGTH];
    strncpy(executable_path, argv0, MAX_LINE_LENGTH);
    char *last_separator = strrchr(executable_path, '/');
    if (last_separator != NULL) {
        *(last_separator + 1) = '\0';  // Null terminate after the last separator
    } else {
		 strcpy(executable_path, "./");  // No directory path, use current directory
		 /* char cwd[MAX_LINE_LENGTH]; */
		 /* if (getcwd(cwd, sizeof(cwd)) != NULL) { */
			 /* printf("Current working directory: %s\n", cwd); */
		 /* } else { */
			 /* perror("getcwd() error"); */
			 /* return 1; */
		 /* } */
		 /* strncpy(executable_path, cwd, MAX_LINE_LENGTH); */
    }

    // Construct the path to the configuration file
    const char *conf_filename = "dummymail.conf";
    char conf_path[MAX_LINE_LENGTH];



    strncpy(conf_path, executable_path, MAX_LINE_LENGTH);
    strncat(conf_path, conf_filename, MAX_LINE_LENGTH - strlen(executable_path) - 1);

    char* absolute_path = get_absolute_path(conf_path);

    if (absolute_path != NULL) {
        printf("Absolute path: %s\n", absolute_path);
		  strncpy(conf_path, absolute_path, MAX_LINE_LENGTH);
        free(absolute_path);
    }

	 if (chdir(INCOMING)) {
		 perror("chdir");
		 exit(3);
	 }

    // Parse the configuration file
    parse_conf_file(attr, conf_path);

    return 0;
}


FILE *flog = NULL;
pth_mutex_t mutex = PTH_MUTEX_INIT;
int threads = 0;
int messages = 0;
int connections = 0;
time_t start_time;

int basic_init()
{
   unsigned long required = FD_SETSIZE;
   struct rlimit rlim;

   getrlimit(RLIMIT_NOFILE, &rlim);
   printf("Maximum filehandles was set to soft:%i hard:%i\n", (int)(rlim.rlim_max), (int)(rlim.rlim_cur));

   if (rlim.rlim_max < required)
      rlim.rlim_max = required;
   if (rlim.rlim_cur < required)
      rlim.rlim_cur = required;

   if (setrlimit(RLIMIT_NOFILE, &rlim) != 0)
      perror("setrlimit");

   getrlimit(RLIMIT_NOFILE, &rlim);
   printf("Now set to %i/%i\n", (int)(rlim.rlim_max), (int)rlim.rlim_cur);

   start_time = time(NULL);

#ifdef RBL
   printf("Resolving barracudanetworks...\n");
   rbl_init(NULL);                                                                                                                                                                                                                                    
#endif

   return 0;
}

static void *smtpd_handler(void *_arg)
{
   double load[3];
   pth_event_t timeout;
   struct smtp_state state;
   char caLine[MAX_LINE];
   char str[1024];
   char *Output = NULL;
   int fd = (int)((long)_arg);
   int n;
   int ret;
#ifdef RBL
   int rbl_result = 0;
#endif
   int len = sizeof(struct sockaddr_in);
   struct sockaddr_in peer;

   threads ++;

   if (getpeername(fd, (struct sockaddr *)&peer, &len) == 0) {
      state.szRemoteIp = strdup(inet_ntoa(peer.sin_addr));
#ifdef MEGADEBUG
      printf("[%s] Connection Received\n", inet_ntoa(peer.sin_addr));
#endif

   } else {
      /* Unlikely this setion will ever be reached */

      close(fd);
      threads --;
      return NULL;
   }

   connections ++;

   state.szFrom = 
      state.szTo =
      state.szData = 
      state.szUser =
      state.szHost =
      state.szMessage = NULL;
   state.nRblResult = 0;

   // state.szHost = strdup(inet_ntoa(peer.sin_addr));
   state.nState =  0;
   state.nMessageLen = 0;
   // state.nMemLen = 0;


   getloadavg(load, 10);
   if (load[1] > 5) {
      pth_send(fd, BUSY, strlen(BUSY), MSG_NOSIGNAL);
      close(fd);

      threads --;                                                                                                                                                
      return NULL;
   }

   printf("Connection Received: %16s %d\n", state.szRemoteIp, state.nRblResult);
#ifdef RBL
   state.nRblResult = rbl_check_ip(state.szRemoteIp);
   printf("RBL says: %i\n", state.nRblResult);
#endif

   pth_send(fd, GREETING, strlen(GREETING), MSG_NOSIGNAL);

   /* read request */
   for (;;) {

      /* Set timeout */

      // pth_yield(NULL);
      timeout = pth_event(PTH_EVENT_TIME, pth_timeout(TIMEOUT,0));
      n = pth_readline_ev(fd, caLine, MAX_LINE - 1, timeout);

      if (pth_event_status(timeout) == PTH_STATUS_OCCURRED) {
#ifdef MEGADEBUG
         printf("[%s] Timeout \n", state.szRemoteIp);
         if (n > 0 ) {
            caLine[n] = 0; 
            printf("[%s] Partial String '%s'\n", state.szRemoteIp, caLine);
         }
#endif
         break;
      }

      pth_event_free(timeout, PTH_FREE_ALL);

      if (n < 0) {
#ifdef MEGADEBUG
         printf("[%s] read error: errno=%d\n", state.szRemoteIp, errno);
#endif
         break;
      }
      if (n == 0) {
#ifdef MEGADEBUG
         printf("[%s] Remote Site Ended Transmission\n", state.szRemoteIp);
#endif
         break;
      }

      caLine[n] = 0;

#ifdef MEGADEBUG
#ifndef MEGAMEGADEBUG
      if (state.nState != SMTP_IN_DATA)
#endif
         printf("[%s] << %s", state.szRemoteIp, caLine);
#endif

      ret = SmtpProcessInput(&state, caLine);
      if (ret == -1) break;
      if (ret == 0) continue;
      // if (state.nState == SMTP_IN_DATA) continue;

      Output = SmtpStateChange(&state);
      if (Output) {
         n = pth_send(fd, Output, strlen(Output), MSG_NOSIGNAL);
      }

      if (state.nState == SMTP_QUIT) break;
      if (state.nState == SMTP_DOT) {
         messages ++;
         deliver_message(&state);
      }


      if (!Output) continue;
#ifdef MEGADEBUG
      printf("[%s] >> %s", state.szRemoteIp, Output);
#endif

      FREE(Output);

      if (n == 0) {
         printf("no bytes written\n");
         break;
      }
      if (n == -1) {
         perror("send()");
         break;
      }

   }

#ifdef MEGADEBUG
   printf("[%s] Ending Session\n", state.szRemoteIp);
#endif

   if (Output) FREE(Output); 
   if (state.szFrom) FREE(state.szFrom);
   if (state.szTo) FREE(state.szTo);
   if (state.szUser) FREE(state.szUser);
   // if (state.szHost) FREE(state.szHost);
   if (state.szMessage) FREE(state.szMessage);
   FREE(state.szRemoteIp);


   close(fd);
   threads --;
   return NULL;
}


int resolveAddressAndBind(const char *host, int *s, int iptype) {
   struct addrinfo hints, *res, *p;
   memset(&hints, 0, sizeof(hints));
   hints.ai_family = AF_UNSPEC;     // Allow IPv4 or IPv6
   hints.ai_socktype = SOCK_STREAM; // Use TCP
   hints.ai_flags = AI_PASSIVE;     // For binding

	int bind_count = 0;
   int status = getaddrinfo(host, NULL, &hints, &res);
   if (status != 0) {
      fprintf(stderr, "getaddrinfo error: %s: %s (IPv%i)\n", gai_strerror(status), host, iptype);
      return 1;
   }

   // Iterate over the address results
   for (p = res; p != NULL; p = p->ai_next) {
      char ipString[INET6_ADDRSTRLEN];
      void *addr;

      if (iptype == 4 && p->ai_family == AF_INET) {
         struct sockaddr_in *ipv4 = (struct sockaddr_in *)p->ai_addr;

         *s = socket(AF_INET, SOCK_STREAM, 0);
         if (*s == -1) {
            perror("socket (IPv4)");
            continue;
         }

         // Enable socket reuse
         int reuse = 1;
         if (setsockopt(*s, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) == -1) {
            perror("setsockopt (IPv4)");
            close(*s);
            continue;
         }

         ((struct sockaddr_in *)p->ai_addr)->sin_port = htons(GENERIC_PORT);

         addr = &(ipv4->sin_addr);

         inet_ntop(p->ai_family, addr, ipString, sizeof(ipString));
         printf("Resolved IP address: %s\n", ipString);

         if (bind(*s, p->ai_addr, p->ai_addrlen) == -1) {
            perror("bind (IPv4)");
            close(*s);
            continue;
         }
			++bind_count;
      } else if (iptype == 6 && p->ai_family == AF_INET6) {
         struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)p->ai_addr;

         *s = socket(AF_INET6, SOCK_STREAM, 0);
         if (*s == -1) {
            perror("socket (IPv6)");
            continue;
         }

         // Enable socket reuse
         int reuse = 1;
         if (setsockopt(*s, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) == -1) {
            perror("setsockopt (IPv6)");
            close(*s);
            continue;
         }

         ((struct sockaddr_in6 *)p->ai_addr)->sin6_port = htons(GENERIC_PORT);

         addr = &(ipv6->sin6_addr);

         inet_ntop(p->ai_family, addr, ipString, sizeof(ipString));
         printf("Resolved IP address: %s\n", ipString);

         if (bind(*s, p->ai_addr, p->ai_addrlen) == -1) {
            perror("bind (IPv6)");
            close(*s);
            continue;
         }
			++bind_count;
      }
   }

   freeaddrinfo(res);
   return bind_count;
}




static void *generic_accept(void *_arg)
{
   pth_attr_t attr;
   struct sockaddr chosenAddr;
   struct protoent *pe;
   int peer_len;
   int i;
   int sa;
   int period;
   int port;
   int one = 1;
   int t1, t2;
   pth_t tret;
   int sw;
   struct sockaddr_in peer_addr;
   // ---
   int iptype;
   struct Address addr;

   memcpy(&addr, _arg, sizeof(struct Address));

   if (strchr(addr.ip, ':') != NULL) {
      // IPv6 address
      // Handle IPv6 logic here
      iptype = 6;
   } else {
      // IPv4 address
      // Handle IPv4 logic here
      iptype = 4;
   }

   // ::/0
   // 0.0.0.0
   if (resolveAddressAndBind(addr.ip, &sa, iptype) == 0) {
      /*fprintf(stderr, "Failed to resolve addresses %s\n", addr.ip);*/
      return NULL;
   }

   if (iptype == 4 && listen(sa, 10) == -1) {
      perror("listen (IPv4)");
      close(sa);
      return NULL;
   }

   if (iptype == 6 && listen(sa, 10) == -1) {
      perror("listen (IPv6)");
      close(sa);
      return NULL;
   }

   // ---

   /*pe = getprotobyname("tcp");*/

   /*sa = socket(AF_INET, SOCK_STREAM, pe->p_proto);*/
   /*if (sa == -1) {*/
   /*perror("socket()");*/
   /*return NULL;*/
   /*}*/

   /*if (setsockopt(sa, SOL_SOCKET, SO_REUSEADDR, (void *)&one, sizeof(one)) == -1)*/
   /*{*/
   /*perror("setsockopt()");*/
   /*return NULL;*/
   /*}*/

   /*#ifndef GENERIC_HOST*/
   /*((struct sockaddr_in *)chosenAddr)->sin_family = AF_INET;*/
   /*((struct sockaddr_in *)chosenAddr)->sin_addr.s_addr = INADDR_ANY;*/
   /*#else*/
   /*#endif*/



   // ---
   /*printf("binding to port %d\n", GENERIC_PORT);*/
   /*if (bind(sa, chosenAddr, sizeof(struct sockaddr)) == -1)*/
   /*{*/
   /*perror("bind()");*/
   /*close(sa);*/
   /*return NULL;*/
   /*}*/

   if (0) {
      FILE *fw;
      fw = fopen("/var/spool/mail/__test__", "w");
      if (!fw) {
         perror("fopen");
         return NULL;
      }
      fclose(fw);
   }


   /*listen(sa, 10);*/

   attr = pth_attr_new();
   pth_attr_set(attr, PTH_ATTR_NAME, GENERIC_HANDLER_NAME);
   pth_attr_set(attr, PTH_ATTR_STACK_SIZE, 64 * 1024);
   pth_attr_set(attr, PTH_ATTR_JOINABLE, FALSE);
   period = 0;
   t1 = t2 = time(NULL);

   while (1) {
      t2 = time(NULL);
      if (t2 - t1 > 60) 
      {
         t1 = t2;
         printf("%d,%d,'threads: %d (pth: %ld) messages: %d connections: %d (%ld per minute)'\n", connections, messages, threads, pth_ctrl(PTH_CTRL_GETTHREADS), messages, connections, (60 * connections) / (t2 - start_time));
         fflush(stdout);
         // pth_ctrl(PTH_CTRL_DUMPSTATE, stdout);
      }

      peer_len = sizeof(peer_addr);

      sw = pth_accept(sa, (struct sockaddr *)&peer_addr, &peer_len);
      if (threads > MAX_THREADS) {
         (void)pth_send(sw, BUSY, strlen(BUSY), MSG_NOSIGNAL);
         close(sw);
      }
      else if (sw != -1)  {
         tret = pth_spawn(attr, GENERIC_HANDLER, (void *)(long long)sw);
         if (tret == NULL) {
            fprintf(stderr, "Couldn't spawn thread %s\n", GENERIC_HANDLER_NAME);
            close(sw);
         }
      }
#ifdef DEBUG
      else perror("pth_accept()");
#endif

   }
}

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

   /* Spawn the GENERIC server */

   attr = pth_attr_new();
   pth_attr_set(attr, PTH_ATTR_NAME, "generic_accept");
   pth_attr_set(attr, PTH_ATTR_JOINABLE, FALSE);
   int four = 4;
   int six  = 6;
   do_parse_conf_file(attr, argv[0]);
   /*pth_spawn(attr, generic_accept, (void*)&four);*/
   /*pth_spawn(attr, generic_accept, (void*)&six);*/

   pth_yield(NULL);
   if (setgid(8)) perror("setgid");
   if (setuid(8)) perror("setuid");
#ifndef DEBUG
   daemon(1,0);
#endif

   /*
      attr = pth_attr_new();
      pth_attr_set(attr, PTH_ATTR_NAME, "smtp_test");
      pth_spawn(attr, smtp_test, NULL);
    */
   for (;;) {

      /* Sleep for 1/4 of a second, and queue up lots of socket data (and save our precious CPU!) */
      /* This usleep ties up all threads of execution.  It sounds stupid, but hybenating like this */
      /* stop massive CPU usage when we're loaded. */

      // usleep(250000);   

      sleep(1);
      pth_yield(NULL);

      /* Just incase there is no activity, we'll have a much longer nap (but this one doesn't affect */
      /* the other threads.  It's just a way for us to give any socket data a chance to be procesed. */

      // pth_sleep(15);

      // pth_yield(NULL);   /* The pth_sleep already yields */
#ifdef MEGADEBUG
      printf(".");
      fflush(stdout);
#endif
   }
}
/* vim: set ts=3 sts=0 sw=3 noet: */
