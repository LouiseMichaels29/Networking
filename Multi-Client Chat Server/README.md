# Multi-Client Chat Server
This program simulates a multi-client chat server using tcp headers to exchange information in a client server model. 
# Usage
Simply clone the repository or copy all files into the same directory. Run the make command on your desired command line interface (or gcc command), and simply run the executable.
Command line arguments (argv[1], argv[2], argv[3], argv[4]) will require port number, maximum number of clients, timeout given to a client to respond before disconnecting, and 
the maximum length of a username, for the server executable, respectfully. Command line arguments (argv[1], argv[2]) will require the server address and port number, for the 
client executable, respectfully.  
##### The values associated with the server file: 
- Port numbers must contain numbers between 1024 and 65,535. 
- The maximum number of clients between the values of 1 and 256. 
- The timeout length between 1 and 300. 
- The maximum length of a username between 1 and 10.  
##### The values associated with the client file: 
- Server address must contain the same IP address the server is running. (127.0.0.1 for local)
- The port number in which the server executable is running. 
# What I Learned
- The select command in C can be used to exchange information between any number of clients interacting with the same server. 
- The client server model is the foreground for the current networking and information exchange system that is used today. 
