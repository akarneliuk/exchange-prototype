// Preprocessing
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <hiredis/hiredis.h>

// Local headers
#include "helper.h"
#include "comm.h"

// Main function
int main(int argc, char *argv[])
{
    /* This is a main script for info server, which sends multicast feed every so often:

      Possible parameters:
      - multicast group IPv4 address
      - UDP port for application
      - Sleep time between messages
    */

    // Initialize message buffer
    char server_message[MAX_MSG_LEN], client_message[MAX_MSG_LEN];
    memset(server_message, '\0', sizeof(server_message));
    memset(client_message, '\0', sizeof(client_message));

    // Get connection details
    server_t *addr_mcast = get_server("EXCHANGE_TAPE_IP", "EXCHANGE_TAPE_PORT", EXCHANGE_MCAST_PROTOCOL);
    server_t *addr_redis = get_server("REDIS_IP", "REDIS_PORT", IPPROTO_TCP);
    server_t *addr_mcast_source = get_server("EXCHANGE_TAPE_SOURCE_IP", "EXCHANGE_TAPE_PORT", EXCHANGE_MCAST_PROTOCOL);

    // Initialize socket
    int64_t sd = socket(AF_INET, SOCK_DGRAM, addr_mcast->protocol);
    if (sd < 0)
    {
        perror("Error: Cannot create socket: ");
        return 10;
    }
    printf("%lu: Socket created successfully\n", time(NULL));

    // Specify socket to a specific interface
    struct in_addr addr;
    memset(&addr, 0, sizeof(addr));
    addr.s_addr = inet_addr(addr_mcast_source->ip);
    if (setsockopt(sd, IPPROTO_IP, IP_MULTICAST_IF, &addr, sizeof(addr)) < 0)
    {
        perror("Error: Cannot set socket option: ");
        return 12;
    }

    // Initialize server address (Destination IP and port)
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(addr_mcast->port);
    if (inet_pton(AF_INET, addr_mcast->ip, &server_addr.sin_addr) < 0)
    {
        perror("Error: Uncompatible IP Address: ");
    }

    // Open connection to Redis
    redisContext *red_con = redisConnect(addr_redis->ip, addr_redis->port);
    if (red_con != NULL && red_con->err)
    {
        printf("%lu: Error: %s\n", time(NULL), red_con->errstr);
        return 19;
    }
    printf("%lu: Connected to Redis\n", time(NULL));

    // Initialize msg coounter
    uint64_t msg_counter = 0;

    // Start loop for generating and sending messages
    printf("EXECHANGE IS OPENED! TRADING STARTED!\n");
    printf("Sending data at %s @ %lu/%lu\n",
           addr_mcast->ip,
           addr_mcast->port,
           addr_mcast->protocol);

    // Get timestamp for the midnight
    int64_t time_midnight = get_time_nanoseconds_midnight();
    if (time_midnight < 0)
    {
        perror("Error: Cannot get time for midnight");
        return 1;
    }

    // Server execution loop
    while (true)
    {
        // Prepare message
        char *msg = calloc(MAX_MSG_LEN, sizeof(char));

        // Read from Redis
        redisReply *red_reply;
        red_reply = redisCommand(red_con, "HKEYS %s", REDIS_EXCHANGE_A_ORDERS);
        if (red_reply->elements == 0)
        {
            sprintf(msg, "%lu:%lu:;", get_time_nanoseconds_since_midnight(time_midnight), msg_counter);
        }
        else
        {
            sprintf(msg, "%lu:%lu:", time(NULL), msg_counter);
            for (int i = 0; i < red_reply->elements; i++)
            {
                redisReply *redis_reply_order = redisCommand(red_con, "HVALS %s:%s", REDIS_EXCHANGE_ORDER_PREFIX, red_reply->element[i]->str);

                // Append order id
                strncat(msg, red_reply->element[i]->str, strlen(red_reply->element[i]->str));
                strncat(msg, "/", 1);

                // Add order details to the message if provided
                if (redis_reply_order->elements == 7)
                {
                    for (int j = 3; j < redis_reply_order->elements; j++)
                    {
                        strncat(msg, redis_reply_order->element[j]->str, strlen(redis_reply_order->element[j]->str));

                        // Add delimiter
                        if (j < redis_reply_order->elements - 1)
                        {
                            strncat(msg, "/", 1);
                        }
                        else
                        {
                            strncat(msg, ":", 1);
                        }
                    }
                }

                freeReplyObject(redis_reply_order);
            }

            // Add end of the message delimiter
            strcat(msg, ";");
        }
        freeReplyObject(red_reply);

        // Send multicast message to the exchange clients
        if (sendto(sd, msg, strlen(msg), 0, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
        {
            printf("%lu: Unable to send multicast message\n", time(NULL));
            return 11;
        }

        // Debug message test
        printf("Test message: %s\n", msg);

        // Cleanup
        free(msg);

        // Increase counter
        msg_counter++;

        // Delay till next execution
        sleep(1);
    }

    // printf("Server's response: %s\n", server_message);

    // Close the socket
    close(sd);

    // Close connection to Redis
    redisFree(red_con);

    // Clean up
    free(addr_mcast);
    free(addr_redis);

    // Return success
    return 0;
}
