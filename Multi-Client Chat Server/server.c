#include "proj.h"
#include "trie.h"
#include "protocol_defs.h"

#define QLEN 2 
#define MAX_MESSAGE_LEN 4095
#define MAX_FRAGMENT_LEN 255
int visits = 0; 

void checkCommandLineArguments(int argc, char **argv);

uint16_t maxClients = 0;
uint16_t maxUlen = 0;
uint16_t timeOut = 0;
trie_node * usernames;

//Structure for client info 
struct Client{

    char username[64];
    int clientSocket; 
    bool active;
    time_t timeLastComm; 
};

uint16_t header;
uint8_t MT = 0;
uint8_t CODE = 0;
uint8_t UNC = 0;
uint8_t PUB = 0;
uint8_t ULEN = 0;
uint8_t PRV = 0;
uint8_t FRG = 0;
uint8_t LST = 0;
uint8_t RSVD = 0;
uint16_t MESS_LENGTH = 0;

//Set all client info to default values 
void clientsInit(struct Client * clients, size_t len) {
    
    for(int i = 0; i < len; i++) {

        clients[i].active = false;
        clients[i].clientSocket = 0;
        clients[i].timeLastComm = 0;
        clients[i].username[0] = '\0';
    }
}

//Checks our client array for the first available index 
int getFreeClient(struct Client * clients) {

    for(int i = 0; i < maxClients * 2; i++) {

        if(clients[i].active == false && clients[i].clientSocket == 0) {

            return i;
        }
    }
    
    return -1;
}

//resets a client to the default unoccupied state, a new client can occupy this object
void resetClient(struct Client * client) {

    client->active = false;
    client->clientSocket = 0;
    client->timeLastComm = 0;
    client->username[0] = '\0';
}

//Sends a control header to the client indicating the username they entered was too long 
void usernameTooLong(int clientSocket, int ULEN) {

    if(ULEN > maxUlen) {

        uint16_t header;
        create_control_header(&header, 0, CODE_USER_NEGO_MESS, UNC_USERNAME_TOO_LONG, 0);
        header = htons(header);
        send(clientSocket, &header, 2, 0);
    }
}

//Checks to see if a username entered is valid. Checks lowercase, uppercase and number values associated with usernames. 
bool validUsername(char * proposedUsername, int ULEN) {

    for(int idx = 0; idx < ULEN; idx++) {

        if(proposedUsername[idx] > 122  || proposedUsername[idx] < 97) { 

            if(proposedUsername[idx] < 97 && proposedUsername[idx] < 65) { 

                if(proposedUsername[idx] < 65 && (proposedUsername[idx] > 57 || proposedUsername[idx] < 48)) { 
                
                    return false;
                }
            }
        }
    }

    return true;
}

//Parse the first byte of the header to check to see if the MT bit is set
int isChatHeader(uint16_t header) {
    
    uint8_t upperByte = header >> 8; 
    int res = parse_mt_bit(&upperByte); 
    return res; 
}

//Handle timeouts for clients who take too long to make a username 
int handleTimeouts(struct Client * clients, size_t len, time_t timeout, fd_set * readSet) {

    int i = 0;
    int amountKicked = 0;
    time_t timeNow = time(NULL);

    //Loop through all clients (this simply checks all clients)
    while(i < len) { 
        
        //If we find that no username is entered and that the client has exceeded the time limit 
        if((strlen(clients[i].username) == 0) && (clients[i].clientSocket > 0) && (timeOut < (timeNow - clients[i].timeLastComm))) { 
            
            //Send a control header to the client to kick the client 
            uint16_t header;
            create_control_header(&header, 0, CODE_USER_NEGO_MESS, UNC_USERNAME_TIME_EXP, 0);
            header = htons(header);
            send(clients[i].clientSocket, &header, sizeof(header), 0);

            //Close the client socket and clear it's file descriptor. Reset client data to be used by a new client
            close(clients[i].clientSocket);
            FD_CLR(clients[i].clientSocket, readSet);
            amountKicked++;
            resetClient(&clients[i]);
        }

        i++;
    }
    
    return amountKicked;
}

