# Socket-DB
Socket-DB is a lightweight client-server database system written in C, designed to demonstrate network programming and custom database management over TCP/IP sockets. It supports basic SQL-like operations (CREATE, INSERT, SELECT, UPDATE, DELETE, SAVE, LOAD) with remote table synchronization between client(Windows) and server(Linux). Server-side coded as multi-threaded with checking race conditions and handling over requests.

Features;

Client-Server Architecture using TCP sockets

Cross-platform support (Linux & Windows)

Custom Database Engine in C with binary file storage

Remote Table Synchronization (client can request tables from server, store(save) it to server)

Lightweight, Fast, No External Dependencies

SQL-like commands:

CREATE – Create tables.

INSERT – Insert rows.

SELECT – Query rows.

UPDATE – Modify existing rows.

DELETE – Remove rows.

SAVE – Save table to local disk and save it to server.

LOAD – Load table from local disk or remote server.

LIST – List available tables on the server.

DROP - Drops the table.

Tech Stack;

Language: C (C99 Standard)

Networking: TCP/IP sockets

Operating Systems: Linux Mint (server), Windows 10 (client)

Build Tools: GCC, MinGW.

To use and test it,

You need to enter your server ip address on client-side, receiver.c and sender.c as a string.

To compile on Windows;

gcc main.c functions.c sender.c receiver.c -o client.exe -lws2_32

To compile on Linux;

gcc server.c -o server -lpthread

Future Improvements;

Writing my own B-Tree to access faster to files on storage.
User authentication.
Known Issues;

If you select the attribute value as 'string' but enter different type of data type it returns garbage value.
