#include "proj.h"
#include "trie.h"

#define QLEN 60 
#define a_index(c) ((int)c - (int)'a')

struct Client{
    
    uint8_t playerID;
    uint8_t playerScore;
    int clientSocket; 
    struct sockaddr_in clientAddress; 
    socklen_t clientSize;
};

trie_node *createTrie(char *path);
void checkCommandLineArguments(int argc, char **argv); 
void acceptPairs(int serverSocket, struct Client * c1, struct Client * c2);
void handlePairs(int serverSocket, struct Client * c1, struct Client * c2);
void initClientStart(struct Client * client);
void handleRoundStart(struct Client * c1, struct Client * c2, uint8_t roundNumber, char * board);
void sendPlayerTurn(struct Client * c1, struct Client * c2, bool isPlayerOneTurn);
void endRound(struct Client * c1, struct Client * c2);
bool handlePlayerGuess(struct Client * c, char *,  char * board, trie_node *guessed);

uint8_t boardLength;
uint8_t secondsPerTurn;
trie_node *root;

int main(int argc, char **argv) {

    struct protoent *pointerToProtocolTable;
    struct sockaddr_in serverAddress, clientAddress;
    int serverSocket; 
    int portNumber, socketOption; 
    
    socklen_t clientSize; 
    char buffer[BUFFER_SIZE];

    memset((char *) &serverAddress, 0, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET; 
    serverAddress.sin_addr.s_addr = INADDR_ANY; 
    checkCommandLineArguments(argc, argv); 
    
    //Create a trie to store each word from our file 
    root = createTrie(argv[4]); 
    portNumber = atoi(argv[1]);
    
    //Convert port number to network short 
    if(portNumber > 0) { 

        serverAddress.sin_port = htons((u_short) portNumber);
    }

    else{ 
        
        fprintf(stderr, "Error: Bad port number %s\n", argv[1]); 
        exit(EXIT_FAILURE); 
    }

    //Return a pointer to the protocol table for TCP 
    if(((long int) (pointerToProtocolTable = getprotobyname("tcp"))) == 0){

        fprintf(stderr, "Error: Cannot map \"tcp\" to protocol number");
        exit(EXIT_FAILURE);
    }
    
    //Create a socket for the server. Return a file descriptor for protocol info 
    serverSocket = socket(PF_INET, SOCK_STREAM, pointerToProtocolTable -> p_proto);
    if(serverSocket < 0){ 

        perror("Socket creation failed"); 
        exit(EXIT_FAILURE); 
    } 
    
    //Assign socket options. We will assign options at the socket level, reuse this socket (for multiple clients) 
    if(setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &socketOption, sizeof(socketOption)) < 0){ 

        perror("Error setting socket option"); 
        exit(EXIT_FAILURE); 
    }
    
    //Create timeval structure to timeout clients 
    struct timeval timeout;
    timeout.tv_sec = secondsPerTurn;
    timeout.tv_usec = 0;
    
    //Bind the address of the server to the socket. Clients will not use bind, because they can send and receive 
    //data through any port (typically chosen by the kernel). Bind is required for listening for incoming connections 
    //on the same port. 
    if(bind(serverSocket, (struct sockaddr *) &serverAddress, sizeof(serverAddress)) < 0){

        perror("Bind failed"); 
        exit(EXIT_FAILURE); 
    }
    
    //Listen for incoming connections on this socket 
    if(listen(serverSocket, QLEN) < 0){

        perror("Listen failed"); 
        exit(EXIT_FAILURE);
    } 
    
    //Continuously listen for connections, accept pairs of clients 
    while(1){

        //Create client structures for client data 
        clientSize = sizeof(clientAddress);
        struct Client c1; 
        struct Client c2;

        c1.clientSize = clientSize; 
        c2.clientSize = clientSize;

        c1.playerID = 1; 
        c2.playerID = 2;

        c1.playerScore = 0;
        c2.playerScore = 0;

        //Accept two connections at once 
        acceptPairs(serverSocket, &c1, &c2); 
        
        //Once two clients are accepted, we will create a child process for running the game. 
        if(fork() == 0){

            //For both sockets, we will go ahead and set the socket option to SO_RCVTIMEO to timeout lingering 
            //clients who wait too long to enter data. Seconds is specified in timeout structure. 
            if(setsockopt(c1.clientSocket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout) < 0) {

                perror("SetSockOpt failed\n"); 
                exit(EXIT_FAILURE);
            }
            
            if(setsockopt(c2.clientSocket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout) < 0) {

                perror("SetSockOpt failed\n"); 
                exit(EXIT_FAILURE);
            }

            uint8_t roundNumber = 1;
            bool isPlayerOneTurn = true;

            //Main game loop here. 
            while(true) { 

                //Create a board for each round 
                char board[boardLength];

                //Create a seperate trie for each word guessed. (We will search this trie for already guessed words)
                trie_node *guessed = trie_create();

                //Handles the start of each round. Sends player scores, round number and board to each client. 
                handleRoundStart(&c1, &c2, roundNumber, board); 

                //If the round number is odd it is player one's turn. 
                isPlayerOneTurn = (roundNumber % 2 == 1); 

                //While loop for each round in our game 
                while (true) {  
                    
                    sendPlayerTurn(&c1, &c2, isPlayerOneTurn);

                    bzero(buffer, BUFFER_SIZE); 

                    //If it is player one's turn, we will send the correct data in client structure c1
                    if(isPlayerOneTurn) {
                        
                        //Handle player guess will receive the player guess from the client and determine if the word entered is valid. (If not valid, end round)
                        if(!handlePlayerGuess(&c1, buffer + 1, board, guessed)) { 
                            
                            //If the player has disconnected, close client sockets and 
                            if(errno == EINVAL) { 
                                close(c1.clientSocket);
                                close(c2.clientSocket);
                                exit(1);
                            }

                            //If word is not valid, end round 
                            endRound(&c1, &c2);
                            c2.playerScore++;
                            
                            break;
                        } 
                        
                        //Else, if the player's guess is valid 
                        else {

                            //Send a flag for the valid guess to the client 
                            uint8_t validGuessFlag = 1;
                            send(c1.clientSocket, &validGuessFlag, 1, 0);

                            //Guess length for the length of the player guess (copied from handle player guess into buffer) 
                            uint8_t guessLen = strlen(buffer + 1); 
                            buffer[0] = guessLen;
                            
                            //Send the guess that the player made to the inactive client (c2 if player one turn)
                            send(c2.clientSocket, buffer, guessLen + 1, 0); 
                        }  
                    } 
                    
                    //Else, if it is not player one's turn, we will handle player two guess on second client. 
                    else {
                        
                        if(!handlePlayerGuess(&c2, buffer + 1, board, guessed)) { 
                        
                            //Check if player disconnects
                            if(errno == EINVAL) { 
                                close(c1.clientSocket);
                                close(c2.clientSocket);
                                exit(1);
                            } 
                            
                            //If guess is not valid, end round
                            endRound(&c1, &c2);
                            c1.playerScore++;
                            
                            break;
                        } 

                        //Else, player two has a valid guess
                        else { 

                            uint8_t validGuessFlag = 1;
                            send(c2.clientSocket, &validGuessFlag, 1, 0);

                            //Guess length for strlen buffer (starts at + 1 for guess) and send the guess to the inactive client 
                            uint8_t guessLen = strlen(buffer + 1); 
                            buffer[0] = guessLen;
                            
                            send(c1.clientSocket, buffer, guessLen + 1, 0); 
                        }
                    }

                    //Swap turns and increment round number 
                    isPlayerOneTurn = !isPlayerOneTurn;
                }
                
                roundNumber++;
            }
        } 

        else {
            
            //For the parent process, we will close both sockets when the game ends. 
            int c1sock = c1.clientSocket;
            int c2sock = c2.clientSocket;
            close(c1sock);
            close(c2sock); 
        }
    }
}

