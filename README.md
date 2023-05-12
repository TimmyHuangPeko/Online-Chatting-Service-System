# Online-Chatting-Service-System
The Homework Project for Network Application Programming (in NSYSU)

In this project, the server provides an online chat service, and multiple clients can send messages via the service provided by the server. The functions provided by the service are listed below:

* **User Login and Notification:** When a client wants to send messages to another client, it must first log in to the server. The server maintains the mapping between the user name and its socket address. When a client logs in or out, all other active clients will be notified.
* **Unicast Communication:** Two clients can send messages to each other. The server is responsible for relaying messages between them.
* **Multicast Communication:** A client can send messages to multiple clients simultaneously.
* **Offline Messaging:** An active client can send offline messages to an inactive client. When the latter becomes active again (i.e., logs in to the server again), these messages will be sent to that client.

The server is concurrent and developed with multithreading. The client is developed with multiple processes (using the function fork()).

For more details on implementation, please refer to the README file located in the [server](./server/README.txt) and [client](./client/README.txt) directories.
