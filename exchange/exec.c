/* This code aims to read data from readis each 333 ms and send messages to clients, if anything is needed */

#define _GNU_SOURCE

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <time.h>

// Local code
#include "helper.h"
#include "comm.h"
#include "serializers.h"
#include "matching_engine.h"

// Main function
int main(void)
{
    // Get connectivity details for redis
    server_t *addr_redis = get_server("REDIS_IP", "REDIS_PORT", IPPROTO_TCP);
    server_t *addr_fake_with_port = get_server("EXCHANGE_ORDER_IP", "CUSTOMER_PORT", CUSTOMER_PROTOCOL);

    // Connect to Redis
    redisContext *red_con = redisConnect(addr_redis->ip, addr_redis->port);

    // Start loop
    while (1)
    // if (1)
    {
        // Read executed orders from Redis
        order_t *order = deserialize_order_redis(red_con, REDIS_EXCHANGE_E_ORDERS);
        cid_ip_t *cid_ip_map = deserialize_cid_ip_redis(red_con, REDIS_EXCHANGE_C2IP);

        // // Print orders for debugging purposes
        // order_t *phead = order;
        // while (phead != NULL)
        // {
        //     printf("Order: %s:%lu:%lu\n",
        //            phead->cid,
        //            phead->t_server,
        //            phead->oid);
        //     phead = phead->next;
        // }
        // cid_ip_t *chpead = cid_ip_map;
        // while (chpead != NULL)
        // {
        //     printf("C2IP: %s:%s\n",
        //            chpead->cid,
        //            chpead->ip);
        //     chpead = chpead->next;
        // }

        // Send orders to clients
        order_t *head = order;
        while (head != NULL)
        {
            // Find client IP
            cid_ip_t *chead = cid_ip_map;
            while (chead != NULL)
            {
                if (strcmp(chead->cid, head->cid) == 0)
                {
                    break;
                }
                chead = chead->next;
            }

            // Initialize socket
            int64_t sd = socket(AF_INET, SOCK_STREAM, CUSTOMER_PROTOCOL);
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
            server_addr.sin_port = htons(addr_fake_with_port->port);
            server_addr.sin_addr.s_addr = inet_addr(chead->ip);

            // Connect to client
            if (connect(sd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
            {
                printf("%lu: Unable to connect to %s at %lu/%lu.\n", time(NULL), chead->ip, addr_fake_with_port->port, addr_fake_with_port->protocol);
                return 11;
            }

            // Prepare message
            sprintf(client_message, "%lu:%lu:;", head->t_server, head->oid);

            // Send order to exchange
            if (send(sd, client_message, strlen(client_message), 0) < 0)
            {
                printf("%lu: Unable to send order to %s at %lu/%lu.\n", time(NULL), chead->ip, addr_fake_with_port->port, addr_fake_with_port->protocol);
                return 12;
            }
            printf("%lu: Confirmation sent to client, waiting for response...\n", time(NULL));

            // Receive exchange's response
            if (recv(sd, server_message, sizeof(server_message), 0) < 0)
            {
                printf("%lu: Error while receiving server's msg\n", time(NULL));
                return 13;
            }
            printf("%lu: Client's response: %s\n", time(NULL), server_message);

            // Compare sent to received message
            if (strcmp(client_message, server_message) != 0)
            {
                printf("%lu: Error: Sent and received messages do not match\n", time(NULL));
                return 14;
            }
            else
            {
                printf("%lu: Sent and received messages match\n", time(NULL));

                // Delete entries from Redis
                redisReply *red_rep = redisCommand(red_con, "HDEL %s %lu",
                                                   REDIS_EXCHANGE_E_ORDERS,
                                                   head->oid);

                // Check if Redis returned an error
                if (red_rep->str != NULL)
                {
                    printf("%lu: Unable to delete order details in Redis: %s\n", time(NULL), red_rep->str);
                    freeReplyObject(red_rep);
                    return 1;
                }
                else
                {
                    printf("%lu: Order details deleted from Redis\n", time(NULL));
                }
                freeReplyObject(red_rep);
            }

            // Client has received the message, close connection
            close(sd);

            // Move to next order
            head = head->next;
        }

        // Cleanup
        free_cid_ip_map(cid_ip_map);
        free_order_list(order);

        // Print info message
        printf("%lu: Sleeping for 500 ms...\n", time(NULL));

        // Sleep for 500 ms
        nanosleep((const struct timespec[]){{0, 500000000L}}, NULL);
    }

    // Close connection to Redis
    redisFree(red_con);

    // Cleanup
    free(addr_redis);
}