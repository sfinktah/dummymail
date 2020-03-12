/* SMTPD.c 
	Core Proccesing Routines for SMTP compliant state based mail server 
	Copyright 2003 Cyrus Lesser and InterTEN Networking.
*/

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>

#include "smtp.h"
#include "snprintf.h"


#define DEBUG
#define MACHINE_NAME "mail"
#define MAX_MESSAGE_SIZE (1024 * 1024)


/* Internal Definitions. Do not change */

#define STATE state->nState
#define DATA state->szData
// #define VIRET(y,x) {*Data = Input + x; while (*Data[0] == 32) (*Data)++; return y;}
#define VIRET(y,x) {*Data = Input + x; return y;}

/* End Internal Definitions. */




int exists(char *filename) {
	struct stat statbuf;

// Description of stat field /*{{{*/
/*
           struct stat {
               dev_t     st_dev;     / ID of device containing file /
               ino_t     st_ino;     / inode number /
               mode_t    st_mode;    / protection /
               nlink_t   st_nlink;   / number of hard links /
               uid_t     st_uid;     / user ID of owner /
               gid_t     st_gid;     / group ID of owner /
               dev_t     st_rdev;    / device ID (if special file) /
               off_t     st_size;    / total size, in bytes /
               blksize_t st_blksize; / blocksize for file system I/O /
               blkcnt_t  st_blocks;  / number of blocks allocated /
               time_t    st_atime;   / time of last access /
               time_t    st_mtime;   / time of last modification /
               time_t    st_ctime;   / time of last status change /
           };

 The following POSIX macros are defined to check the file type using the st_mode field:

           S_ISREG(m)  is it a regular file?

           S_ISDIR(m)  directory?

           S_ISCHR(m)  character device?

           S_ISBLK(m)  block device?

           S_ISFIFO(m) FIFO (named pipe)?

           S_ISLNK(m)  symbolic link? (Not in POSIX.1-1996.)

           S_ISSOCK(m) socket? (Not in POSIX.1-1996.)

       The following flags are defined for the st_mode field:

           S_IFMT     0170000   bit mask for the file type bit fields
           S_IFSOCK   0140000   socket
           S_IFLNK    0120000   symbolic link
           S_IFREG    0100000   regular file
           S_IFBLK    0060000   block device
           S_IFDIR    0040000   directory
           S_IFCHR    0020000   character device
           S_IFIFO    0010000   FIFO
           S_ISUID    0004000   set UID bit
           S_ISGID    0002000   set-group-ID bit (see below)
           S_ISVTX    0001000   sticky bit (see below)
           S_IRWXU    00700     mask for file owner permissions
           S_IRUSR    00400     owner has read permission
           S_IWUSR    00200     owner has write permission
           S_IXUSR    00100     owner has execute permission
           S_IRWXG    00070     mask for group permissions
           S_IRGRP    00040     group has read permission
           S_IWGRP    00020     group has write permission
           S_IXGRP    00010     group has execute permission
           S_IRWXO    00007     mask for permissions for others (not in group)
           S_IROTH    00004     others have read permission
           S_IWOTH    00002     others have write permission
           S_IXOTH    00001     others have execute permission
*//*}}}*/

	int r = stat(filename, &statbuf);
	return (r == 0) ? statbuf.st_mode : 0;
}


/* ValidMask: Checks that all characters inside a string match a mask */
/* Returns: 1 on valid
   0 on invalid
   */
int validmask(char *st, char *valid)
{
	int len, x;
	char ch;

	len = strlen(st);
	for (x = 0; x < len; x++)
	{
		if (x == len) break;  // Don't know why this was needed, but it was!
		ch = st[x];
		if (!strchr(valid, ch))
		{
			return 0;

		}
	}

	return 1;
}

/* Valid String: Determine whether a string comprises of a valid email address */
/* Returns: 1 on valid
   0 on invalid
   */

int validst(char *st)
{
#define VALIDST "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz1234567890.-_@+"
	return validmask(st, VALIDST);
}


#include <string.h>

inline char mytolower(char p) {
	if (p >= 'A' && p <= 'Z') p ^= 0x20;
		return p;
}

