#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "smtp.h"
#include "snprintf.h"


ValidateInput(char *Input)
{
    if (!isdigit(Input[0]))
	return -1;		/* Check for non-numeric responses */
    if (!isdigit(Input[1]))
	return -1;		/* If found, return -1 for error, while will force disconnection */
    if (!isdigit(Input[2]))
	return -1;

    if (Input[3] == '-') {

	char sCrCode[6], sLfCode[6];

	sCrCode[0] = '\r';
	sLfCode[0] = '\n';
	sCrCode[1] = sLfCode[1] = Input[0];
	sCrCode[2] = sLfCode[2] = Input[1];
	sCrCode[3] = sLfCode[3] = Input[2];
	sCrCode[4] = sLfCode[4] = ' ';
	sCrCode[5] = sLfCode[5] = 0;

	if (!strstr(Input, sLfCode) && !strstr(Input, sCrCode))
	    return 0;
    }
    Input[3] = 0;		/* We're only interested in the first 3 characters */
}



char *GetFromAddress()
{
    return "me@prestongarrison.com";
}

char *GetMessage()
{
    return "This is a message";
}


char *SmtpStateChange(struct smtp_state * state)
{
#define OUTPUT_SIZE 1500
#define HELO_DOMAIN "mail.com"
#define SSC_ERROR return free(Output), NULL

    char *Output;
    char *ExtraData;

    switch (state->nState) {
    case 0:
	break;			/* This will return an empty string (not NULL) */
    case 1:
	asprintf(&Output, "HELO %s\r\n", HELO_DOMAIN);
	break;
    case 2:
	if ((ExtraData = GetFromAddress()) == NULL)
	    return NULL;
/*
	if (!GetFromAddress(sFrom)) return 0;
	
	if (sFrom.GetLength() < (MAX_EMAIL * 2))
		strcpy(PerIoData->From, sFrom);
	else {
		Error("From address too long (max: %d chars)\n", MAX_EMAIL*2);
		return 0;
	}

	if (sFrom.Find("<") != -1 && sFrom.Find(">") != -1)	{ 
		int nLeft = sFrom.Find('<');
		int nRight = sFrom.ReverseFind('>');
		if (!nLeft || ! nRight) {
			return 0;
		}

		sprintf(Output, "MAIL FROM: %s\r\n", sFrom.Mid(nLeft, nRight - nLeft + 1));
		break;
	}
		
	else 
		sprintf(Output, "MAIL FROM: <%s>\r\n", sFrom); 
*/
	asprintf(&Output, "MAIL FROM: <%s>\r\n", ExtraData);
	break;
    case 3:
	asprintf(&Output, "RCPT TO: <%s>\r\n", state->szTo);
	break;
    case 4:
	asprintf(&Output, "DATA\r\n");
	break;
    case 5:
	if ((ExtraData = GetMessage()) == NULL)
	    return NULL;

	asprintf(&Output, "%s\r\n.\r\n", ExtraData);
	break;
    case 6:
	asprintf(&Output, "RSET\r\n");
	break;
    case 7:
	asprintf(&Output, "QUIT\r\n");
	break;
    default:
    	break;
    }

    return Output;

#ifdef DEBUG
    if (Output)
	printf("%s", Output);
#endif
}