//Checks our trie to see if the word is a valid guess and has already been guessed 
bool validatePlayerGuess(char * playerGuess, size_t length,  char * board, trie_node *guessed) {
    
    //If we find our guess has already been inserted into our trie (for guessed words only). We will return false. 
    if(trie_search(guessed, playerGuess, length)) {

        return false;
    }

    //Otherwise, insert the guess into our trie. 
    trie_insert(guessed, playerGuess, length);
    int boardCharCount[26] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}; 

    //Create a simple array for counting the number of occurences of each letter for our board 
    for (int i = 0; i < strlen(board); i++) {

        int z = a_index(board[i]);
        boardCharCount[z]++;
    }

    //Check to make sure the player guess contains the letters used for our board and does not exceed the number of occurrences. 
    for(int i = 0; i < length - 1; i++) {

        int z = a_index(playerGuess[i]);

        if (boardCharCount[z] <= 0) {

            return false;
        }

        boardCharCount[z]--;
    }

    //If we find the word in our trie, return true. 
    if (trie_search(root, playerGuess, length)){

        return true;
    }
    
    else{

        return false;
    }
}

//Handles each guess made by the player. 
bool handlePlayerGuess(struct Client * c, char * buf,  char * board, trie_node *guessed) {

    bzero(buf - 1, BUFFER_SIZE); 

    //First call from recv will be the guess length 
    uint8_t guessLength; 
    int n = recv(c->clientSocket, &guessLength, 1, 0);

    if(n <= 0) {

       return false; 
    }

    //Create character array for the guess length. Then, receive the guess from the player
    char playerGuess[guessLength];  
    recv(c->clientSocket, playerGuess, guessLength, 0);
    
    playerGuess[guessLength] = '\0'; 
    
    //Copy the player guess into a seperate buffer. (Used to display what the player guessed for the inactive client. Plus one for null terminator) 
    memcpy(buf, playerGuess, guessLength + 1); 

    return validatePlayerGuess(playerGuess, guessLength, board, guessed);
}

