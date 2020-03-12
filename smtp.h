// #define DEBUG
// #define TABDEBUG
#ifndef KHFHVMIJFLDIJIRJ
#define KHFHVMIJFLDIJIRJ

#define FREE(x) {if (x) {free(x); x=NULL;}}

#define SMTP_START				0
#define SMTP_SENDING_HELLO		1
#define SMTP_SENDING_MAIL_FROM	2
#define	SMTP_SENDING_RCPT_TO	3
#define SMTP_SENDING_DATA		4
#define SMTP_SENDING_MESSAGE	5
#define SMTP_SENDING_QUIT		7
#define SMTP_SENDING_RSET		6
#define SMTP_QUIT_ACCEPTED		8

#define SMTP_UNKNOWN 0
#define SMTP_HELO 1
#define SMTP_VRFY 2
#define SMTP_MAIL_FROM 3
#define SMTP_RCPT_TO 4
#define SMTP_DATA 5
#define SMTP_RSET 6
#define SMTP_QUIT 7
#define SMTP_DOT 8
#define SMTP_IN_DATA 9


/* States:	0 - Socket Open
			1 - Server has greeted us
			2 - Server has accepted helo
			3 - Server has accepted MAIL FROM;
			4 - Server has accepted RCPT TO
			5 - Server is prepared to accept data
			6 - Server has acknowledged EOF
			7 - Server has acknowledged QUIT
*/


struct smtp_state {
	int	nState;
	char *szTo;
	char *szFrom;
	char *szMessage;
	char *szData;
	char *szRemoteIp;
	char *szUser, *szHost;
	int nMessageLen;
	int nRblResult;
//	int nMemLen;
};

extern int SmtpProcessInput(struct smtp_state *state, char *Input);
extern char *SmtpStateChange(struct smtp_state * state);

#endif
