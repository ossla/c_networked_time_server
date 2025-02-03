#if defined(_WIN32)
/* WIN */
#ifndef _WIN32_WINNT /* defined for the Winsock headers to provide all the functions we neeed. */
#define _WIN32_WINNT 0x0600
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib") /* this tells the Microsoft Visual C compiler to 
                                link your program against the Winsock library, ws2_32.lib 
                                
                                IF using MinGW on Windows, use -lws2_32 flag with gcc 
                                (to link library ws2_32.lib) */
#else
/* UNIX */
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#endif

#if defined (_WIN32)
#define CLOSESOCKET(s) closesocket(s)
#define ISVALIDSOCKET(s) ((s) != INVALID_SOCKET)
#define GETSOCKETERRNO() (WSAGetLastError())
#else
#define CLOSESOCKET(s) close(s)
#define ISVALIDSOCKET(s) ((s) >= 0)
#define SOCKET int
#define GETSOCKETERRNO() (errno)
#endif
#include <stdio.h>
#include <string.h>
#include <time.h>

/*
int getaddrinfo(const char* restrict node,
                const char* restrict service,
                const struct addrinfo* restrict hints,
                struct addrinfo** restrict res);

struct addrinfo {
    int ai_flags;         // Flags for options (e.g., passive socket)
    int ai_family;        // Address family (AF_INET for IPv4, AF_INET6 for IPv6)
    int ai_socktype;      // Socket type (SOCK_STREAM for TCP, SOCK_DGRAM for UDP)
    int ai_protocol;      // Protocol (usually 0 for default)
    size_t ai_addrlen;    // Size of ai_addr
    struct sockaddr *ai_addr;  // Pointer to address structure (IP & port)
    char *ai_canonname;   // Canonical (official) hostname
    struct addrinfo *ai_next;  // Pointer to next structure in linked list
};
*/

int main() {
    #if defined(_WIN32)
    WSADATA d;
    if (WSAStartup(MAKEWORD(2, 2), &d)) { /* initializing Winsock
                        the MAKEWORD marco allows us to request Winsock version 2.2 */
        fprintf(stderr, "Failed to initialize.\n");
        return 1;
    }
    #endif


    printf("Configuring local address...\n");
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));

    hints.ai_family = AF_INET; /* specifies that we are looking for an IPv4 address */
    hints.ai_socktype = SOCK_STREAM; /* SOCK_STREAM indicates, that we are going to be using TCP
                                        SOCK_DGRAM would be used if we were doing UDP server instead */
    hints.ai_flags = AI_PASSIVE; /* telling getaddrinfo we want it to bind to the wildcard address.
    We are asking getaddrinfo() to set up the address 
    so we listen on any available network interface */


    struct addrinfo* bind_address; /* holds return information of getaddrinfo() */
    getaddrinfo(0, "8080", &hints, &bind_address); /*getaddrinfo() returns one or more addrinfo structures,
                                                    each of which contains an Internet address that can
                                                    be specified in a call to bind(2) or connect(2).
    For our purpose, it generates an address that's suitable for bind(). 
    To make it generate this, we must pass in the first parameter as NULL and 
    have the AI_PASSIVE flag set in hints.ai_flags. */


    printf("Creating socket...\n");
    SOCKET socket_listen;
    socket_listen = socket(bind_address->ai_family,
                        bind_address->ai_socktype, bind_address->ai_protocol);
    /* we call the socket() function to generate the actual socket */
    /* the reason we used getaddrinfo() before called socket() is that
    we can now pass in parts of bind_address as the arguments to socket().
    This makes it very easy to change our program's protocol 
    without needing a major rewrite */

    if (!ISVALIDSOCKET(socket_listen)) {
        fprintf(stderr, "socket() failed. (%d)\n", GETSOCKETERRNO());
        return 1;
    }


    printf("Binding socket to local address...\n");
    if (bind(socket_listen,
            bind_address->ai_addr, bind_address->ai_addrlen)) {
        fprintf(stderr, "bind() failed. (%d)\n", GETSOCKETERRNO());
        return 1;
    }
    freeaddrinfo(bind_address);


    printf("Listening...\n");
    /*  The second argument to listen(), which is 10 in this case, 
          tells listen() how many connections it is allowed to queue up. */ 
    /*  If many clients are connecting to our server all at once,
          and we aren't dealing with them fast enough, 
          then the operating system begins to queue up these incoming connections. */
    if (listen(socket_listen, 10) < 0) {
        fprintf(stderr, "listen() failed. (%d)\n", GETSOCKETERRNO());
        return 1;
    }


    printf("Waiting for connection...\n");
    struct sockaddr_storage client_address; /* stores addr info for connecting client (fills in accept()) */
    socklen_t client_len = sizeof(client_address); /* differs when IPv4 and IPv6 (fills in accept()) */
    /*  
        Calling to accept() will block the program until 
            a new connection is made to the listening socket (socket_listen).
        To continue, when the new connection is made, accept() will create a NEW 
            socket for it.
        Original socket continues listen for new connections, but the new socket 
            returned by accept() can be used to send and receive data over the newly 
            established connection.
        accept() also fills the address information of the connected client
    */
    SOCKET socket_client = accept(socket_listen,
                            (struct sockaddr*) &client_address, &client_len);
    if (!ISVALIDSOCKET(socket_client)) {
        fprintf(stderr, "accept() failed. (%d)\n", GETSOCKETERRNO());
        return 1;
    }

    /* Printing client's address to the console: */
    printf("Client is connected... ");
    char address_buffer[100];
    getnameinfo((struct sockaddr*)&client_address,
                client_len, address_buffer, sizeof(address_buffer), 0, 0, 
                NI_NUMERICHOST);
    printf("%s\n", address_buffer);


    printf("Reading request...\n");
    char request[1024];
    int bytes_received = recv(socket_client, request, 1024, 0); /* (client socket; request buffer; buffer size) */
    printf("Received %d bytes.\n", bytes_received);
    if (bytes_received < 1) {
      printf("Connection terminated by the client. ");
      return 1;   
    }
    /* we just ignore request altogether since our program just tells us what time it is. */
 

    printf("%.*s", bytes_received, request);


    printf("Sending response...\n");
    const char *response =
        "HTTP/1.1 200 OK\r\n"
        "Connection: close\r\n"
        "Content-Type: text/plain\r\n\r\n"
        "Local time is: ";
    int bytes_sent = send(socket_client, response, strlen(response), 0);
    printf("Sent %d of %d bytes.\n", bytes_sent, (int)strlen(response));

    time_t timer;
    time(&timer);
    char *time_msg = ctime(&timer);
    bytes_sent = send(socket_client, time_msg, strlen(time_msg), 0);
    printf("Sent %d of %d bytes.\n", bytes_sent, (int)strlen(time_msg));

    printf("Closing connection...\n");
    CLOSESOCKET(socket_client);

    printf("Closing listening socket...\n");
    CLOSESOCKET(socket_listen);

    #if defined(_WIN32) // for windows, to clean up Winsock
        WSACleanup();
    #endif

    printf("Finished.\n");

    return 0;
}
