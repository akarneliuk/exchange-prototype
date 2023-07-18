/* This code aims to subscribe to multicast tape and receive feed from exchange */

// Preprocessor directives
#include <stdio.h>
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
    int sd = socket(AF_INET, SOCK_DGRAM, addr_mcast->protocol);
    if (sd < 0)
    {
        printf("%lu: Unable to create socket\n", time(NULL));
        return 10;
    }
    printf("%lu: Socket created successfully\n", time(NULL));

    // Allow multiple sockets to use the same PORT number
    u_int64_t yes = 1;
    if (
        setsockopt(
            sd, SOL_SOCKET, SO_REUSEADDR, (char *)&yes, sizeof(yes)) < 0)
    {
        perror("Reusing ADDR failed");
        return 1;
    }

    // Initialize server address (Specifically, what we are listen to -> this host source IP or multicast group)
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(addr_mcast_local->port);
    server_addr.sin_addr.s_addr = inet_addr(addr_mcast->ip);
    uint64_t server_struct_length = sizeof(server_addr);

    // Initialize client address
    struct sockaddr_in client_addr;
    uint32_t client_struct_length = sizeof(client_addr);

    // Bind to the set port and IP:
    if (bind(sd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        printf("%lu: Couldn't bind to the port.\n", time(NULL));
        return 11;
    }
    printf("Done with binding of %lu/%lu at %s\n",
           addr_mcast_local->port,
           addr_mcast_local->protocol,
           addr_mcast_local->ip);

    // Join multicast group on specicific interface
    struct ip_mreq
    {
        /* IP multicast address of group.  */
        struct in_addr imr_multiaddr;

        /* Local IP address of interface.  */
        struct in_addr imr_interface;
    };
    struct ip_mreq mreq;
    mreq.imr_multiaddr.s_addr = inet_addr(addr_mcast->ip);
    mreq.imr_interface.s_addr = inet_addr(addr_mcast_local->ip);

    printf("Joining multicast group %s at %s...\n", addr_mcast->ip, addr_mcast_local->ip);
    if (setsockopt(sd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)&mreq, sizeof(mreq)) < 0)
    {
        printf("%lu: Couldn't join multicast group.\n", time(NULL));
        return 12;
    }

    // Setup connection to Redis
    redisContext *red_con = redisConnect(addr_redis->ip, addr_redis->port);
    if (red_con != NULL && red_con->err)
    {
        printf("%lu: Error: %s\n", time(NULL), red_con->errstr);
        return 19;
    }
    printf("%lu: Connected to Redis\n", time(NULL));

    printf("%lu: Listening for incoming messages...\n\n", time(NULL));
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
            printf("%lu: Couldn't receive message\n", time(NULL));
            return 4;
        }

        // Print message
        printf("%lu: Received message from %s:%d\n",
               time(NULL),
               inet_ntoa(client_addr.sin_addr),
               ntohs(client_addr.sin_port));

        printf("Msg from Exchange: %s\n", client_message);

        // Parse message
        order_t *order = get_orders_from_tape(client_message);

        // Send update to Redis
        order_t *head = order;
        while (head)
        {
            // Update orders in Redis
            uint64_t red_result = add_order_to_redis(red_con, head, 0);

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
        uint64_t red_result = delete_inactive_quotes_from_redis(red_con, last_time);

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
}