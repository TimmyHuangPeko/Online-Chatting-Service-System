Concept of client program:

	The communication between clients and server is based a user-defined 
application layer packet and header.
The user-defined header includes a 1-byte 'type', a 16-byte 'name', a 
8-byte 'time'.

 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6
+-+
| |	<- type
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|			  name				|
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|	  time		|
+-+-+-+-+-+-+-+-+

'type' specify the type of the application packet by its value:
	0: introduce the user name to either server or clients
	1: tell users that the content of packet is send by other user
	2: handle action of user sending packet to other users
'name' specify either the name of sender or receiver.
'time' specify the time that the sender or server send the packet.

	when client connect to the specific server with given IP address 
and port number(note that there exist default IP address and port number, 
and both of them are specify by value 0), it create a slave process for 
receiving packet from server(we'll discusss that later).
	When the client launch the program and connect to server with TCP
connection, it sends a 0-type packet which specify the name of user,
the server would send the packet to all the other users. On the other
hand, the server would multicast 0-type packet which is a contcatenation 
of user name and character '-' when the user disconnect with server.
	when user send a message via a 2-type packet to other users(through 
server), there are three conditions. first, if the given user isn't 
record in user list of the server, the server would echo the packet with
the name field contcatenates with character ' '. Then, if given user is 
off-line, ther server store the message and echo the packet with the 
name field contcatenates with character '-'. Finally, if the user is 
on-line, the server forward the message to the given user and echo the 
packet to the sender.

The program includes one master process and one slave process, the master 
process is responsible for inputing commands and sending packets to server. 
When user connect to server and and the 0-type packet, it fork the slave 
process to receive packets from server and show messages on the screen.
