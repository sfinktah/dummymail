#include "deliver_message.h"
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <errno.h>

#define UNKNOWN "Unknown"
#define DEBUG

extern int exists(char *filename);
extern int validmask(char *st, char *valid);
extern int validst(char *st);

// %a %b %d %H:%M:%S %Y

char *datestamp(time_t whattime)		/* Thu May 13 20:00:18 2004 */
{
        static char     buf[256];
        time_t          timex;
        struct tm       timeg;

        if (!whattime) timex = time(NULL);
        else timex = whattime;

        bcopy(localtime(&timex), &timeg, sizeof(timeg));

        strftime(buf, sizeof(buf), "%a %b %d %H:%M:%S %Y", &timeg);

        return(buf);
}

char *datestamp_rfc822(time_t whattime)		/* Thu, 13 May 2004 13:31:06 +1000 */
{
        static char     buf[256];
        time_t          timex;
        struct tm       timeg;

        if (!whattime) timex = time(NULL);
        else timex = whattime;

        bcopy(localtime(&timex), &timeg, sizeof(timeg));

        strftime(buf, sizeof(buf), "%a, %d %b %Y %H:%M:%S %z", &timeg);

        return(buf);
}


void deliver_message(struct smtp_state * state)
{
	static int unique_counter = 0;
	char filename[256] = {};
	FILE *fw;

	if (!state->szFrom || !state->szTo || !state->szMessage) {
		#ifdef DEBUG
			printf("No From/To/Message\n");
		#endif
		return;
	}

//	if (!*state->szFrom) {
//		/* No FROM information in envelope header, probably spam bounces clogging up our system. 
//		   Just silently ignore! */
//
//	   #ifdef DEBUG
//	       printf("Ignoring address with no FROM envelope header\n");
//	   #endif

//	   return;
//	}

	if (!state->szTo || strlen(state->szTo) > 64 || !validst(state->szTo)) {
	   #ifdef DEBUG
	       printf("Invalid TO address (length or illegal characters)\n");
	   #endif

	   return;
	}




	if (!strcmp(state->szTo, "i")) {
		#ifdef DEBUG
			printf("Spammers\n");
		#endif
		if (!(fw = fopen("SPAMMERS", "a"))) {
			// fclose(fw);
			return;
		}

		fprintf(fw, "%s\n", state->szRemoteIp);
		fclose(fw);
		return;
	}

	unique_counter ++;
	snprintf(filename, 255, "/var/spool/virtual/%s/%s", state->szHost, state->szUser);
#ifdef DEBUG
	printf("Final Delivery to: %s\n", filename);
#endif
	int perms = exists(filename);
	if (perms & 0111) {
		fw = popen(filename, "w");	
	} else {
		fw = fopen(filename, "a");
	}

	if (!fw) {
#ifdef DEBUG
        if (!fw) {
            printf("Couldn't open %s (perms: %o) errno: %i\n", filename, perms, errno);
        }
#endif
		return;
	}

#ifdef DEBUG
	printf("From: <%s> (%s)\n", state->szFrom, filename);
#endif

	fprintf(fw, "From %s %s\n", *state->szFrom ? state->szFrom : UNKNOWN, datestamp(time(NULL)));
	fprintf(fw, "Received: from %s (%s [%s])\n", state->szRemoteIp, state->szRemoteIp, state->szRemoteIp);
	fprintf(fw, " \tby localhost\n");
	fprintf(fw, " \tfor <%s>; %s\n", state->szTo, datestamp_rfc822(time(NULL)));	

	
	char *p;
	p = strstr(state->szMessage, "From: ");
	if (p && !strstr(state->szTo, "streetfx.com.au")) {
		// Normal Processing (not to streetfx)
		char *q;
		q = strchr(p, '\n');
		if (q) {
			*q = 0;
			*p = 0;
			fprintf(fw, "%s", state->szMessage);
			fprintf(fw, "X-Originally-From: %s\n", (p+6));
			fprintf(fw, "X-Envelope-From: <%s>\n", state->szFrom);
			fprintf(fw, "X-SpamTrak-Tag: %s\n", state->szTo);
			fprintf(fw, "X-SpamTrak-RemoteIp: %s\n", state->szRemoteIp);
#ifdef RBL
			fprintf(fw, "X-SpamTrak-RBL-Result: %d\n", state->nRblResult);
#endif
			fprintf(fw, "Reply-To: %s\n", (p+6));
			fprintf(fw, "From: \"[%s]\" <%s>", state->szTo, state->szTo);
			*q = '\n';
			fprintf(fw, "%s\n\n", q);
		} else {
			fprintf(fw, "%s\n\n", state->szMessage);
		}
	} else if (p) {
		// Street FX processing
		char *q;
		q = strchr(p, '\n');
		if (q) {
			*q = 0;
			*p = 0;
			fprintf(fw, "%s", state->szMessage);
			fprintf(fw, "X-Originally-From: %s\n", (p+6));
			fprintf(fw, "X-Envelope-From: <%s>\n", state->szFrom);
			fprintf(fw, "X-SpamTrak-Tag: %s\n", state->szTo);
			fprintf(fw, "X-SpamTrak-RemoteIp: %s\n", state->szRemoteIp);
#ifdef RBL
			fprintf(fw, "X-SpamTrak-RBL-Result: %d\n", state->nRblResult);
#endif
			fprintf(fw, "From: \"[%s]\" <%s>", state->szTo, state->szTo);
			*q = '\n';
			fprintf(fw, "%s\n\n", q);
		} else {
			fprintf(fw, "%s\n\n", state->szMessage);
		}
	} /* else {
		fprintf(fw, "%s\n\n", state->szMessage);
	} */

	if (perms & 0111) {
		pclose(fw);
	} else {
		fclose(fw);
	}
}