int spc_email_isvalid(char *address, char *user, char *host) {
	int         count = 0, user_count = 0;
	char        *c, *domain;
	int         file_found = 0;
	char        vdir[96];
	static char *rfc822_specials = "()<>@,;:\\\"[]";
	FILE        *exist_file;

	/* first we validate the name portion (name@domain) */
	for (c = address;  *c;  c++) {
		*c = tolower(*c);
		if (*c == '\"' && (c == address || *(c - 1) == '.' || *(c - 1) == 
					'\"')) {
			while (*++c) {
				if (*c == '\"') break;
				if (*c == '\\' && (*++c == ' ')) continue;
				if (*c <= ' ' || *c >= 127) return 0;
			}

			if (!*c++) return 0;
			if (*c == '@') {
				// *c = 0;
				break;
			}
			user_count ++;

			if (*c != '.') return 0;
			continue;
		}
		if (*c == '@') {
			*c = 0;
			break;
		}
		if (*c <= ' ' || *c >= 127) return 0;
		if (strchr(rfc822_specials, *c)) return 0;

	}
	if (c == address || *(c - 1) == '.') return 0;


	/* next we validate the domain portion (name@domain) */
	if (!*(domain = ++c)) return 0;
	{
		do {
			*c = tolower(*c);
			if (*c == '.') {
				// If the domain name starts with or has two periods (.) then it's wrong.
				if (c == domain || *(c - 1) == '.') return 0;
				count++;
			}
			// Check each character against allowables
			if (*c <= ' ' || *c >= 127) return 0;
			if (strchr(rfc822_specials, *c)) return 0;
		} while (*++c);

		if (count < 1) return 0;
	}


	// We should be in the working directory /var/spool/virtual/

	// Override x.y.z.spamtrak.org to be spamtrak.org
	puts(domain);
	char *p = strstr(domain, "spamtrak.org");
	if (p != NULL) {
		// But only if there isn't already a directory that matches (so we can bonuce .x.y.z.spamtrka.org
		printf("Checking directory: %s - %s\n", domain, exists(domain) ? "exists" : "doesn't exist");
		if (!exists(domain)) {
			domain = p;
		}
	}


	/*
	char *q = strstr(domain, "streetfx.com.au");
	if ((q) != NULL) {
		domain = q;
	}
	*/

	/* Check for ondisk file that matches */
	if (strstr(address, "master") ||
			strstr(address, "abuse")) 
	{
		return 1;
	}

	// File-Found will be false from init at this point
	if (!file_found) {
		snprintf(vdir, 96, "/var/spool/virtual/%s/%s", domain, address);

		puts(vdir);
		if (exists(vdir)) {
			file_found = 1;
		} else
		{
			snprintf(vdir, 96, "/var/spool/virtual/%s/%%", domain);	/* Wildcard domain? */
			puts(vdir);

//           S_ISREG(m)  is it a regular file?
//
//           S_ISDIR(m)  directory?
//
//           S_ISCHR(m)  character device?
//
//           S_ISBLK(m)  block device?
//
//           S_ISFIFO(m) FIFO (named pipe)?
//
//           S_ISLNK(m)  symbolic link? (Not in POSIX.1-1996.)
//
//           S_ISSOCK(m) socket? (Not in POSIX.1-1996.)
// if (S_ISREG(exists(vdir))) ... 

			if (exists(vdir)) {
				file_found = 1; 
			}
		} 
	}

	strcpy(user, address);
	strcpy(host, domain);
	address[strlen(address)] = '@';

	return (file_found ? 1 : -1);
}


/* Validate Input: Process a line of SMTP conversation */
/* Returns: New current stage of SMTP conversation */
/*          or -1 on error. */

