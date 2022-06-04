#include "proj.h" 
#include "protocol_defs.h"

#define MAX_FRAGMENT_LEN 255
#define MAX_MESSAGE_LEN 4095
#define MAX_BUFFER_LEN 4095
#define MAX_USERNAME_LEN 11

void checkCommandLineArguments(int argc, char **argv);
void createAndSendHeader(int clientSocket, uint8_t mt, uint8_t code, uint8_t unc, uint8_t ulen);
void codeCheck();
void negotiateCode1();
void code4();
void recieveChatMessage(); 
void sendChatMessage(); 
int isChatHeader(uint16_t header);
int handleHeaders(int clientSocket, int * n, uint16_t * cHeader, uint32_t * chatHeader);
    
    int clientSocket;
    int n; int more = 1;
    char buf[MAX_BUFFER_LEN]; 
    char msg[MAX_MESSAGE_LEN]; 
    char username[MAX_USERNAME_LEN];
    char recUser[MAX_USERNAME_LEN]; 
    uint8_t recUserLength; 
    int waitingOnInput = 0; 
    int acceptedUsername = 0;
    uint16_t header;
    uint8_t mt;
    uint8_t code;
    uint8_t rsvd;
    uint8_t unc;
    uint8_t ulen;
    uint8_t max_ulen = 0;

    uint8_t pub;
    uint8_t prv;
    uint8_t frg;
    uint8_t lst;
    uint8_t pad;
    uint16_t length;

