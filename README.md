CMPT 332\
Assignment 4\
October December 6, 2021\
Heather Caswell | Trang Nguyen\
hec626 | len054

# Multicast Messaging
A network includes a server and multiple senders and receivers. Whenever a sender sends a message, the server would send the message to every receiver. While communicating with multiple concurrent senders and receivers, the server spontaneously handles new connection.

### Launch
- Use "make run" to compile
- to run the program use "./server" to start the server,\
    "./sender <hostname> 33490" to start a sender,\
    "./client <hostname> 34950" to start a receiver
- Use "make clean" to remove executables when finished

### Summary of functionality
-sender.c: once initiated can send a message to all of the receivers available. Multiple senders can be active at once and they are all able to send to the receivers.\
-client.c: displays a message sent from any sender, along with where the message came from.\
-server.c: Connects to the senders and receivers to facilitate the sending of messages.\
Most important functions:
  
    void *sendToRev(void *arg)
        arg: contains the socket to connect to and the socket to send to
        -sends the message from sender to the receiver client
    
    void *revFromSender(void *arg)
        arg: contains the socket to connect to and the socket to send to
        -gets the message from sender and saves it in buffer to be sent to the receiver client

### Tests run
- tested if server can handle >1 sender.
- tested if server will send messages to >1 receiver.
- tested closing the senders and receivers.
- tested closing the server.

### Known issues: 
- new client is receiving the most recent old message
- when the server is shut off, the senders and receivers continue running.