void createAndSendControlHeader(int clientSocket, uint8_t mt, uint8_t code, uint8_t unc, uint8_t ulen) {

    uint16_t header;
    create_control_header(&header, mt, code, unc, ulen);
    header = htons(header);
    send(clientSocket, &header, 2, 0); 
}

void createAndSendChatHeader(int clientSocket, uint8_t mt, uint8_t pub, uint8_t prv, uint8_t frg, uint8_t lst, uint8_t ulen, uint16_t messLength) {

    uint32_t chatHeader;
    create_chat_header(&chatHeader, mt, pub, prv, frg, lst, ulen, messLength);
    chatHeader = htonl(chatHeader);
    send(clientSocket, &chatHeader, 4, 0); 
}

//Broadcast a new user to all active clients. Indicating a new user has joined. 
void broadcastNewUser(uint16_t header, struct Client * clients, char * proposedUsername, int ULEN) {

    for(int idx = 0; idx < maxClients; idx++) {

        if(clients[idx].active){

            send(clients[idx].clientSocket, &header, 2, 0);
            send(clients[idx].clientSocket, proposedUsername, ULEN, 0);
        }
    }
} 

//This method will broadcast the public message to all clients 
void broadcastPublicMessage(int socket, struct Client * clients, char * client_message) {
             
    //Assign char pointer to the username of the sender 
    char * username;
    for(int i = 0; i < maxClients; i++) {

        if(clients[i].clientSocket == socket) {
            
            username = clients[i].username;
        }
    }    
    
    //Iterate through all clients and send the username and message in two seperate send calls 
    for(int idx = 0; idx < maxClients; idx++) {

        if(clients[idx].active && clients[idx].clientSocket != socket) {
            
            createAndSendChatHeader(clients[idx].clientSocket, MT_CHAT, PUB_PUB, PRV_NOT_PRIV, FRG_NOT_FRAG, 0, strlen(username), MESS_LENGTH);
            send(clients[idx].clientSocket, username, strlen(username), 0); 
            send(clients[idx].clientSocket, client_message, MESS_LENGTH, 0); 
        }
    } 
}

//Checks to see if the incoming header is a control or chat header and parses the header
int handleHeaders(int curSocket, int * n, uint16_t * cHeader, uint32_t * chatHeader) {
    
    //Receive incoming header and check the first bit (MT bit)
    uint16_t cHeaderLower = 0;
    *n = recv(curSocket, cHeader, 2, 0); 
    *cHeader = ntohs(*cHeader);
    int bIsChatHeader = isChatHeader(*cHeader); 
    
    //If this is a chat header, then we will assemble the remaining 16 bits. 
    if(bIsChatHeader && !(*n <= 0)) { 

        //Read the header once more, parse the upper 16 bits, and then parse the header. 
        *n = recv(curSocket, &cHeaderLower, 2, 0); 
        cHeaderLower = ntohs(cHeaderLower);
        *chatHeader |=  *cHeader << 16; 
        *chatHeader |= cHeaderLower; 
        parse_chat_header(chatHeader, &MT, &PUB, &PRV, &FRG, &LST, &ULEN, &MESS_LENGTH);
    }
    
    return bIsChatHeader;
}

//Sends a private message to the correct recipient. Or returns a flag indicating the client does not exist 
int sendPrivateMessage(int curSocket, struct Client * clients, char * intendedUsername, char * client_message) {

    //Assign char pointer to the username of the sender 
    char * username;
    int bIsValidUsername = 0;
    for(int i = 0; i < maxClients; i++) {

        if(clients[i].clientSocket == curSocket) {
            
            username = clients[i].username;
        }
    }

    //For all clients, find the correct recipient from the username entered in the message 
    for(int i = 0; i < maxClients; i++) {

        if(strncmp(clients[i].username, intendedUsername, ULEN) == 0) {
            
            //Send message to that specific client and header. Return that the recipient does exist 
            bIsValidUsername = 1;
            createAndSendChatHeader(clients[i].clientSocket, MT_CHAT, PUB_PRIV, PRV_PRV, 0, 0, strlen(username), MESS_LENGTH);
            send(clients[i].clientSocket, username, strlen(username), 0);
            send(clients[i].clientSocket, client_message, MESS_LENGTH, 0);
            return bIsValidUsername;
            
        }
    }
    
    return bIsValidUsername;
} 