int main(int argc, char **argv) {
    
    struct hostent *pointerToHostTable;
    struct protoent *pointerToProtocolTable;
    struct sockaddr_in serverAddress;
    uint16_t port; 
    checkCommandLineArguments(argc, argv);
    
    //Clear server address structure, set family to AF_INET 
    memset((char *) &serverAddress, 0, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET; 

    //Convert port number and convert to network short 
    port = atoi(argv[2]); 
    if (port > 0) { 

        serverAddress.sin_port = htons((u_short) port);
    } 

    else {

        perror("Error: bad port number");
        exit(EXIT_FAILURE);
    }

    //Get host information from IP address and copy to server address structure. 
    pointerToHostTable = gethostbyname(argv[1]);
    if (pointerToHostTable == NULL) {

        perror("Error: Invalid host");
        exit(EXIT_FAILURE);
    }

    //Pointer to proto table will return protocols for sending and receiving data in TCP
    memcpy(&serverAddress.sin_addr, pointerToHostTable->h_addr, pointerToHostTable->h_length);
    if (((long int) (pointerToProtocolTable = getprotobyname("tcp"))) == 0) {

        perror("Error: Cannot map \"tcp\" to protocol number");
        exit(EXIT_FAILURE);
    }

    //Create a socket for the client, return a file descriptor for protocol info 
    clientSocket = socket(PF_INET, SOCK_STREAM, pointerToProtocolTable->p_proto);
    if (clientSocket < 0) {

        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    //Connect the client socket with the server address 
    if (connect(clientSocket, (struct sockaddr *) &serverAddress, sizeof(serverAddress)) < 0) {

        perror("Connect failed");
        exit(EXIT_FAILURE);
    }

    bzero(username, sizeof(username));
    fd_set readSet, readyToReadSet;
    int retval; 

    //Empty the read set. Set the input file descriptor (zero) to the read set, and the client socket. We will copy the 
    //read set to the ready to read set. We will wait for input on STDIN_FILENO and recieve input on clientSocket 
    FD_ZERO(&readSet);
    FD_SET(STDIN_FILENO, &readSet);
    FD_SET(clientSocket, &readSet);

    //Main loop for our chat program 
    while(1) { 
        
        //Create a copy of the read set. Monitor all file descriptors for all client sockets using select. (Select blocks here until data is to be read. 
        //Once data is read, it will go to the first call of read_stdin and enter the data itself)
        memcpy(&readyToReadSet, &readSet, sizeof(readSet));
        retval = select(clientSocket + 2, &readyToReadSet, NULL, NULL, NULL); 

        if(retval == 0) {

            continue;
        }

        if(retval < 0) {

            perror("Error with select");
            exit(EXIT_FAILURE);
        } 

        //If a file descriptor is ready, we will check if a username has been given. Else, we will allow the client to type a message. 
        if (FD_ISSET(STDIN_FILENO, &readyToReadSet)) {
            
            waitingOnInput = 1; 
            if(!acceptedUsername) {

                negotiateCode1();
            } 
            
            else if(waitingOnInput && acceptedUsername) {

                sendChatMessage();
            }
        } 
        
        //If the client socket is ready, we will first run the code check for the username 
        else if (FD_ISSET(clientSocket, &readyToReadSet)){ 

            uint16_t controlHeader = 0; 
            uint32_t chatHeader = 0; 
            int n = 0;
            
            //Will check if the received header is a control or chat header 
            int bIsChatHeader = handleHeaders(clientSocket, &n, &controlHeader, &chatHeader); 

            if(!bIsChatHeader) {

                parse_control_header(&controlHeader, &mt, &code, &unc, &ulen); 
                if (code == 0 && acceptedUsername) {

                    PRINT_MSG("Lost connection with the server\n");
                    exit(EXIT_SUCCESS);
                }

                codeCheck();
            } 
            
            else {

                recieveChatMessage();
            }
        }  
    }
}

int isChatHeader(uint16_t header) {
    
    uint8_t upperByte = header >> 8; 
    int res = parse_mt_bit(&upperByte); 
    return res; 
}

//This method checks to see if the new header is a control or chat header 
int handleHeaders(int clientSocket, int * n, uint16_t * cHeader, uint32_t * chatHeader) {
    
    //Receive header from server and parse the MT bit. If the MT bit is set to one, we will parse the following 16 bits of the chat header. 
    uint16_t cHeaderLower = 0;
    *n = recv(clientSocket, cHeader, 2, 0); 
    *cHeader = ntohs(*cHeader);
    int bIsChatHeader = isChatHeader(*cHeader); 

    //Receive the remaining 16 bits from the server and parse. 
    if(bIsChatHeader && !(*n <= 0)) { 

        *n = recv(clientSocket, &cHeaderLower, 2, 0); 
        cHeaderLower = ntohs(cHeaderLower);
        *chatHeader |=  *cHeader << 16; 
        *chatHeader |= cHeaderLower; 
        parse_chat_header(chatHeader, &mt, &pub, &prv, &frg, &lst, &ulen, &length);
        recUserLength = ulen; 
    }

    return bIsChatHeader;
}

//Method to check the value of the code bits for control headers. 
void codeCheck() {

    switch (code) {
        
        case CODE_SERVER_FULL:
            PRINT_MSG("Server is full\n");
            exit(EXIT_SUCCESS);
            break;

        case CODE_SERVER_NOT_FULL:
            max_ulen = ulen;
            PRINT_wARG("Choose a username (should be less than %d): ", max_ulen + 1); 
            break;

        case CODE_NEW_USER_CONNECT:
            bzero(buf, sizeof(buf));
            recv(clientSocket, buf, ulen, 0); 
            PRINT_USER_JOINED(buf);
            PRINT_CHAT_PROMPT;
            break;

        case CODE_USER_LEFT:
            bzero(buf, sizeof(buf));
            recv(clientSocket, buf, ulen, 0);
            PRINT_CHAT_PROMPT;
            PRINT_USER_LEFT(buf); 
            break;

        case CODE_USER_NEGO_MESS:
            code4();
            break;

        case CODE_MESS_TOO_LONG:
            PRINT_MSG("% Couldn't send message. It was too long.\n");
            break;

        case CODE_RECIP_NOT_ACTIVE:
            bzero(buf, sizeof(buf));
            recv(clientSocket, buf, ulen, 0);
            PRINT_wARG("%% Warning: user %s doesn't exist\n", buf);
            PRINT_CHAT_PROMPT;
            break;

        case CODE_BAD_MSG_FORMAT:
            PRINT_MSG("% Warning: Incorrectly formatted private message. Missing recipient.\n");
            PRINT_CHAT_PROMPT;
            break;
    }
}

//Username negotiation method for client
void negotiateCode1() {

    bzero(buf, sizeof(buf));
    bzero(username, sizeof(username)); 

    //If we are waiting on input from the client
    if(waitingOnInput) {

        //Client enters a username. Copy the data from buf into username array (This will block on select and automatically enter the data)
        read_stdin(buf, 20, &more);

        if(strlen(buf)-1 <= max_ulen) {

            buf[strcspn(buf, "\n")] = 0;
            strncpy(username, buf, strlen(buf));
            
            //Send array to the server to be checked 
            createAndSendHeader(clientSocket, 0, CODE_USER_NEGO_MESS, 0 ,strlen(buf));
            n = send(clientSocket, buf, strlen(buf), 0);
        } 
        
        //Else, prompt the client to enter a username (waiting on input will be set to one)
        else {

            waitingOnInput = 0;
            PRINT_wARG("Choose a username (should be less than %d): ", max_ulen + 1);
        }

        waitingOnInput = 0;
    } 
}

void createAndSendHeader(int clientSocket, uint8_t mt, uint8_t code, uint8_t unc, uint8_t ulen) {

    uint16_t header;
    create_control_header(&header, mt, code, unc, ulen);
    header = htons(header);
    send(clientSocket, &header, 2, 0); 
}

void sendChatHeader(int clientSocket, uint8_t mt, uint8_t pub, uint8_t prv, uint8_t frg, uint8_t lst, uint8_t ulen, uint16_t length){

    uint32_t chatHeader;
    create_chat_header(&chatHeader, mt, pub, prv, frg, lst, ulen, length);
    chatHeader = htonl(chatHeader);
    send(clientSocket, &chatHeader, sizeof(chatHeader), 0);
}

//Method to receive chat messages from the server 
void recieveChatMessage(){

    bzero(recUser, sizeof(recUser));
    bzero(msg, sizeof(msg)); 

    if(pub == 1 && prv == 0){

        recv(clientSocket, recUser, ulen, 0); 
        recv(clientSocket, msg, 4095, 0); 
        PRINT_PUBLIC_MSG(recUser, msg); 
        PRINT_CHAT_PROMPT;
    }

    else if(pub == 0 && prv == 1){

        recv(clientSocket, recUser, ulen, 0);
        recv(clientSocket, msg, length, 0); 
        PRINT_PRIVATE_MSG(username, recUser, msg); 
        PRINT_CHAT_PROMPT;
    } 
} 

//Simple method here to find the length of a recipient's username 
int getRecipientLength(char * message) {

    char * loc = strchr(message, '@');
    int length = 1;

    while(loc[length] != ' ' && loc[length] != '\0') {

        length++; 
        if(length >= max_ulen) {

            break;
        }
    }

    return length - 1;
}

//Sends initial fragment to the server. 
void sendInitalFragment(int clientSocket, char * recipient, char * bodyOfMessage, int recipientLength , int * index) {

    //We will first send a header indicating there are more fragments, followed by the message. 
    sendChatHeader(clientSocket, MT_CHAT, pub, prv, FRG_FRG, LST_MORE_FRGS, recipientLength, MAX_FRAGMENT_LEN); 
    send(clientSocket, recipient, recipientLength, 0); 

    char fragment[MAX_FRAGMENT_LEN]; 
    memcpy(fragment, bodyOfMessage + *index, MAX_FRAGMENT_LEN); 
    
    int fragLength = strlen(fragment);
    send(clientSocket, fragment, fragLength, 0); 
    *index += fragLength;
}

//Will send the rest of the fragments to the server. 
void sendRestOfFragments(int clientSocket, int divideBuffer, char * bodyOfMessage, int recipientLength, int * index) {

    //For the number of fragments, copy the next 255 characters of our message. 
    for(int i = 1; i <= divideBuffer; i++){

        char fragment[MAX_FRAGMENT_LEN];
        bzero(fragment, sizeof(fragment));
        memcpy(fragment, bodyOfMessage + *index, MAX_FRAGMENT_LEN); 

        int fragLength = strlen(fragment);

        //If we have remaining fragments, set LST_MORE_FRGS bit. Else, we will send our final header. 
        *index += fragLength; 
        if(fragLength == MAX_FRAGMENT_LEN){

            sendChatHeader(clientSocket, MT_CHAT, pub, prv, FRG_FRG, LST_MORE_FRGS, recipientLength, fragLength); 
        }
          
        else{

            sendChatHeader(clientSocket, MT_CHAT, pub, prv, FRG_FRG, LST_LAST_FRG, recipientLength, fragLength); 
        }
            
        //Send fragmented message. 
        send(clientSocket, fragment, fragLength, 0); 
    }
}

//For messages that do not need to be fragmented, we will simply find the length of our message and send the message. 
void sendUnfragmentedMessage(int clientSocket, char * bodyOfMessage, char * recipient, int isFragment, int recipientLength, int messageLength, int index){

    //Create a new array for the message. If the recipient exists and is a private message, indicate correct bits. Else, indicate public message. 
    char message[MAX_FRAGMENT_LEN]; 
    memcpy(message, bodyOfMessage + index, MAX_FRAGMENT_LEN); 

    if(recipient){

        sendChatHeader(clientSocket, 1, PUB_PRIV, PRV_PRV, isFragment, 0, recipientLength, messageLength);
        send(clientSocket, recipient, recipientLength, 0); 
    } 
    
    else{

        sendChatHeader(clientSocket, 1, PUB_PUB, PRV_NOT_PRIV, isFragment, 0, 0, strlen(message));
    }

    //Send unfragmented message. 
    send(clientSocket, bodyOfMessage, messageLength, 0);
}

//This method will send a message to the server 
void sendChatMessage(){

    //Prompt for user input. We will create an array for the length of the string the user entered. 
    bzero(buf, sizeof(buf));
    PRINT_CHAT_PROMPT;

    read_stdin(buf, 1000, &more); 

    buf[strcspn(buf, "\n")] = 0;

    uint8_t isFragment = 0;
    int messageLength = 0;
    int recipientLength = 0;
    char * bodyOfMessage = buf; 
    char * recipient = NULL; 

    //Check if this is a private message. 
    if(strchr(buf, '@') != NULL){

        recipientLength = getRecipientLength(buf);
        recipient = strchr(buf, '@') + 1;
        bodyOfMessage = strchr(buf, '@') + (recipientLength + 1);
        
        pub = 0; prv = 1;
        messageLength = strlen(bodyOfMessage); 
    } 
    
    else {
        
        messageLength = strlen(buf);
        pub = 1; prv = 0;
    }

    //If the message will need to be fragmented 
    int index = 0; 
    if(strlen(buf) > MAX_FRAGMENT_LEN){

        isFragment = 1; 
        int divideBuffer = strlen(buf) / MAX_FRAGMENT_LEN; 

        //Send the initial fragments, followed by the rest of the fragments to the server. 
        sendInitalFragment(clientSocket, recipient, bodyOfMessage, recipientLength, &index); 
        sendRestOfFragments(clientSocket, divideBuffer, bodyOfMessage, recipientLength, &index); 
    } 
    
    else {

        //For all unfragmented messages, simply send the message as it is. 
        sendUnfragmentedMessage(clientSocket, bodyOfMessage, recipient, isFragment, recipientLength, messageLength, index); 
    }
    
    waitingOnInput = 0;
}

//Print the correct prompt when code 4 is received from the control header (indicating invalid username)
void code4() {
    
    switch(unc) {

        case UNC_USERNAME_TIME_EXP:
            PRINT_MSG("Time to enter username has expired. Try again.\n");
            exit(EXIT_FAILURE);
            break; 

        case UNC_USERNAME_TAKEN:
            PRINT_MSG("Username is taken. Try again.\n");
            PRINT_wARG("Choose a username (should be less than %d): ", max_ulen + 1);
            break;
        
        case UNC_USERNAME_TOO_LONG:
            PRINT_MSG("Username is too long. Try again.\n");
            PRINT_wARG("Choose a username (should be less than %d): ", max_ulen + 1);
            break;

        case UNC_USERNAME_INVALID:
            PRINT_MSG("Username is invalid. Only alphanumerics are allowed. Try again.\n");
            PRINT_wARG("Choose a username (should be less than %d): ", max_ulen + 1);
            break;

        case UNC_USERNAME_ACCEPT:
            PRINT_MSG("Username accepted\n");
            bzero(buf, ulen);
            recv(clientSocket, buf, ulen, 0);
            acceptedUsername = 1;
            break;
            
        default: 
            PRINT_MSG("Time to enter username has expired. Try again.\n");
            exit(EXIT_FAILURE);
            break;
    }
} 

//Simple method to check the command line arguments
void checkCommandLineArguments(int argc, char *argv[]){

    if(argc != 3){ 

        PRINT_MSG("Invalid number of arguments.\n"); 
        exit(EXIT_FAILURE); 
    }
    
    if(gethostbyname(argv[1]) == NULL){

        PRINT_MSG("Invalid IP Address.\n");
        exit(EXIT_FAILURE);
    }

    if(sizeof(atoi(argv[2])) != sizeof(int) || atoi(argv[2]) < 1024 || atoi(argv[2]) > 65535){ 
        
        PRINT_MSG("Invalid port number.\n"); 
        exit(EXIT_FAILURE);
    }
}



