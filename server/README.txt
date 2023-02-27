Concept of server program:

	The communication between clients and server is based a user-defined 
application layer packet and header, for more detail please read the 
README in the client directory.

	The server is manipulated with multiple threads, the master thread 
is responsible for waiting connection requests from clients, when a 
connection is established, server create a new thread to handle all the
communication with client, and thus achieve concurrency.

Equipment of server:
	The server is required to record all the users and the on-line state
of them, so a fixed table recording user's information is keep, and 
there would be a linklist data structure to record the state of all 
on-line users.
	The data structure of user table includes user name, the head of 
the FIFO queue that record all the off-line message, and a pointer 
referencing to the corresponding on-line state of the user recording 
in on-line state linklist if the user is on-line.
	The data structure of on-line state linklist includes user's name, 
the socket descriptor using to connect with client, the socket address 
of the user, a semaphore that control the synchronization of the socket, 
note that, as we record the socket address on the on-line state, the 
user can log into the server with different socket address, which 
provide availability.

Log in procedure:
	Whenever a user logs in the chat system, the server does a search on 
user list table using double hashing to find user's information, if the 
user is new to the chat system, it updates the table and records the 
information in the tuple(like create a new account for new user). Then, 
server updates the on-line state linklist and references the user 
information to on-line state. After that, checking the information tuple 
to see if there exist off-line message, if it does, transmit all of the 
off-line message to the client. Finally, multicast the type-0 packet 
to all the other on-line users through the on-line linklist, and now 
we can know the functionality of the on-line linklist, easily finding 
on-line user.

Forward messages from users:
	If the socket receive a type-2 packet, which means the corresponding
user want to send a message to others, server look for the given name on 
user table using double hashing to find the given information tuple and 
the corresponding on-line state, including three conditions. First, if 
the given user name is not found in the table, echos the packet with the 
name field contcatenates with a character ' '. Second, if the given user 
is off-line, records the message in the off-line message queue of the 
user information, and echos the packet with the name field contcatenates 
with a character '-'. Finally, if the given user is on-line, forward 
the packet with the corresponding socket descriptor.
	Note that server will tansfer the type of the packet from 2 to 1 
wiether before updating off-line message queue or forwarding packet 
through other socket descriptor, and every time the thread wants to 
forward packet through other threads socket, it should wait for the 
corresponding semaphore til it's value returns to 1. We can describe 
the semaphore as a gate to a phone booth, there should be only thread 
occupying the socket at the same time, which achieve synchornization.

Handle disconnection of client:
	Whenever the user disconnect with the server, the corrsponding 
socket should update the user table and on-line state at fast as 
possible to prevent other thread from using the socket, then follow 
the procedure by notifying other users with type-0 packet and the 
character '-' in the name field, closing the socket descriptor and 
terminating the thread.
