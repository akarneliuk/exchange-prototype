// Preprocessing
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>     // used for close function
#include <sys/socket.h> // used for socket functions
#include <arpa/inet.h>  // used for inet_addr function
#include <time.h>       // used for timestamp

// Local code
#include "comm.h"

// Define function prototypes
uint64_t send_order(char *client_id, order_t *order, server_t *server)
{
    // Serialize order before sending
    char *str_order = malloc(MAX_MSG_LEN * sizeof(char));
    sprintf(
        str_order, "%s:%lu:%lu:%s:%lu:%.2f",
        client_id,
        order->t_client,
        order->operation,
        order->symbol,
        order->quantity,
        order->price);

    // Initialize socket
    int sd = socket(AF_INET, SOCK_STREAM, server->protocol);
    if (sd < 0)
    {
        printf("%lu: Unable to create socket\n", time(NULL));
        return 10;
    }
    printf("%lu: Socket created successfully\n", time(NULL));

    // Initialize message buffer
    char server_message[MAX_MSG_LEN], client_message[MAX_MSG_LEN];
    memset(server_message, '\0', sizeof(server_message));
    memset(client_message, '\0', sizeof(client_message));

    // Initialize server address (Destination IP and port)
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server->port);
    server_addr.sin_addr.s_addr = inet_addr(server->ip);

    // Connect to server
    if (connect(sd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        printf("%lu: Unable to connect to %s at %lu/%lu.\n", time(NULL), server->ip, server->port, server->protocol);
        return 11;
    }

    // Send order to exchange
    if (send(sd, str_order, strlen(str_order), 0) < 0)
    {
        printf("%lu: Unable to send order to %s at %lu/%lu.\n", time(NULL), server->ip, server->port, server->protocol);
        return 12;
    }
    printf("%lu: Order sent to exchange, waiting for response...\n", time(NULL));

    // Receive exchange's response
    if (recv(sd, server_message, sizeof(server_message), 0) < 0)
    {
        printf("%lu: Error while receiving server's msg\n", time(NULL));
        return 13;
    }
    printf("%lu: Exchange's response: %s\n", time(NULL), server_message);

    // Analyze exchange's response
    int n = strlen(server_message);
    char *read_buffer = calloc(1, n * sizeof(char));

    for (int i = 0; i < n; i++)
    {
        // If delimeter is found, convert to digits what is so far parsed
        if (server_message[i] == ':')
        {
            order->t_server = strtol(read_buffer, NULL, 10);

            // free the buffer and re-allocate it again
            free(read_buffer);
            read_buffer = calloc(1, n * sizeof(char));
        }
        else
        {
            strncat(read_buffer, &server_message[i], 1);
        }
    }
    order->oid = strtol(read_buffer, NULL, 10);

    if (order->t_server == 3833738885019939885 || order->oid == 1688241182)
    {
        printf("%lu: Error while analyzing exchange's response\n", time(NULL));
        return 14;
    }

    // Clean up
    free(read_buffer);
    free(str_order);
    close(sd);

    // Return success if everything is OK
    return 0;
}