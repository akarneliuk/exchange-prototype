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
    {
        // Read executed orders from Redis
        order_t *order = deserialize_order_redis(red_con, REDIS_EXCHANGE_E_ORDERS);
        cid_ip_t *cid_ip_map = deserialize_cid_ip_redis(red_con, REDIS_EXCHANGE_C2IP);

        // Get midnight time
        uint64_t time_midnight = get_time_nanoseconds_midnight();

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
                perror("Error: Cannot create socket: ");
                return 1;
            }
            printf("%lu: Socket created successfully\n",
                   get_time_nanoseconds_since_midnight(time_midnight));

            // Initialize message buffer
            char server_message[MAX_MSG_LEN], client_message[MAX_MSG_LEN];
            memset(server_message, '\0', sizeof(server_message));
            memset(client_message, '\0', sizeof(client_message));

            // Initialize server address (Destination IP and port)
            struct sockaddr_in server_addr;
            memset(&server_addr, 0, sizeof(server_addr));
            server_addr.sin_family = AF_INET;
            server_addr.sin_port = htons(addr_fake_with_port->port);
            if (inet_pton(AF_INET, chead->ip, &server_addr.sin_addr) < 0)
            {
                perror("Error: Uncompatible IP Address: ");
                return 2;
            }

            // Connect to client
            if (connect(sd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
            {
                perror("Error: Cannot connect to exchange: ");
                return 3;
            }

            // Prepare message
            sprintf(client_message, "%lu:%lu:;", head->t_server, head->oid);

            // Send order to exchange
            if (send(sd, client_message, strlen(client_message), 0) < 0)
            {
                printf("%lu: Can't send response to client\n",
                       get_time_nanoseconds_since_midnight(time_midnight));
                return 15;
            }
            printf("%lu: Confirmation send to %s on %lu/%lu, waiting response\n",
                   get_time_nanoseconds_since_midnight(time_midnight),
                   chead->ip,
                   addr_fake_with_port->port,
                   addr_fake_with_port->protocol);
            // Receive exchange's response
            if (recv(sd, server_message, sizeof(server_message), 0) < 0)
            {
                printf("%lu: Error while receiving server's msg\n",
                       get_time_nanoseconds_since_midnight(time_midnight));
                return 13;
            }
            printf("%lu: Client's response: %s\n",
                   get_time_nanoseconds_since_midnight(time_midnight),
                   server_message);

            // Compare sent to received message
            if (strcmp(client_message, server_message) != 0)
            {
                printf("%lu: Error: Sent and received messages do not match\n",
                       get_time_nanoseconds_since_midnight(time_midnight));
                return 14;
            }
            else
            {
                printf("%lu: Sent and received messages match\n",
                       get_time_nanoseconds_since_midnight(time_midnight));

                // Delete entries from Redis
                redisReply *red_rep = redisCommand(red_con, "HDEL %s %lu",
                                                   REDIS_EXCHANGE_E_ORDERS,
                                                   head->oid);

                // Check if Redis returned an error
                if (red_rep->str != NULL)
                {
                    printf("%lu: Unable to delete order details in Redis: %s\n",
                           get_time_nanoseconds_since_midnight(time_midnight),
                           red_rep->str);
                    freeReplyObject(red_rep);
                    return 1;
                }
                else
                {
                    printf("%lu: Order details deleted from Redis\n",
                           get_time_nanoseconds_since_midnight(time_midnight));
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
        printf("%lu: Sleeping for 500 ms...\n",
               get_time_nanoseconds_since_midnight(time_midnight));

        // Sleep for 500 ms
        nanosleep((const struct timespec[]){{0, 500000000L}}, NULL);
    }

    // Close connection to Redis
    redisFree(red_con);

    // Cleanup
    free(addr_redis);
}