//Receives chat messages containing multiple fragments 
int recieveChatMessage(int clientSocket, char * messageDest, uint16_t messagedLength){
    
    //Array specifiying the message length from the header 
    int endOfMessageIndex = MESS_LENGTH;
    char msgFragment[MESS_LENGTH]; 

    //While we have not received our last fragment 
    while(LST != LST_LAST_FRG) { 

        //Continue to receive fragments and append the new fragment to the array containing the message. Continue to check header for the last fragment. 
        recv(clientSocket, msgFragment, MESS_LENGTH, 0);
        uint16_t controlHeader = 0;
        uint32_t chatHeader = 0;
        int n;

        memcpy(messageDest + (endOfMessageIndex - MESS_LENGTH), msgFragment, MESS_LENGTH); 
        handleHeaders(clientSocket, &n, &controlHeader, &chatHeader); 
        
        //If the message received from the client is too long, return flag 
        if(endOfMessageIndex > MAX_MESSAGE_LEN) {

            return 0;
        }

        //Empty our buffer and increment our index for message array 
        bzero(msgFragment, MESS_LENGTH);
        endOfMessageIndex += MESS_LENGTH;
    }

    //Receive the last fragment from the client. Return flag that message is valid 
    recv(clientSocket, msgFragment, MESS_LENGTH, 0); 
    if(endOfMessageIndex > MAX_MESSAGE_LEN) {
        
        return 0; 
    }

    memcpy(messageDest + (endOfMessageIndex - MESS_LENGTH), msgFragment, MESS_LENGTH); 
    bzero(msgFragment, MESS_LENGTH);
    MESS_LENGTH = endOfMessageIndex;
    return 1;
}

