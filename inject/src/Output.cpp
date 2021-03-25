#include "Output.h"

BOOL Run = FALSE;
std::ostringstream OUTPUT;
HANDLE hThread;

DWORD WINAPI PIPETHREAD(LPVOID lpParameter) {
	HANDLE hPipe;
	char buffer[BUFFER];
	DWORD dwRead;


	hPipe = CreateNamedPipe(TEXT("\\\\.\\pipe\\quantumcore"),
		PIPE_ACCESS_DUPLEX,
		PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,   // FILE_FLAG_FIRST_PIPE_INSTANCE is not needed but forces CreateNamedPipe(..) to fail if the pipe already exists...
		1,
		1024 * 16,
		1024 * 16,
		NMPWAIT_USE_DEFAULT_WAIT,
		NULL);

	while (Run)
	{
		
		while (hPipe != INVALID_HANDLE_VALUE)
		{
			if (ConnectNamedPipe(hPipe, NULL) != FALSE)   // wait for someone to connect to the pipe
			{
				memset(buffer, '\0', BUFFER);
				while (ReadFile(hPipe, buffer, sizeof(buffer) , &dwRead, NULL) != FALSE)
				{
					buffer[dwRead] = '\0';
					//printf("%s\n", buffer);
					OUTPUT << buffer;
				}
			}

			DisconnectNamedPipe(hPipe);
		}

		if (!Run)
		{
			break;
		}
	}

	return 0;
}

void Prepare()
{
	Run = TRUE;
	hThread = CreateThread(NULL, 0, PIPETHREAD, NULL, 0, NULL);
	if (hThread == NULL)
	{
		printf("Error Creating Thread: %ld\n", GetLastError());
	}
}

BOOL isPipeThreadRunning()
{
	DWORD exitCode;
	return GetExitCodeThread(hThread, &exitCode);
}

std::string ReadReflectiveDllOutput(int Timeout)
{
	int x = 0;
	if (Run)
	{
		do {
			Sleep(1000);
			x++;
		} while (x != Timeout);

		Run = FALSE; // The thread ends.

	}
	return OUTPUT.str();
}

