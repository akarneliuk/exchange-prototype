// Preprocessing
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <time.h>
#include <hiredis/hiredis.h>

// Local code
#include "comm.h"
#include "helper.h"
#include "matching_engine.h"
#include "serializers.h"

// Define function prototypes
uint64_t receive_orders(
    server_t *addr_order,
    u_int64_t orders,
    trading_trie_t *tt,
    cid_ip_t *cid_ip_map,
    redisContext *red_con)
{
    // Initialize order number
    u_int64_t order_number = orders;

    // Initialize socket
    int64_t sd = socket(AF_INET, SOCK_STREAM, addr_order->protocol);
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

    // Initialize server address (Destination IP and port, what this server is listening on)
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(addr_order->port);
    server_addr.sin_addr.s_addr = inet_addr(addr_order->ip);

    // Bind socket to port
    if (bind(sd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        printf("%lu: Unable to bind socket to to %s at %lu/%lu.\n", time(NULL), addr_order->ip, addr_order->port, addr_order->protocol);
        return 11;
    }
    printf("%lu: Socket binded successfully %s at %lu/%lu.\n", time(NULL), addr_order->ip, addr_order->port, addr_order->protocol);

    // Listen for incoming connections
    if (listen(sd, 1) < 0)
    {
        printf("%lu: Unable to listen on %s at %lu/%lu.\n", time(NULL), addr_order->ip, addr_order->port, addr_order->protocol);
        return 12;
    }
    printf("%lu: Start Listening on %s at %lu/%lu.\n", time(NULL), addr_order->ip, addr_order->port, addr_order->protocol);

    // Continously receive orders
    while (1)
    {
        // Each time new order is received, increment order number
        order_number++;

        // Create client socket
        struct sockaddr_in client_addr;
        u_int32_t client_size = sizeof(client_addr);
        int64_t csd = accept(sd, (struct sockaddr *)&client_addr, &client_size);
        if (csd < 0)
        {
            printf("%lu: Unable to accept connection on %s at %lu/%lu.\n", time(NULL), addr_order->ip, addr_order->port, addr_order->protocol);
            return 13;
        }
        printf("%lu: Recived order from %s on %i/%lu\n", time(NULL), inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), addr_order->protocol);

        // Recieve order from client
        if (recv(csd, client_message, sizeof(client_message), 0) < 0)
        {
            printf("%lu: Couldn't receive\n", time(NULL));
            return 14;
        }
        printf("%lu: Order from client: %s\n", time(NULL), client_message);

        // Update CID to IP mapping
        char *order_customer_id = get_customer_id(client_message);
        if (order_customer_id == NULL)
        {
            printf("%lu: Unable to extract customer id from order\n", time(NULL));
            return 14;
        }
        update_cid_ip(cid_ip_map, order_customer_id, inet_ntoa(client_addr.sin_addr), red_con);

        // Read clients order from wire
        order_t *order = deserialize_order_wire(client_message, order_number);

        // Send response to client
        sprintf(server_message, "%lu:%lu", time(NULL), order_number);

        if (send(csd, server_message, strlen(server_message), 0) < 0)
        {
            printf("%lu: Can't send response to client\n", time(NULL));
            return 15;
        }
        printf("%lu: Confirmation send to %s on %hu/%lu\n", time(NULL), inet_ntoa(client_addr.sin_addr), htons(client_addr.sin_port), addr_order->protocol);

        // Close client socket
        close(csd);

        // Update trading trie
        match_trade(tt, order, red_con, false);

        // Cleanup
        memset(server_message, '\0', sizeof(server_message));
        memset(client_message, '\0', sizeof(client_message));
    }

    // Cleanup
    close(sd);
    free_cid_ip_map(cid_ip_map);

    // Return success if everything is OK
    return 0;
}