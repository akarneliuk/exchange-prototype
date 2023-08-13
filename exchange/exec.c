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
#include <byteswap.h>

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
            memset(server_message, 0, sizeof(server_message));
            memset(client_message, 0, sizeof(client_message));

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

            // Prepare message to send to customer
            struct order_gateway_request_message_t ogm_output;
            memset(&ogm_output, 0, sizeof(ogm_output));
            ogm_output.order_id = bswap_64(head->oid);
            ogm_output.ts_placed = bswap_64(head->t_server);
            ogm_output.ts_executed = bswap_64(get_time_nanoseconds_since_midnight(time_midnight));
            ogm_output.status = 'E';

            // Send notification to customer
            if (send(sd, &ogm_output, sizeof(ogm_output), 0) < 0)
            {
                perror("Error: Cannot send message to customer: ");
                return 15;
            }
            printf("%lu: Notification sent to %s on %lu/%lu, waiting response\n",
                   get_time_nanoseconds_since_midnight(time_midnight),
                   chead->ip,
                   addr_fake_with_port->port,
                   addr_fake_with_port->protocol);

            // Receive response from the customer
            ssize_t recv_bytes;
            if ((recv_bytes = recv(sd, server_message, sizeof(server_message), 0)) < 0)
            {
                perror("Error: Cannot receive message from customer: ");
                return 13;
            }
            struct order_gateway_response_message_t ogm_input;
            memset(&ogm_input, 0, sizeof(ogm_input));
            if (recv_bytes != sizeof(ogm_input))
            {
                perror("Error: TCP: Corrupted message from customer: ");
                return 12;
            }
            memcpy(&ogm_input, server_message, recv_bytes);
            ogm_input.order_id = bswap_64(ogm_input.order_id);
            ogm_input.ts_ack = bswap_64(ogm_input.ts_ack);

            printf("%lu: Customer %s acknowledgement for order_id %lu received\n",
                   get_time_nanoseconds_since_midnight(time_midnight),
                   chead->ip,
                   ogm_input.order_id);

            // Compare sent to received message
            // TODO: Add comparisson of timestamps
            if (ogm_input.order_id == bswap_64(ogm_output.order_id) && ogm_input.status == 'A' && ogm_input.ts_ack > bswap_64(ogm_output.ts_executed))
            {
                printf("%lu: Customer %s acknowledgement for order_id %lu is correct.\n",
                       get_time_nanoseconds_since_midnight(time_midnight),
                       chead->ip,
                       ogm_input.order_id);

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
            else
            {
                printf("%lu: Error: Sent and received messages do not match\n",
                       get_time_nanoseconds_since_midnight(time_midnight));
                return 14;
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