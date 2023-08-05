/* This code aims to subscribe to multicast tape and receive feed from exchange */

// Preprocessor directives
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <linux/in.h>
#include <hiredis/hiredis.h>

// Local code
#include "helper.h"
#include "comm.h"

// Main function
int main(void)
{
    // Get connection details
    server_t *addr_mcast = get_server("EXCHANGE_TAPE_IP", "EXCHANGE_TAPE_PORT", IPPROTO_UDP);
    server_t *addr_mcast_local = get_server("CUSTOMER_IP_ACCEPT_MULTICAST", "EXCHANGE_TAPE_PORT", IPPROTO_UDP);
    server_t *addr_redis = get_server("REDIS_IP", "REDIS_PORT", IPPROTO_TCP);

    // Initialize socket
    int64_t sd = socket(AF_INET, SOCK_DGRAM, addr_mcast->protocol);
    if (sd < 0)
    {
        perror("Error: Cannot create socket: ");
        return 10;
    }
    printf("%s: Socket created successfully\n", get_human_readable_time());

    // Initialize address, which server listens to
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(addr_mcast_local->port);
    if (inet_pton(AF_INET, addr_mcast->ip, &server_addr.sin_addr) < 0)
    {
        perror("Error: Uncompatible IP Address: ");
    }

    // Initialize client address
    struct sockaddr_in client_addr;
    memset(&client_addr, 0, sizeof(client_addr));
    uint32_t client_struct_length = sizeof(client_addr);

    // Allow multiple sockets to use the same PORT number
    uint64_t so_reuseaddr = 1;
    if (setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &so_reuseaddr, sizeof(so_reuseaddr)) < 0)
    {
        perror("Error: Cannot set socket option: ");
        return 1;
    }

    // Bind to the set port and IP:
    if (bind(sd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("Error: Cannot bind port: ");
        return 11;
    }
    printf("%s, Done with binding of %lu/%lu at %s\n",
           get_human_readable_time(),
           addr_mcast_local->port,
           addr_mcast_local->protocol,
           addr_mcast_local->ip);

    // Initialize request to join multicast
    struct ip_mreq
    {
        /* IP multicast address of group.  */
        struct in_addr imr_multiaddr;

        /* Local IP address of interface.  */
        struct in_addr imr_interface;
    };
    struct ip_mreq mreq;
    memset(&mreq, 0, sizeof(0));
    mreq.imr_multiaddr.s_addr = inet_addr(addr_mcast->ip);
    mreq.imr_interface.s_addr = inet_addr(addr_mcast_local->ip);

    // Join multicast group on specicific interface
    printf("Joining multicast group %s at %s...\n", addr_mcast->ip, addr_mcast_local->ip);
    if (setsockopt(sd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0)
    {
        perror("Error: Cannot join multicast group: ");
        return 12;
    }

    // Setup connection to Redis
    redisContext *red_con = redisConnect(addr_redis->ip, addr_redis->port);
    if (red_con != NULL && red_con->err)
    {
        printf("%s: App Error: %s\n", get_human_readable_time(), red_con->errstr);
        return 19;
    }
    printf("%s: Connected to Redis\n", get_human_readable_time());

    printf("%s: Listening for incoming messages...\n\n", get_human_readable_time());
    // Start listen loop
    while (1)
    {
        // Initialize message buffer
        char client_message[MAX_MSG_LEN];
        memset(client_message, '\0', sizeof(client_message));

        // Receive message
        if (recvfrom(sd, client_message, sizeof(client_message), 0,
                     (struct sockaddr *)&client_addr, &client_struct_length) < 0)
        {
            perror("Error: Cannot receive message: ");
            return 4;
        }

        // Print message
        printf("%s: Received message from %s:%d\n",
               get_human_readable_time(),
               inet_ntoa(client_addr.sin_addr),
               ntohs(client_addr.sin_port));

        printf("Content: %s\n", client_message);

        // Parse message
        order_t *order = get_orders_from_tape(client_message);

        // Send update to Redis
        order_t *head = order;
        while (head)
        {
            // Update orders in Redis
            if (add_order_to_redis(red_con, head, 0) < 0)
            {
                perror("Error: Cannot update quotes: ");
                return 5;
            }

            head = head->next;
        }

        // Get the last quote time or set 0 if there is no active quote
        uint64_t last_time;
        if (order != NULL)
        {
            last_time = order->t_server;
        }
        else
        {
            last_time = 0;
        }

        // Delete quotes which are gone
        if (delete_inactive_quotes_from_redis(red_con, last_time) < 0)
        {
            perror("Error: Cannot delete obsolte quotes: ");
            return 6;
        }

        // Clean memory for new message
        memset(client_message, '\0', sizeof(client_message));
        free_order_list(order);
    }

    // Close socket
    close(sd);

    // Close connection to Redis
    redisFree(red_con);

    // Clean up
    free(addr_mcast);
    free(addr_mcast_local);
    free(addr_redis);

    // Return success
}