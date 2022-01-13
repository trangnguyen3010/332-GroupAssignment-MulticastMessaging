# Multicast Messaging
A network includes a server and multiple senders and receivers. Whenever a sender sends a message, the server would send the message to every receiver. While communicating with multiple concurrent senders and receivers, the server spontaneously handles new connection.

### Launch
In the terminal:
make run
./server
./sender <hostname> <portnumber>
./rev <hostname> <portnumber>
