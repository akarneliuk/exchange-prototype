/* This code aims to listen to messages from exchange about executed orders */

// Preprocessor directives
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <hiredis/hiredis.h>

// Local code
#include "helper.h"
#include "comm.h"

// Main function
int main(void)
{
    // Get connection details
    server_t *addr_redis = get_server("REDIS_IP", "REDIS_PORT", IPPROTO_TCP);
    server_t *addr_local = get_server("CUSTOMER_IP_ACCEPT_UNICAST", "CUSTOMER_PORT", IPPROTO_TCP);

    // Initialize socket
    int sd = socket(AF_INET, SOCK_STREAM, addr_local->protocol);
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

    // Initialize server address (Specifically, what we are listen to)
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(addr_local->port);
    server_addr.sin_addr.s_addr = inet_addr(addr_local->ip);

    // Bind to the set port and IP:
    if (bind(sd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        printf("%lu: Couldn't bind to the port.\n", time(NULL));
        return 11;
    }
    printf("Done with binding of %lu/%lu at %s\n",
           addr_local->port,
           addr_local->protocol,
           addr_local->ip);

    // Listen for incoming connections
    if (listen(sd, 1) < 0)
    {
        printf("%lu: Unable to listen on %s at %lu/%lu.\n", time(NULL), addr_local->ip, addr_local->port, addr_local->protocol);
        return 12;
    }
    printf("%lu: Start Listening on %s at %lu/%lu.\n", time(NULL), addr_local->ip, addr_local->port, addr_local->protocol);

    // Setup connection to Redis
    redisContext *red_con = redisConnect(addr_redis->ip, addr_redis->port);
    if (red_con != NULL && red_con->err)
    {
        printf("%lu: Error: %s\n", time(NULL), red_con->errstr);
        return 19;
    }
    printf("%lu: Connected to Redis\n", time(NULL));

    // Listen to incoming connections
    while (1)
    {
        // Create client socket
        struct sockaddr_in client_addr;
        uint32_t client_size = sizeof(client_addr);
        int64_t csd = accept(sd, (struct sockaddr *)&client_addr, &client_size);
        if (csd < 0)
        {
            printf("%lu: Unable to accept connection on %s at %lu/%lu.\n", time(NULL), addr_local->ip, addr_local->port, addr_local->protocol);
            return 13;
        }
        printf("%lu: Recived executed order notification from %s on %i/%lu\n", time(NULL), inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), addr_local->protocol);

        // Read message from exchange
        if (recv(csd, client_message, sizeof(client_message), 0) < 0)
        {
            printf("%lu: Couldn't receive\n", time(NULL));
            return 14;
        }
        printf("%lu: Order from exchange: %s\n", time(NULL), client_message);

        // Deserialize message
        order_t *order = deserialize_exhange_confirmation(client_message);

        // Send response to client
        strncpy(server_message, client_message, sizeof(server_message));
        if (send(csd, server_message, strlen(server_message), 0) < 0)
        {
            printf("%lu: Can't send response to exchange\n", time(NULL));
            return 15;
        }
        printf("%lu: Confirmation send to exchange %s:%hu/%lu\n", time(NULL), inet_ntoa(client_addr.sin_addr), htons(client_addr.sin_port), addr_local->protocol);

        // Close client socket
        close(csd);

        // Update Redis
        uint64_t result_redis_process = process_completed_order_redis(red_con, order);

        // Cleanup
        memset(server_message, '\0', sizeof(server_message));
        memset(client_message, '\0', sizeof(client_message));
        free(order);
    }

    // Close socket
    close(sd);

    // Close connection to Redis
    redisFree(red_con);

    // Clean up
    free(addr_local);
    free(addr_redis);
}