int main(int argc, char **argv) {
    
    struct protoent *pointerToProtocolTable;
    struct sockaddr_in serverAddress, clientAddress;
    int severSocket, clientSocket;
    int port; int optval = 1; int totalClients = 0;
    checkCommandLineArguments(argc, argv);

    //Total set of connections will be twice the number of clients 
    struct Client clients[maxClients * 2];
    clientsInit(clients, maxClients * 2);
    usernames = trie_create(); 

    //Clear our server address and set address family and IP address
    memset((char *)&serverAddress, 0, sizeof(serverAddress)); 
    serverAddress.sin_family = AF_INET;            
    serverAddress.sin_addr.s_addr = INADDR_ANY;     

    //Convert port number from host to network short 
    port = atoi(argv[1]); 
    if (port > 0) { 
    
        serverAddress.sin_port = htons((u_short)port);
    }

    else { 
    
        perror("Error: bad port number");
        exit(EXIT_FAILURE);
    }

    //Get TCP info for server 
    if (((long int)(pointerToProtocolTable = getprotobyname("tcp"))) == 0) {

        fprintf(stderr, "Error: Cannot map \"tcp\" to protocol number");
        exit(EXIT_FAILURE);
    }

    //Create a socket for server with TCP protocol info 
    severSocket = socket(PF_INET, SOCK_STREAM, pointerToProtocolTable->p_proto);
    if (severSocket < 0) {

        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    //Assign options at the socket level and allow this socket to be resued by clients 
    if (setsockopt(severSocket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {

        perror("Error setting socket option");
        exit(EXIT_FAILURE);
    }

    //Bind the address of the server to the socket. 
    if (bind(severSocket, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0) {

        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    //Listen for incoming connections on this socket 
    if (listen(severSocket, QLEN) < 0) {

        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    //File descriptor sets for the read and write sets 
    struct timeval timeout;
    fd_set readSet, readyToReadSet;
    int max_fd;

    //Empty the read set, allow the server socket to listen for incoming data on file descriptors 
    FD_ZERO(&readSet);
    FD_SET(severSocket, &readSet);
    max_fd = severSocket;

    //While loop to continue listening for connections 
    while (1) {

        //Copy the read set to the ready to read set 
        memcpy(&readyToReadSet, &readSet, sizeof(readSet));

        //Wait one second on each file descriptor 
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        //Use select to monitor all file descriptors. Select will block until a socket becomes available. 
        int r = select(max_fd + 1, &readyToReadSet, NULL, NULL, &timeout);
        
        if (r < 0) {

            perror("Error with select");
            exit(EXIT_FAILURE); 
        } 
        
        //If no data is received we will kick all clients who take too long to make a username
        else if (r == 0){

            int amountKicked = handleTimeouts(clients, maxClients * 2, timeOut, &readSet);
            totalClients -= amountKicked; 
            continue; 
        }
        
        int amountKicked = handleTimeouts(clients, maxClients * 2, timeOut, &readSet);
        totalClients -= amountKicked;

        //Loop through all file descriptors to see which file descriptors are ready 
        for (int curSocket = 0; curSocket <= max_fd; curSocket++) {

            //If we find our current socket is part of the file descriptor set 
            if (FD_ISSET(curSocket, &readyToReadSet)) {
                
                //If we find our current socket is the server socket, we will check for new connections from incoming clients. 
                if (curSocket == severSocket) {

                    //We will need to check if the current number of clients exceeds the maximum. Accept the new client. 
                    socklen_t alen = sizeof(clientAddress);
                    if ((clientSocket = accept(severSocket, (struct sockaddr *)&clientAddress, &alen)) < 0) {

                        fprintf(stderr, "Error: Accept failed\n");
                        exit(EXIT_FAILURE);
                    } 
                    
                    //If we find the current number of clients exceeds the maximum, we will send a header to that client and close the socket. 
                    if (totalClients >= maxClients) {

                        createAndSendControlHeader(clientSocket, 0, CODE_SERVER_FULL, 0, 0);
                        close(clientSocket);
                        continue;
                    } 
                    
                    //Else, we will accept the new client and add that socket to the read set of file descriptors. 
                    else { 
                    
                        //Update the current number of file descriptors and send a header to the client. 
                        FD_SET(clientSocket, &readSet);
                        max_fd = clientSocket > max_fd ? clientSocket : max_fd; 
                        create_control_header(&header, 0, CODE_SERVER_NOT_FULL, 0, maxUlen);
                        header = htons(header);
                        send(clientSocket, &header, 2, 0);

                        //Get the index of the newly added client. Set the socket and time since last commit 
                        int free = getFreeClient(clients); 
                        clients[free].clientSocket = clientSocket;
                        time_t connectTime = time(NULL);
                        clients[free].timeLastComm = connectTime;
                        
                        totalClients++;
                    }
                } 

                //Else, we know a client socket is ready 
                else {

                    //Server will expect a header from the client (control header or chat header)
                    uint16_t controlHeader = 0; 
                    uint32_t chatHeader = 0; 
                    int n; 

                    //Fill header with appropriate info. Return if control or chat header               
                    int bIsChatHeader = handleHeaders(curSocket, &n, &controlHeader, &chatHeader); 

                    //If control header, parse                                                 
                    if(!bIsChatHeader){

                        parse_control_header(&controlHeader, &MT, &CODE, &UNC, &ULEN);
                    }
                        
                    //If we received no data from client, client disconnected 
                    if (n <= 0) {

                        if (n < 0) {

                            perror("Error receiving data from client");
                        }

                        //Else if a client disconnects, create control header for client disconnect
                        else {
                        
                            fprintf(stderr, "Client disconnected\n");
                            totalClients--;
                            
                            create_control_header(&header, 0, CODE_USER_LEFT, 0, ULEN);
                            header = htons(header); 
                            int clientsIdx;

                            //Find the appropriate index in our client array for the disconnected client 
                            for(int idx = 0; idx < maxClients; idx++) {

                                if(clients[idx].clientSocket == curSocket) {
                                    
                                    clientsIdx = idx;
                                }
                            }

                            //Once we have the client index (active client) 
                            if(clients[clientsIdx].active) { 

                                //Delete their username from the trie and reset client info to be used by another client 
                                trie_delete(usernames, clients[clientsIdx].username, strlen(clients[clientsIdx].username));
                                resetClient(&clients[clientsIdx]);
                                
                                //For all active clients, send the header and username of the client that disconnected 
                                for(int idx = 0; idx < maxClients; idx++) {

                                    if(clients[idx].active){

                                        send(clients[idx].clientSocket, &header, 2, 0);
                                        send(clients[idx].clientSocket, clients[clientsIdx].username, strlen(clients[clientsIdx].username), 0); //sends the username of the user who disconnected
                                    }
                                }    
                            } 
                            
                            //If the client was not active, simply reset data for that client
                            else {
                                
                                resetClient(&clients[clientsIdx]);
                            }
                        }

                        //Close socket, remove socket from the file descriptor set 
                        close(curSocket);
                        FD_CLR(curSocket, &readSet); 
                        continue; 
                    }

                    //If we have a control message 
                    if(!bIsChatHeader){

                        //Apply the appropriate code in the header we received from the client
                        switch(CODE) {

                            //Username negotiation message. There are a few different cases here. We will first read in the username to determine 
                            //the case in which the negotation failed. If the username is too long, contains invalid characters, or is already taken, 
                            //we will send the correct control header to the client. Else, we will add that username to our trie. 
                            case CODE_USER_NEGO_MESS: { 

                                char proposedUsername[ULEN];
                                recv(curSocket, proposedUsername, ULEN, 0); 

                                if(ULEN > maxUlen) {

                                    usernameTooLong(curSocket, ULEN);
                                    break;
                                }

                                if(!validUsername(proposedUsername, ULEN)) {

                                    createAndSendControlHeader(curSocket, 0, CODE_USER_NEGO_MESS, UNC_USERNAME_INVALID, 0);
                                    break;
                                }

                                if(trie_search(usernames, proposedUsername, ULEN)) { 

                                    createAndSendControlHeader(curSocket, 0, CODE_USER_NEGO_MESS, UNC_USERNAME_TAKEN, 0);
                                    break;
                                } 
                                
                                else {

                                    trie_insert(usernames, proposedUsername, ULEN);
                                }

                                //Our clients for this program are indexed arbitrarily. So, we need to find the 
                                //client with the same socket as the one we're handling right now. 
                                for(int idx = 0; idx < maxClients; idx++) {

                                    //Assign that client their username and set their active status to true 
                                    if(clients[idx].clientSocket == curSocket) {

                                        memcpy(clients[idx].username, proposedUsername, ULEN);
                                        clients[idx].active = true;
                                    }
                                }

                                //Send header to the client that their username has been accepted. Also, send header 
                                //to all clients that a new user has joined. 
                                createAndSendControlHeader(curSocket, 0, CODE_USER_NEGO_MESS, UNC_USERNAME_ACCEPT, ULEN);
                                send(curSocket, proposedUsername, ULEN, 0);
                                
                                create_control_header(&header, 0, CODE_NEW_USER_CONNECT, 0, ULEN);
                                header = htons(header); 
                                broadcastNewUser(header, clients, proposedUsername, ULEN);

                                break;
                            }
                        }
                    } 
                    
                    //Else, server recieved a chat header
                    else {
                        
                        //If the server receives a message length greater than 255, we will send a control header to the client. 
                        if(MESS_LENGTH > MAX_FRAGMENT_LEN) { 
                        
                            createAndSendControlHeader(curSocket, 0, CODE_MESS_TOO_LONG, 0, 0);
                            break;
                        }

                        //Array for the message to be sent to the client
                        char client_message[MAX_MESSAGE_LEN];
                        switch(PUB) {

                            //If we have a public message, we will check to see if we need to receive the message in multiple fragments. 
                            case PUB_PUB: {
                    
                                //If no fragment is needed, and the message is public, we will simply broadcast the message 
                                if(PRV == PRV_NOT_PRIV && FRG == FRG_NOT_FRAG && LST == 0) {
                                    
                                    recv(curSocket, client_message, MESS_LENGTH, 0);
                                    broadcastPublicMessage(curSocket, clients, client_message);
                                } 
                                
                                //Else, if the message is fragmented 
                                else if(FRG == FRG_FRG) { 

                                    //Receive message in fragments. Broadcast message to clients unless it exceeds maximum length 
                                    int bMessageIsWellFormed = recieveChatMessage(curSocket, client_message, MESS_LENGTH);

                                    if(!bMessageIsWellFormed) {

                                       createAndSendControlHeader(curSocket, 0, CODE_MESS_TOO_LONG, 0, 0);
                                       break; 
                                    }
                                    
                                    broadcastPublicMessage(curSocket, clients, client_message); 
                                }

                                break;
                            }

                            //Else, if we have a private message 
                            case PUB_PRIV: {
                                
                                //If there are no fragments, receive the username and message and send the private message to the correct recipient
                                if(PRV == 1 && FRG == 0 && LST == 0) { 

                                    char intendedUsername[ULEN]; 
                                    recv(curSocket, intendedUsername, ULEN, 0); 
                                    recv(curSocket, client_message, MESS_LENGTH, 0); 

                                    int recipientExists = sendPrivateMessage(curSocket, clients, intendedUsername, client_message); 

                                    if(!recipientExists) {
                                        
                                        createAndSendControlHeader(curSocket, 0, CODE_RECIP_NOT_ACTIVE, 0, ULEN);
                                        send(curSocket, intendedUsername, ULEN, 0);
                                    }
                                } 
                                
                                //Else if the message is fragmented 
                                else if (FRG == FRG_FRG) { 

                                    //We will first recieve the username from the client and then receive the chat message in fragments 
                                    char intendedUsername[ULEN];
                                    recv(curSocket, intendedUsername, ULEN, 0); 
                                    int bMessageIsWellFormed = recieveChatMessage(curSocket, client_message, MESS_LENGTH);

                                    //Check if message exceeds message length 
                                    if(!bMessageIsWellFormed) {

                                       createAndSendControlHeader(curSocket, 0, CODE_MESS_TOO_LONG, 0, 0);
                                       break; 
                                    }

                                    //If the recipient does exist, we will send a private message to that recipient
                                    int recipientExists = sendPrivateMessage(curSocket, clients, intendedUsername, client_message); 

                                    //Else, we will send a control header to the client indicating that no recipient exists 
                                    if(!recipientExists) {
                                        
                                        createAndSendControlHeader(curSocket, 0, CODE_RECIP_NOT_ACTIVE, 0, ULEN);
                                        send(curSocket, intendedUsername, ULEN, 0);
                                    }
                                }

                                break;
                            }
                        }
                    }
                }
            }
        }
    }
}

//Simple method to check command line arguments with program 
void checkCommandLineArguments(int argc, char *argv[]) {

    if (argc != 5) {
        
        PRINT_MSG("Invalid number of arguments.\n");
        exit(EXIT_FAILURE);
    }

    int portNumber = atoi(argv[1]);
    if (sizeof(atoi(argv[1])) != sizeof(int) || portNumber < 1024 || portNumber > 65535) {
        
        PRINT_MSG("Invalid port number.\n");
        exit(EXIT_FAILURE);
    }

    maxClients = atoi(argv[2]);
    if (maxClients <= 0 || maxClients >= 256) {

        PRINT_MSG("Invalid number of clients.\n");
        exit(EXIT_FAILURE);
    }

    timeOut = atoi(argv[3]);
    if (timeOut <= 0 || timeOut >= 300) {

        PRINT_MSG("Invalid time out number.\n");
        exit(EXIT_FAILURE);
    }

    maxUlen = atoi(argv[4]);
    if (maxUlen <= 0 || maxUlen >= 11) {

        PRINT_MSG("Invalid user name length.\n");
        exit(EXIT_FAILURE);
    }
}
