/* This code aims to listen to messages from exchange about executed orders */

// Preprocessor directives
#include <stdio.h>
#include <stdint.h>
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
    int64_t sd = socket(AF_INET, SOCK_STREAM, addr_local->protocol);
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

    // Initialize server address (Specifically, what we are listen to)
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(addr_local->port);
    if (inet_pton(AF_INET, addr_local->ip, &server_addr.sin_addr) < 0)
    {
        perror("Error: Uncompatible IP Address: ");
        return 2;
    }

    // Allow reusing IP address to cater for process crashes/restarts
    uint64_t so_reuseaddr = 1;
    if (setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &so_reuseaddr, sizeof(so_reuseaddr)) < 0)
    {
        perror("Error: Cannot set socket option: ");
        return 3;
    }

    // Bind to the set port and IP:
    if (bind(sd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("Error: Cannot bind socket: ");
        return 4;
    }
    printf("%s, Done with binding of %lu/%lu at %s\n",
           get_human_readable_time(),
           addr_local->port,
           addr_local->protocol,
           addr_local->ip);

    // Listen for incoming connections
    if (listen(sd, LISTENQ) < 0)
    {
        perror("Error: Cannot listen on socket: ");
        return 5;
    }
    printf("%s: Start Listening on %s at %lu/%lu.\n",
           get_human_readable_time(),
           addr_local->ip,
           addr_local->port, addr_local->protocol);

    // Setup connection to Redis
    redisContext *red_con = redisConnect(addr_redis->ip, addr_redis->port);
    if (red_con != NULL && red_con->err)
    {
        printf("%s: Error: %s\n", get_human_readable_time(), red_con->errstr);
        return 19;
    }
    printf("%s: Connected to Redis\n", get_human_readable_time());

    // Listen to incoming connections
    while (1)
    {
        // Create client socket
        struct sockaddr_in client_addr;
        uint32_t client_size = sizeof(client_addr);
        int64_t csd = accept(sd, (struct sockaddr *)&client_addr, &client_size);
        if (csd < 0)
        {
            perror("Error: Cannot accept connection on socket: ");
            return 13;
        }

        // Get exchange IP
        char exchange_ip[INET_ADDRSTRLEN];
        memset(exchange_ip, '\0', sizeof(exchange_ip));
        if (inet_ntop(AF_INET, &(client_addr.sin_addr), exchange_ip, INET_ADDRSTRLEN) == NULL)
        {
            perror("Error: Cannot get IP Address of exchange: ");
            return 8;
        }

        printf("%s: Recived executed order notification from %s on %i/%lu\n",
               get_human_readable_time(),
               exchange_ip,
               ntohs(client_addr.sin_port),
               addr_local->protocol);

        // Read message from exchange
        if (recv(csd, client_message, sizeof(client_message), 0) < 0)
        {
            perror("Error: Cannot receive message: ");
            return 14;
        }
        printf("%s: Order from exchange: %s\n",
               get_human_readable_time(),
               client_message);

        // Deserialize message
        order_t *order = deserialize_exhange_confirmation(client_message);

        // Send response to client
        memcpy(server_message, client_message, sizeof(server_message));
        if (send(csd, server_message, strlen(server_message), 0) < 0)
        {
            perror("Error: Cannot send message: ");
            free(order);
            goto cleanup;
        }
        printf("%s: Confirmation send to exchange %s:%hu/%lu\n",
               get_human_readable_time(),
               exchange_ip,
               htons(client_addr.sin_port),
               addr_local->protocol);

        // Close client socket
        close(csd);

        // Update Redis
        if (process_completed_order_redis(red_con, order) < 0)
        {
            perror("Error: Cannot process Redis data: ");
            free(order);
            goto cleanup;
        }

        // Cleanup
        memset(server_message, '\0', sizeof(server_message));
        memset(client_message, '\0', sizeof(client_message));
        free(order);
    }

cleanup:

    // Close socket
    close(sd);

    // Close connection to Redis
    redisFree(red_con);

    // Clean up
    free(addr_local);
    free(addr_redis);
}