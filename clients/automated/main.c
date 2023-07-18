// Preprocessing
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h> // for ip_mreq
#include <unistd.h>     // close function

// Statics
#define LISTENER_PORT 2525
#define LISTENER_INTERFACE_IP "192.168.1.115"
#define LISTENER_MCAST_GROUP "239.11.22.33"
#define

// Declare function prototypes

// Main function
int main(void)
{
    // Create socket descriptior
    int socket_desc;

    // Declare server and client addresses
    struct sockaddr_in server_addr, client_addr;

    // Declare buffers
    char server_message[100], client_message[100];

    // Declare length of the destination address struct
    unsigned int client_struct_length = sizeof(client_addr);

    // Clean buffers before using
    memset(server_message, '\0', sizeof(server_message));
    memset(client_message, '\0', sizeof(client_message));

    // Create UDP socket
    socket_desc = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    // Check if socket was created successfully. If not, exit the program
    if (socket_desc < 0)
    {
        printf("Error while creating socket\n");
        return 1;
    }
    printf("Socket created successfully\n");

    // Set port and IP of receiver, where traffic is listened to
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(LISTENER_PORT);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    // Bind to the set port and IP:
    if (bind(socket_desc, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        printf("Couldn't bind to the port\n");
        return 2;
    }
    printf("Done with binding of %i/%d at %s\n", LISTENER_PORT, IPPROTO_UDP, LISTENER_INTERFACE_IP);

    // Join multicast group
    struct ip_mreq mreq;
    mreq.imr_multiaddr.s_addr = inet_addr(LISTENER_MCAST_GROUP);
    mreq.imr_interface.s_addr = inet_addr(LISTENER_INTERFACE_IP);

    printf("Joining multicast group %s at %s...\n", LISTENER_MCAST_GROUP, LISTENER_INTERFACE_IP);
    if (setsockopt(socket_desc, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)&mreq, sizeof(mreq)) < 0)
    {
        printf("Couldn't join multicast group\n");
        return 3;
    }

    printf("Listening for incoming messages...\n\n");

    // Receive client's message:
    if (recvfrom(socket_desc, client_message, sizeof(client_message), 0,
                 (struct sockaddr *)&client_addr, &client_struct_length) < 0)
    {
        printf("Couldn't receive\n");
        return 4;
    }
    printf("Received message from IP: %s and port: %i\n",
           inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

    printf("Msg from client: %s\n", client_message);

    // Change to uppercase:
    for (int i = 0; client_message[i]; ++i)
        client_message[i] = toupper(client_message[i]);

    // Respond to client:
    strcpy(server_message, client_message);

    if (sendto(socket_desc, server_message, strlen(server_message), 0,
               (struct sockaddr *)&client_addr, client_struct_length) < 0)
    {
        printf("Can't send\n");
        return 5;
    }

    // Close the socket:
    close(socket_desc);
}
