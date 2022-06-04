#include "proj.h" 

void checkCommandLineArguments(int argc, char **argv);
void startGame(int clientSocket, char buffer[]);
void handleRounds(int clientSocket, char buffer[]);
bool handleTurns(int clientSocket, char buffer[], int more);
void endGame(uint8_t playerOneScore, uint8_t playerTwoScore);

uint8_t boardLength;
uint8_t playerID;

int main(int argc, char **argv) {
    
    struct hostent *pointerToHostTable;
    struct protoent *pointerToProtocolTable;
    struct sockaddr_in serverAddress; 
    uint16_t port; char *host; int more = 1;
    int clientSocket;
    char buffer[1024];
    checkCommandLineArguments(argc, argv);
    
    memset((char *) &serverAddress, 0, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;

    //Convert host to network short for the port number (since network byte order 
    //is always big endian. But operating systems can be either big or little endian)
    port = atoi(argv[2]);
    if (port > 0) {

        serverAddress.sin_port = htons(port);
    }

    else {

        fprintf(stderr, "Error: bad port number %s\n", argv[2]);
        exit(EXIT_FAILURE);
    }
    
    //Get the host information for the port number. The list of fields identified by the structure hostent 
    host = argv[1];
    pointerToHostTable = gethostbyname(host);
    if (pointerToHostTable == NULL) {

        fprintf(stderr, "Error: Invalid host: %s\n", host);
        exit(EXIT_FAILURE);
    }

    //Two different structures used here. Host table will automatically identify the fields for the 
    //internet address port number. (Used for host lookup). Sockaddr_in is used to store internet addresses. 
    memcpy(&serverAddress.sin_addr, pointerToHostTable->h_addr, pointerToHostTable->h_length);
    if (((long int)(pointerToProtocolTable = getprotobyname("tcp"))) == 0) {

        perror("Error: Cannot map \"tcp\" to protocol number");
        exit(EXIT_FAILURE);
    }

    //Create a socket for the client. Assign the protocol info and return a file descriptor. 
    clientSocket = socket(PF_INET, SOCK_STREAM, pointerToProtocolTable->p_proto);
    if (clientSocket < 0) {

        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    //Connect the client socket with the server address (including protocol information)
    if (connect(clientSocket, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0) {

        perror("Connect failed");
        exit(EXIT_FAILURE);
    }

    //Start game method handles sending and receiving data for each game 
    startGame(clientSocket, buffer); 

    //While loop to handle rounds of each game
    while(true){

        handleRounds(clientSocket, buffer); 

        //While loop to handle game turns 
        while(true){

            if(!handleTurns(clientSocket, buffer, more)) {

                break;
            }
        }   
    }
    
    close(clientSocket);
    exit(EXIT_SUCCESS);
}

//Simple method to end the game for our program 
void endGame(uint8_t playerOneScore, uint8_t playerTwoScore){

    //If either player one score or player two score is greater than three, display win / lose message and exit. 
    if(playerOneScore >= 3){

        if(playerID == 1){

            PRINT_MSG("You won!\n");
        }

        else{

            PRINT_MSG("You lost!\n");
        }
    }

    else if(playerTwoScore >= 3){

        if(playerID == 2){

            PRINT_MSG("You won!\n");
        }

        else{

            PRINT_MSG("You lost!\n");
        }
    }

    exit(EXIT_SUCCESS);
}

//Method to handle each turn in our game 
bool handleTurns(int clientSocket, char buffer[], int more){ 

    //Empty our buffer and receive 'Y' or 'N'. (Client who receives 'Y' is their turn)
    bzero(buffer, BUFFER_SIZE);
    int n = recv(clientSocket, buffer, 1, 0);
    
    if(n <= 0) {

        exit(EXIT_FAILURE);
    }
    
    char playerTurn = buffer[0]; 
    bzero(buffer, BUFFER_SIZE);

    //If it is the player's turn, we will prompt for user input. 
    if(playerTurn == 'Y'){

        PRINT_MSG("Your turn, enter word: "); 
        read_stdin(buffer, 1024, &more); 
        PRINT_wARG("You entered: %s", buffer); 
        uint8_t wordLength = strlen(buffer) - 1; 

        char word[wordLength];
        strcpy(word, buffer);

        //Send the length of the player's guess and the guess itself to the server 
        send(clientSocket, &wordLength, 1, 0);
        send(clientSocket, buffer, wordLength, 0);

        //Receive whether the guess is valid or not. Flag will be one or zero. 
        n = recv(clientSocket, buffer, 1, 0);
        
        if(n <= 0) {

            exit(EXIT_FAILURE);
        }

        //If flag is zero, invalid guess. 
        if(buffer[0] == 0) {

            PRINT_MSG("Invalid word!\n");
            return false; 
        } 

        else {
            
            PRINT_MSG("Valid word!\n");
            return true;
        }
    } 

    //Else, wait for the opponent to enter a word. 
    else{

        //Recv guess length and word and display for client 
        PRINT_MSG("Please wait for opponent to enter word...\n"); 
        recv(clientSocket, buffer, 1, 0);
            
        uint8_t guessLen = buffer[0];
        if(guessLen == 0) { 
            
            PRINT_MSG("Opponent lost the round!\n");
            return false;
        }

        recv(clientSocket, buffer, guessLen, 0); 
        PRINT_wARG("Opponent entered \"%s\"\n", buffer); 
        return true;
    } 
}

//Handles the game rounds. 
void handleRounds(int clientSocket, char buffer[]){

    //Empty buffer and recieve our board and player scores for the start of each round 
    bzero(buffer, BUFFER_SIZE);
    int n = recv(clientSocket, buffer, (boardLength + 3), 0);
    
    if(n <= 0) {

        exit(EXIT_FAILURE);
    }

    //Assign values for player scores and round number from recv
    uint8_t playerOneScore = buffer[0];
    uint8_t playerTwoScore = buffer[1]; 
    uint8_t roundNumber = buffer[2]; 

    //Check to see if a player has won the game 
    if(playerOneScore >= 3 || playerTwoScore >= 3){

        endGame(playerOneScore, playerTwoScore);
    }

    PRINT_wARG("\nRound %u...\n", roundNumber); 
    PRINT_wARG("Score is %u-%u\n", playerOneScore, playerTwoScore);

    //Assign the board values from our buffer. Then, print board characters. 
    char board[boardLength]; 
    for(int i = 0; i < boardLength; i++){

        board[i] = buffer[3 + i]; 
    }
    
    PRINT_MSG("Board: "); 
    for(int i = 0; i < boardLength; i++){

        PRINT_wARG("%c ", board[i]);
    }

    PRINT_MSG("\n"); 
}

//Start game method will handle the start of the game (player scores, board size and seconds per turn)
void startGame(int clientSocket, char buffer[]){

    //Empty buffer and receive data. Assign values to player IDs, seconds and board length. 
    bzero(buffer, BUFFER_SIZE);
    recv(clientSocket, buffer, 3, 0);
    uint8_t secondsPerRound = buffer[2];
    playerID = buffer[0];
    boardLength = buffer[1];
    
    //Prompt text for player one and player two
    if(playerID == 1){

        PRINT_wARG("You are Player %d... the game will begin when Player 2 joins...\n", playerID);
    } 
    
    else{

        PRINT_wARG("You are Player %d...\n", playerID);
    }

    PRINT_wARG("Board size: %u\n", boardLength);
    PRINT_wARG("Seconds per turn: %d\n", secondsPerRound); 
}

//Simple method to check command line arguments with program 
void checkCommandLineArguments(int argc, char **argv){

    if(argc != 3){ 

        PRINT_MSG("Invalid number of arguments.\n"); 
        exit(EXIT_FAILURE); 
    }
    
    if(gethostbyname(argv[1]) == NULL){

        PRINT_MSG("Invalid IP Address.\n");
        exit(EXIT_FAILURE);
    }

    if(sizeof(atoi(argv[2])) != sizeof(int)){ 

        PRINT_MSG("Invalid port number.\n"); 
        exit(EXIT_FAILURE);
    }
}
