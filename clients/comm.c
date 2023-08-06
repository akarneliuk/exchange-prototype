// Preprocessing
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <time.h>

// Local code
#include "comm.h"
#include "helper.h"

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
    int64_t sd = socket(AF_INET, SOCK_STREAM, server->protocol);
    if (sd < 0)
    {
        perror("Error: Cannot create socket: ");
        return 1;
    }
    printf("%s: Socket created successfully\n", get_human_readable_time());

    // Initialize message buffer
    char server_message[MAX_MSG_LEN], client_message[MAX_MSG_LEN];
    memset(server_message, '\0', sizeof(server_message));
    memset(client_message, '\0', sizeof(client_message));

    // Initialize server address (Destination IP and port)
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server->port);
    if (inet_pton(AF_INET, server->ip, &server_addr.sin_addr) < 0)
    {
        perror("Error: Uncompatible IP Address: ");
        return 2;
    }

    // Connect to server
    if (connect(sd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("Error: Cannot connect to exchange: ");
        return 3;
    }

    // Send order to exchange
    if (send(sd, str_order, strlen(str_order), 0) < 0)
    {
        perror("Error: Cannot send the message: ");
        return 4;
    }
    printf("%s: Order sent to exchange, waiting for response...\n", get_human_readable_time());

    // Receive exchange's response
    if (recv(sd, server_message, sizeof(server_message), 0) < 0)
    {
        perror("Error: Cannot receive the message: ");
        return 5;
    }
    printf("%s: Exchange's response: %s\n", get_human_readable_time(), server_message);

    // Analyze exchange's response
    uint64_t n = strlen(server_message);
    char *read_buffer = calloc(1, n * sizeof(char));

    for (uint64_t i = 0; i < n; i++)
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
        printf("%s: Error while analyzing exchange's response\n", get_human_readable_time());
        return 14;
    }

    // Clean up
    free(read_buffer);
    free(str_order);
    close(sd);

    // Return success if everything is OK
    return 0;
}