int SmtpProcessInput(struct smtp_state *state, char *Input)
{
#define STATE state->nState

    /* Returns:
       -1           Calling process should terminate socket;
       0            Calling process should do nothing (continue to read data)
       1            Calling process should send data now stored in PerIoData.Buffer
     */

    int nCode;
    int nInputLength;


    if (!Input) {
    return -1;
    }

    nInputLength = strlen(Input);
    if (nInputLength < 4)
	return 0;		/* Check for short lines */


    // Store timestamp and response.
    /*
       Wait(TTRACK.lock);
       TTRACK.lastresponse = Input;
       ttrack_pool[PerIoData->ttrack_no].timestamp = timeGetTime();
       TTRACK.updated ++;
       EndWait(TTRACK.lock);
     */

    if (ValidateInput(Input) == -1)
	return -1;

    nCode = atoi(Input);

    switch (nCode) {
    case 0:
	return 0;

    case 220:			/* 220 gatekeeper.nt4.com ESMTP Sendmail 8.11.4/8.11.4; Sun, 28 Jul 2002 03:59:28 -0700 */
	if (STATE == SMTP_START) {

	    STATE = SMTP_SENDING_HELLO;

	    //                     PerIoData->tConnectMade = clock();
	    //                     GoodConnect(PerIoData);


	} else {
	    Error("SMTP server sent an extra greeting message\n");
	    return -1;
	}

	break;

    case 221:			/* 221 2.0.0 gatekeeper.nt4.com closing connection */
	if (STATE == SMTP_SENDING_QUIT) {
	    STATE = SMTP_QUIT_ACCEPTED;

	    /*                     Wait(TTRACK.lock);
	       TTRACK.email = "Closing connection";
	       TTRACK.updated ++;
	       EndWait(TTRACK.lock);
	     */
	    return -1;		/* If we changed this to -1, we would force close */

	} else {
	    Error("SMTP server has quit unexpectedly\n");
	    return -1;
	}

	break;			/* this point not reached */

    case 250:			/* 250 gatekeeper.nt4.com Hello root@localhost [127.0.0.1], pleased to meet you */
	/* 250 2.1.0 cjl@nt4.com... Sender ok */
	/* 250 2.1.5 cjl@nt4.com... Recipient ok */
	/* 250 2.0.0 g6SAxXM27649 Message accepted for delivery */
	/* 250 2.0.0 Reset state */
	if (STATE == SMTP_SENDING_HELLO || STATE == SMTP_SENDING_MAIL_FROM) {

	    //                     PerIoData->nTo = PerIoData->nMaxTo;             /* Reset this just before we enter to RCPT TO phase */
	    /* This is going to execute twice (once for HELO and once for MAIL_FROM - Oh well! Won't hurt */

	    //                     *(PerIoData->ExtraHeaders) = 0;                 /* Zero this out */ 

	    STATE++;
	}

	else if (STATE == SMTP_SENDING_RCPT_TO) {

	    //                     g_nDelivered ++;                /* Erm, yucky non-locked globals */
	    //                     PerIoData->nTo --;              /* One less person to address this message to */

	    //                     GoodAddress(PerIoData->To, inet_ntoa(PerIoData->ulMXIp));       /* Log this as being a good address */

	    /*
	       if ((10 + strlen(PerIoData->To)) > sizeof(PerIoData->ExtraHeaders)) {
	       Error("Imminent buffer overrun in ExtraHeaders\n");
	       return -1;
	       }

	       strcat(PerIoData->ExtraHeaders, "\r\nTo: <");
	       strcat(PerIoData->ExtraHeaders, PerIoData->To);
	       strcat(PerIoData->ExtraHeaders, ">");
	     */
	    /*
	       if (PerIoData->nTo > 0
	       && g_nActive 
	       && GetToAddress(PerIoData, PerIoData->To, PerIoData->To)) 
	       {
	       STATE = SMTP_SENDING_RCPT_TO;       // Smart viewers will not that this is really the same value that STATE was set to 

	       Wait(TTRACK.lock);
	       TTRACK.email = PerIoData->To;
	       TTRACK.updated ++;
	       EndWait(TTRACK.lock);

	       } 

	       else 
	     */
	    {

		/* We've addressed this message to enough people, we can proceed with sending it */
		/*                             
		   if (g_bVerifyOnly)  {
		   if (PerIoData->nMessages > 0 && g_nActive)
		   STATE = SMTP_SENDING_RSET;
		   else 
		   STATE = SMTP_SENDING_QUIT;
		   }
		   else                                
		 */
		STATE = SMTP_SENDING_DATA;

	    }
	}

	else if (STATE == SMTP_SENDING_MESSAGE) {
	    /* We just sent a messaeg */

	    //                     PerIoData->nMessages --;

	    /*
	       if (PerIoData->nMessages > 0 && g_nActive)
	       STATE = SMTP_SENDING_RSET; // or SMTP_SENDING_QUIT 
	       else
	     */
	    STATE = SMTP_SENDING_QUIT;

	}
	/*
	   else if (STATE == SMTP_SENDING_RSET) {

	   if (PerIoData->nMessages > 0
	   && g_nActive 
	   && GetToAddress(PerIoData, PerIoData->To, PerIoData->To)) 
	   {
	   STATE = SMTP_SENDING_MAIL_FROM;     // Smart viewers will not that this is really the same value that STATE was set to 
	   Wait(TTRACK.lock);
	   TTRACK.email = PerIoData->To;
	   TTRACK.updated ++;
	   EndWait(TTRACK.lock);
	   } 

	   else {

	   // We've addressed this message to enough people, we can proceed with sending it 

	   STATE = SMTP_SENDING_QUIT;
	   }
	   }

	 */
	else {
	    Error("SMTP server acknowledged a request out of turn\n");
	    return -1;
	}

	break;			/* As if we get here anyway! */

    case 354:			/* 354 Enter mail, end with "." on a line by itself */

	if (STATE == SMTP_SENDING_DATA) {
	    STATE = SMTP_SENDING_MESSAGE;
	}

	else {
	    Error("SMTP server is asking for message text out of turn\n");
	    return -1;
	}

	break;			/* Never reached */

    default:

	//             g_nFailed ++; /* TODO: this properly */

	// TODO: Proper error handling 

	//             if (STATE == SMTP_SENDING_RCPT_TO)  // Got an error specifying a recipient, so just try again! 
	{
	    // Yucky CString code! 
	    /*                     
	       CString sBadLine;
	       sBadLine.Format("%s,\"%s\"", PerIoData->To, &Input[4]);
	       BadAddress(sBadLine);
	     */
	    // NB: If there are no more recipients matching @domain.com, and there were no previous 
	    //     valid RCPT TO's specified, then will ungracefully return server error such as:   
	    //         'NO RECIPIENTS SPECIFIED'                                                                                                            


	    // Duplication of some code above: 
	    /*
	       if (PerIoData->nTo > 0      
	       && g_nActive 
	       && GetToAddress(PerIoData, PerIoData->To, PerIoData->To)) 
	       {
	       STATE = SMTP_SENDING_RCPT_TO;       // Smart viewers will not that this is really the same value that STATE was set to 
	       Wait(TTRACK.lock);
	       TTRACK.email = PerIoData->To;
	       TTRACK.updated ++;
	       EndWait(TTRACK.lock);

	       } 
	     */
	    //                     else 
	    {

		// We've addressed this message to enough people, we can proceed with sending it 
		/*
		if (g_bVerifyOnly)
		    STATE = SMTP_SENDING_RSET;
		else
		*/
		    STATE = SMTP_SENDING_DATA;

	    }
	    // End duplication 
	}

	/*
	   else {
	   if (pst.IsBlackListed(&Input[4]))           // Check for any kind of rbl/blacklist message
	   {
	   RemovedAddress(PerIoData->To);
	   g_nRemoved ++;

	   // Direct or via Proxy delivery error: remove the domain from the list of
	   // addresses we're mailing 

	   if (g_nDeliverDirect & METH_NOTRELAY) 
	   WaxDomain(PerIoData->Domain);
	   else {
	   Wait(ttrack_pool[PerIoData->ttrack_no].ttrack.lock);
	   DisableRelay(ttrack_pool[PerIoData->ttrack_no].ttrack.relay, "Banned"); 
	   ttrack_pool[PerIoData->ttrack_no].ttrack.updated++; 
	   EndWait(ttrack_pool[PerIoData->ttrack_no].ttrack.lock);
	   }

	   Error("Blacklisted (%s): %s\n", PerIoData->To, &Input[4]);
	   } 
	   else 
	 */
	Error("Smtp Terminal Error (%s): %s %s\n", "" /* PerIoData->To */ , Input, &Input[4]);

	// Now terminate this connection 
	return -1;
	//             }


	break;			// not reachecd

    }				// switch 

    /* Okay, if we have reached here, then everything is good! */

    return 1;

    //     if (!SmtpStateChange(PerIoData))        /* Prepare the output based on our new state */
    //             return -1;      /* Something went wrong */
    //     else
    //             return 1;       /* Return to calling function for WSASend() of data */
}