//Send a zero flag to each client for the end of each round 
void endRound(struct Client * c1, struct Client * c2) {
    
    uint8_t zero = 0;
    send(c1->clientSocket, &zero, 1, 0);
    send(c2->clientSocket, &zero, 1, 0);
}

//Method to send the correct data, depending on whose turn it is. 
void sendPlayerTurn(struct Client * c1, struct Client * c2, bool isPlayerOneTurn) {
    
    //If client receives 'Y' it is their turn
    char * y = "Y";
    char * n = "N";

    if(isPlayerOneTurn) {

        send(c1->clientSocket, y, 1, 0);
        send(c2->clientSocket, n, 1, 0);
    } 
    
    else {

        send(c1->clientSocket, n, 1, 0);
        send(c2->clientSocket, y, 1, 0);
    }
}

//Initiate each client by sending player id, board length and seconds per turn  
void initClientStart(struct Client * client) {
    
    char buf[32];
    buf[0] = client->playerID;
    buf[1] = boardLength;
    buf[2] = secondsPerTurn;

    send(client->clientSocket, &buf, 3, 0); 
}

//This method will generate a random board for our game 
void generateRandomBoard(char* board) {

    //Pick a random vowel if none were chosen on board setup
    char *vowels = "aeiou";
    int bHasVowel = 0;

    time_t timeN = time(NULL);
    srandom((unsigned int )timeN);

    //For the length of the board we will select a random (lowercase) letter. If no vowels were 
    //chosen, we will pick one at random. 
    for(int i = 0; i < boardLength - 1; i++) { 

        board[i] = (random() % 26) + 97; 
        
        for(int j = 0; j < 5 && !bHasVowel; j++) { 

            if(board[i] == vowels[j]) {

                bHasVowel = 1;
            }
        } 
    }

    //If no vowel has been found, select a random vowel from our string
    if(!bHasVowel) {

        board[boardLength - 1] = vowels[(random() % 5)]; 
    } 
    
    //Otherwise, the last character of our board will be random 
    else {

        board[boardLength - 1] = (random() % 26) + 97;
    }

    //Null terminate board for safe keeping 
    board[boardLength] = '\0'; 
}

