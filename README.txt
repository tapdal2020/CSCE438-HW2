Compilation:

To compile the program, navigate to the root project directory in a bash terminal and type 'make' or 'make all'

Execution:

The server (crsd) should be running before the client is started so the client will be able to connect to the server.

1) In order to run the server, navigate to the root project directory in a bash terminal and type the command './crsd <port #>' using a valid, open port number.
   
   NOTE: The port number passed as an argument to the server is only a REQUEST for that particular port. If the port isn't available, 
         the server will bind to the smallest available  port >= the given port # (to avoid crashes if the server could not bind to 
         the port provided). Once started, the server will output through the command line which port it was able to bind to. 

2) To run the client, first start up the server and then start the client with the command './crc <server ip> <port #>' where <server ip> is the network address (in dotted decimal notation)
   of the server, for example '127.0.0.1' should be used if the client and server are running on the same machine. <port #> is the port number that the server has bound to.
