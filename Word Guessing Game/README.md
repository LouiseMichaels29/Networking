# Word Guessing Game
This program implementes a Trie to create a word guessing game in a client server model. It takes two clients to initialize each game, upon which an arbitrary number of clients 
are allowed. 
# Usage 
Simply clone the repository or copy all files into the same directory. Run the make command on your desired command line interface (or gcc command), and simply run the executable. 
Command line arguments (argv[1], argv[2], argv[3], argv[4]) will require port number, board size, seconds per round and a filename ("twl06.txt), for the server executable, 
respectfully. Command line arguments (argv[1], argv[2]), will require the server address and port number, for the client executable, respectfully. 
#### The values associated with the server file: 
- Port numbers must contain numbers between 1024 and 65,535.
- The board size for the number of characters used to guess a word. 
- The number of seconds per round before a client ends their turn. 
- The path to the word dictionary (twl06.txt). 
#### The values associated with the client file: 
- Server address must contain the same IP address the server is running. (127.0.0.1 for local)
- The port number in which the server executable is running.
# What I Learned
- Sockets are used as endpoints for communication in networking. 
- Tries are useful data structures for storing strings with common prefixes. 