int ValidateInput(struct smtp_state * state, char *Input, char **Data)
{
	int i, len;

	/* Only take one line. */

	len = strlen(Input);

	for (i=0; i<len; i++)
		if (Input[i] == 10 || Input[i] == 13) {
#ifdef TABDEBUG
			printf("Truncated line at position %i of %i\n", i, len);
#endif
			Input[i] = 0;
			break;
		}

	/* Check from termination message data via '.' */
	if (STATE == SMTP_IN_DATA || STATE == SMTP_DATA) {
		if (!strncasecmp(Input, ".", 1) && strlen(Input) == 1)  {
			VIRET(SMTP_DOT, 1);
		} 
		
//		else if (state && state->szMessage && strlen(state->szMessage) > MAX_MESSAGE_SIZE) {		/* Very slow comparison due to strlen(), should change to size counter */
//			VIRET(SMTP_DOT, -1);
//		}
		else VIRET(SMTP_IN_DATA, 0);
	} else {
		/* Check for standard [minimal] SMTP commands */
		if (!strncasecmp(Input, "EHLO", 4)) 				VIRET(SMTP_HELO, 4)	  /* Pretend to be ESTMP compliant - as if */
		if (!strncasecmp(Input, "HELO", 4)) 				VIRET(SMTP_HELO, 4)
		else if (!strncasecmp(Input, "VRFY ", 5)) 			VIRET(SMTP_VRFY, 5)
		else if (!strncasecmp(Input, "MAIL FROM:", 10)) 	VIRET(SMTP_MAIL_FROM, 10)
		else if (!strncasecmp(Input, "RCPT TO:", 8)) 		VIRET(SMTP_RCPT_TO, 8)
		else if (!strncasecmp(Input, "DATA", 4)) 			VIRET(SMTP_DATA, 4)
		else if (!strncasecmp(Input, "RSET", 4)) 			VIRET(SMTP_RSET, 4)
		else if (!strncasecmp(Input, "QUIT", 4)) 			VIRET(SMTP_QUIT, 4)

		else VIRET(SMTP_UNKNOWN, 0);
	}

	return -1;
}


/* Strip Email: Return email address contained inside '<' and '>' */
/* Returns: New pointer to NULL terminated email address, or original pointer on error */
/* WARNING: DESTROYS INPUT BUFFER */
char *strip_email(char *in)			
{
	char *lt, *gt, *i;
	char *p;

	printf("strip_email('%s')                \n", in);

	lt = strrchr(in, '<');
	if (!lt) goto trim;

	gt = strchr(lt, '>');
	if (!gt) goto trim;

	*gt = 0;

	p = lt + 1;
	while (*p) {
		*p = tolower(*p);
		p ++;
	}

	return lt + 1;

trim:
	p = in;

	while (*p && *p == ' ') p ++;

	lt = p;
	while (*p) {
		*p = tolower(*p);
		p ++;
	}

	return lt;
}