//Handles the start of each round. Generates a random board and sends player info
void handleRoundStart(struct Client * c1, struct Client * c2, uint8_t roundNumber, char * board) {

    //Generate a random board at the start of each round. 
    generateRandomBoard(board); 
    char buf[3 + boardLength]; 
    
    //Assign player scores and round number to buffer 
    buf[0] = c1->playerScore;
    buf[1] = c2->playerScore;
    buf[2] = roundNumber;

    //Copy the board into buffer
    for(int i = 0; i < boardLength; i++) {
        
        buf[3 + i] = board[i]; 
    }

    /*
        Send data. Data could be something like "237fjweia"
        Where, player1Score = 2, player2Score = 3, round = 7, board = "fjweia"
    */
    send(c1->clientSocket, buf, (3 + boardLength), 0);
    send(c2->clientSocket, buf, (3 + boardLength), 0);
}

//Accept pairs method will block until two clients connect
void acceptPairs(int serverSocket, struct Client * c1, struct Client * c2) {

    //Assign the socket for pairs of clients and assign data in init client start. 
    c1->clientSocket = accept(serverSocket, (struct sockaddr *) &c1->clientAddress, &c1->clientSize); //blocking
    if(c1->clientSocket < 0) {

        fprintf(stderr, "Error: Accept failed\n"); exit(EXIT_FAILURE);
    }

    initClientStart(c1);

    c2->clientSocket = accept(serverSocket, (struct sockaddr *) &c2->clientAddress, &c2->clientSize); //blocking
    if(c2->clientSocket < 0) {

        fprintf(stderr, "Error: Accept failed\n"); exit(EXIT_FAILURE);
    }

    initClientStart(c2);
}

//Simple method to check the command line arguments of our program. 
void checkCommandLineArguments(int argc, char **argv){

    if (argc != 5) {

        PRINT_MSG("Invalid number of arguments.\n");
        exit(EXIT_FAILURE);
    }

    char *boardptr;
    if (sizeof(atoi(argv[1])) != sizeof(int)) {

        PRINT_MSG("Invalid port number.\n");
        exit(EXIT_FAILURE);
    }

    boardLength = strtoul(argv[2], &boardptr, 10);
    if (atoi(argv[2]) < 1 || atoi(argv[2]) > 255) {

        PRINT_MSG("Invalid board length.\n");
        exit(EXIT_FAILURE);
    }

    secondsPerTurn = strtoul(argv[3], &boardptr, 10);
    if (atoi(argv[3]) < 1 || atoi(argv[3]) > 255) {

        PRINT_MSG("Invalid seconds per turn.\n");
        exit(EXIT_FAILURE);
    } 
}

//Method to create a trie given the words in our dictionary path 
trie_node *createTrie(char *path){

    trie_node *root = trie_create();

    FILE *filePointer; 
    char *lineBuffer;
    size_t length = 0;
    ssize_t read;

    //File pointer for read permissions for our file
    if((filePointer = fopen(path, "r")) == NULL){

        printf("No such file found. SYSTEM_EXIT \n");
        exit(EXIT_FAILURE); 
    }
    
    //Read each line of the file and add that word to our trie 
    while((read = getline(&lineBuffer, &length, filePointer)) != -1){
    
      trie_insert(root, lineBuffer, strlen(lineBuffer) - 1);   
    }
    
    if(lineBuffer){

        free(lineBuffer);
    } 

    fclose(filePointer); 
    return root;
}


