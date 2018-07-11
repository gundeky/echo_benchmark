#include <stdio.h>
#include <string.h>
#include <signal.h>

#include "proactor.h"

aio::Proactor* g_proactor;

void SigHandler(int nSig)
{
	switch(nSig)
	{
		case SIGINT:
			printf("SigHandler: SIGINT signal\n");
			break;

		case SIGTERM:
			printf("SigHandler: SIGTERM signal\n");
			break;

		case SIGSEGV:
			printf("SigHandler: SIGSEGV signal\n");
			break;

		case SIGPIPE:    // socket closed while writing
			printf("SigHandler: SIGPIPE signal\n");
			return;
			//break;

		default:
			printf("SigHandler: %d signal\n", nSig);
			break;
	}
	g_proactor->stop();
}

void SetSigHandler()
{
	struct sigaction sa;
	memset(&sa, 0, sizeof(struct sigaction));
	sa.sa_handler = SigHandler;
	sigfillset(&sa.sa_mask);
	sigaction(SIGINT , &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);
	//sigaction(SIGSEGV, &sa, NULL);
	sigaction(SIGPIPE, &sa, NULL);
}