/* Smtp State Change: Issue Daemon Response Message based on new current state */
/* Return: NULL terminated response to send to socket */
/* 			or NULL pointer on error */
char *SmtpStateChange(struct smtp_state * state)
{
#define OUTPUT_SIZE 1500
#define SMTP_HELO_DOMAIN "mail.com"
#define SSC_ERROR return FREE(Output), NULL

	char *Output = NULL;
	char *ExtraData;
	char *szTmp, szTmp2, *szUser, *szHost;
	int i;

	switch (state->nState) {

		case SMTP_HELO:
			asprintf(&Output, "250 %s Pleased to meet you %s\r\n", MACHINE_NAME, DATA); 
			break;

		case SMTP_RSET:

			if (state->szFrom) FREE(state->szFrom);
			if (state->szTo) FREE(state->szTo);
			if (state->szMessage) FREE(state->szMessage);

			asprintf(&Output, "250 2.0.0 Reset state\r\n");

			break;

			/* Automatically terminate bounce email messages (anti-RFC behaviour) */
			/* (This is to reduce load from bounced UCE with forged SMTP FROM fields */

		case SMTP_MAIL_FROM:
			if (state->szFrom) FREE(state->szFrom);
			state->szFrom = strdup(strip_email(DATA));

			/*
			   if (!*state->szFrom || 
			   !strncmp(state->szFrom, "postmaster@", 11) ||
			   !strncmp(state->szFrom, "mailer-daemon@", 14) ||
			   !strncmp(state->szFrom, "mdaemon@", 8)
			   )
			   asprintf(&Output, "553 5.0.0 Invalid address (UCE Bounce Protection)\r\n");
			   else
			   */
			asprintf(&Output, "250 2.1.0 Sender ok\r\n");

			break;


		case SMTP_RCPT_TO:
			/* DEBUG: Perform random User Unknown reply for load testing 
			   if (rand() & 0x2)
			   asprintf(&Output, "550 5.1.1 User unknown\r\n");
			   else
			   */

			/* Only accept single recipient */
			/* (This is to reduce incidents of spam. Hopefully sending mail server will be smart enough */
			/*  to cope if this was a valid attempt at CC/BCC */
			if (state->szTo) {
				asprintf(&Output, "550 5.1.2 Too Many Recipients\r\n");
				break;
			}

			szTmp = strdup(strip_email(DATA));
			szUser = strdup(szTmp);	// lazy malloc()
			// szHost = strdup(szTmp);	// Maybe this will fix crashed 24/12/08 // removed again 24th

			char *p;
			p=strchr(szUser, '@');
			if (p) {
				szHost = p+1;
				*p = 0;
			} else {
				szHost = NULL;
				#ifdef DEBUG

				printf("szHost is NULL! szHost: '%s'\nszFrom: '%s'\nszUser: '%s'\nDATA: '%s'\n", state->szFrom, szHost, szUser, DATA);
				#endif

				FREE(szTmp);
				FREE(szUser);
				asprintf(&Output, "553 5.0.0 Invalid Address\r\n");
				break;

			}

			#ifdef DEBUG
			printf("szTmp: '%s'\nszHost: '%s'\nszUser: '%s'\nDATA: '%s'\n", szTmp, szHost, szUser, DATA);
			#endif

			/* Duplicate email address and check for validity */

			if (strlen(szTmp) > 64 || (i=spc_email_isvalid(szTmp, szUser, szHost)) == 0) {
				FREE(szTmp);
				FREE(szUser);
				// FREE(szHost);
				asprintf(&Output, "553 5.0.0 Invalid Address\r\n");
				break;
			}


			if (i == -1) {
				//FREE(szTmp);
				//szTmp=strdup("i");
				strcpy(szTmp, "i");
				state->szTo = szTmp;
				
				// pth_sleep(30);	/* 30 seconds I think that should be */
				// asprintf(&Output, "450 4.0.0 Spammer suck - Invalid Address ... (made u wait for it but) ... \r\n");
				asprintf(&Output, "550 2.0.0 Spammer suck\r\n");
				break;
				

				/* Instead, lets pretend we accept this address, but SLOW DOWN */
				//
				


			}


			/* Store TO address if valid */

			if (state->szUser) FREE(state->szUser);
			state->szUser = szUser;
			// if (state->szHost) FREE(state->szHost);
			state->szHost = szHost;
			if (state->szTo) FREE(state->szTo);
			state->szTo = szTmp;

			asprintf(&Output, "250 2.1.5 Recipient ok\r\n");
			break;

		case SMTP_DATA:
			asprintf(&Output, "354 Enter mail, end with \".\" on a line by itself\r\n");
			break;

		case SMTP_DOT:
			asprintf(&Output, "250 2.0.0 %x Message accepted for delivery\r\n", rand());
			break;

		case SMTP_QUIT:
			asprintf(&Output, "221 2.0.0 %s closing connection\r\n", MACHINE_NAME);
			break;


			/* Accumulate message text if in SMTP DATA state */
		case SMTP_IN_DATA:
			{
				char *p;
				int nDataLen;
				int bPretend = 0;

				if (state->szMessage) {
					if (state->nMessageLen < (MAX_MESSAGE_SIZE + 8192)) {
						nDataLen = strlen(DATA);
						asprintf(&p, "%s\n%s", state->szMessage, DATA);		/* Add next line of data to szMessage */
						state->nMessageLen += (nDataLen + 1);
					} else {
						bPretend = 1;
						/* How did we get here, this message is too big! */
						/* Lets just pretend we're receiving it and truncate :-p */
					}
				}
				else {
					asprintf(&p, "%s", DATA);
				}

				if (!bPretend) {
					FREE(state->szMessage);
					state->szMessage = p;
				}
			}

			break;

			/* Ignore any other non-essential commands */
		case SMTP_UNKNOWN:
		case SMTP_VRFY:
		default:
			asprintf(&Output, "500 5.5.1 Command unrecognized \"%s\"\r\n", DATA);
	}

	/* Return output */
	return Output;

#ifdef DEBUG
	if (Output)
		printf("%s", Output);
#endif
}

/* Smtp Process Input: Process line of input, and adjust state structure accordingly */
/* Returns:
   -1           Calling process should terminate socket;
   0            Calling process should do nothing (continue to read data)
   1            Calling process should send data now stored in PerIoData.Buffer
   */
int SmtpProcessInput(struct smtp_state *state, char *Input)
{

	char *Data;

	int nCode;
	int nInputLength;

	if (!Input) 
		return -1;

	/* Get new state */
	nCode = ValidateInput(state, Input, &state->szData);

	/* Check for errors */
	if (nCode == -1)
		return -1;		// At present, this will never return -1

	/* Make new state official (store in struct)*/
	STATE = nCode;

	/* Return success */
	return 1;
}

/*
   char *GetFromAddress()
   {
   return "cjl@nt4.com";
   }

   char *GetMessage()
   {
   return "This is a message";
   }
   */
