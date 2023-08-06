// Preprocessing
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <uuid/uuid.h>
#include <arpa/inet.h>

// Local code
#include "helper.h"

// Define aux functions
void get_or_create_uuid(char *uuid)
{
    /* Helper function to check if UUID is already generated and use it or to create new*/

    // File name
    char *filename = "id.txt";

    // Check if UUID file exists
    FILE *fp = fopen(filename, "r");
    if (fp == NULL)
    {
        // If file does not exist, create new UUID
        uuid_t binuuid;
        uuid_generate_random(binuuid);
        uuid_unparse_lower(binuuid, uuid);

        // Save UUID to file
        fp = fopen(filename, "w");
        fprintf(fp, "%s", uuid);
        fclose(fp);
    }
    else
    {
        // If file exists, read UUID from file
        fscanf(fp, "%s", uuid);
        fclose(fp);
    }
}

uint64_t get_operation(char *op)
{
    /* Helper function to convert operation to integer.
       Result:
        - 0 for sell opeation
        - 1 for buy operation
        - 2 for cancel operation
    */
    if (memcmp(op, "buy", 3) == 0)
    {
        return 1;
    }
    else if (memcmp(op, "sell", 4) == 0)
    {
        return 0;
    }
    else if (memcmp(op, "cancel", 6) == 0)
    {
        return 2;
    }
    else
    {
        return 9999;
    }
}

server_t *get_server(char *env_ip, char *env_port, uint64_t protocol)
{
    /* Helper function to get server details from environment variables*/

    // Allocate memory for server
    server_t *server = calloc(1, sizeof(server_t));

    // Get server IP
    char *ip = getenv(env_ip);
    if (ip == NULL)
    {
        perror("Error: Cannot allocate memory: ");
        exit(1);
    }
    memcpy(server->ip, ip, 15);

    // Get server port
    char *port = getenv(env_port);
    if (port == NULL)
    {
        perror("Error: Cannot allocate memory: ");
        exit(1);
    }
    server->port = strtol(port, NULL, 10);

    // Set the protocol
    server->protocol = protocol;

    return server;
}

order_t *get_orders_from_tape(char *tape)
{
    /* Helper function to parse tape */
    uint64_t msg_len = strlen(tape);
    uint64_t msg_pos = 0;
    uint64_t t_server = 0;

    // Allocate memory for order and initialize values to 0/NULL
    order_t *order = calloc(1, sizeof(order_t));
    if (order == NULL)
    {
        printf("%s: Unable to allocate memory for order\n", get_human_readable_time());
        return NULL;
    }
    order->next = NULL;

    // Allocate buffer
    char buffer[100];
    memset(buffer, '\0', sizeof(buffer));

    // Set moving head
    order_t *head = order;

    // Loop through all characters in the tape
    for (uint64_t i = 0; i < msg_len; i++)
    {
        // Replace '/' with ' ' to split the string
        if (tape[i] == '/')
        {
            tape[i] = ' ';
        }

        // Find boundaries
        if (tape[i] != ':')
        {
            strncat(buffer, &tape[i], 1);
        }
        else
        {
            // Pickup the timestamp
            if (msg_pos == 1)
            {
                t_server = strtoul(buffer, NULL, 10);
            }
            // Pick up values
            if (msg_pos > 1)
            {
                // replace '/' with ' ' to split the string
                for (uint64_t j = 0; j < strlen(buffer); j++)
                {
                    if (buffer[j] == '/')
                    {
                        buffer[j] = ' ';
                    }
                }

                sscanf(buffer, "%lu %s %lu %f %lu", &head->oid, head->symbol, &head->operation, &head->price, &head->quantity);

                // Set timestamp
                head->t_server = t_server;

                // Create new order and update the head
                order_t *new_order = malloc(sizeof(order_t));
                if (new_order == NULL)
                {
                    perror("Error: Cannot allocate memory: ");
                    free_order_list(head);
                    return NULL;
                }
                memset(new_order->symbol, '\0', sizeof(new_order->symbol));
                new_order->next = head;
                head = new_order;
            }
            // printf("%s\n", buffer);
            memset(buffer, '\0', sizeof(buffer));

            // Update counter
            msg_pos++;
        }
    }

    // Free memory for the last order
    order_t *temp = head;
    head = head->next;
    free(temp);

    return head;
}

void free_order_list(order_t *order)
{
    /* Helper function to clean up the memory used in executed orders*/

    // Go through the list to clean memory
    order_t *head = order;
    while (head != NULL)
    {
        order_t *temp = head;
        head = head->next;
        free(temp);
    }
}

int64_t add_order_to_redis(redisContext *red_con, order_t *order, uint64_t my_or_all)
{
    /* Helper function to add active order to redis*/

    // Create Hash with order details
    redisReply *red_rep1 = redisCommand(red_con, "HSET %s:%lu oid %lu t_server %lu symbol %s op %lu price %.2f qty %lu",
                                        REDIS_CUSTOMER_ORDER_PREFIX,
                                        order->oid,
                                        order->oid,
                                        order->t_server,
                                        order->symbol,
                                        order->operation,
                                        order->price,
                                        order->quantity);

    if (red_rep1->str != NULL)
    {
        printf("%s: Unable to create order %lu details in Redis: %s\n",
               get_human_readable_time(),
               order->oid,
               red_rep1->str);
        freeReplyObject(red_rep1);
        return -1;
    }
    else
    {
        printf("%s: Order %lu details are created in Redis.\n",
               get_human_readable_time(),
               order->oid);
        freeReplyObject(red_rep1);
    }

    // Add order depending on the `my_or_all` flag
    char redis_request[100];
    memset(redis_request, '\0', sizeof(redis_request));
    // Select request for all orders
    if (my_or_all == 0)
    {
        sprintf(redis_request, "HSET %s %lu %lu",
                REDIS_CUSTOMER_ALL_ORDERS,
                order->oid,
                order->t_server);
    }
    // Select request for my orders
    else
    {
        sprintf(redis_request, "HSET %s %lu %lu",
                REDIS_CUSTOMER_MY_ORDERS,
                order->oid,
                order->t_server);
    }

    // Add order to the hash queue
    redisReply *red_rep2 = redisCommand(red_con, redis_request);
    if (red_rep2->str != NULL)
    {
        printf("%s: Unable to add order %lu to the active queue in Redis: %s\n",
               get_human_readable_time(),
               order->oid,
               red_rep2->str);
        freeReplyObject(red_rep2);
        return -1;
    }
    else
    {
        printf("%s: Order %lu is added to the active queue in Redis.\n",
               get_human_readable_time(),
               order->oid);
    }
    freeReplyObject(red_rep2);

    // Success
    return 0;
}

int64_t delete_inactive_quotes_from_redis(redisContext *red_con, uint64_t last_time)
{
    /* Helper function to remove inactive order from redis*/

    // Get list of existings orders
    redisReply *red_rep1 = redisCommand(red_con, "HGETALL %s",
                                        REDIS_CUSTOMER_ALL_ORDERS);
    if (red_rep1->str != NULL)
    {
        printf("%s: Unable to get orders' list in Redis: %s\n",
               get_human_readable_time(),
               red_rep1->str);
        freeReplyObject(red_rep1);
        return -1;
    }
    else
    {
        printf("%s: Orders' list is collected in Redis.\n",
               get_human_readable_time());
    }

    // Loop through all orders
    for (uint64_t i = 0; i < red_rep1->elements; i += 2)
    {
        // Check delete element if timestamp is less than last_time
        if ((uint64_t)strtol(red_rep1->element[i + 1]->str, NULL, 10) != last_time)
        {
            printf("HDEL %s %s\n",
                   REDIS_CUSTOMER_ALL_ORDERS,
                   red_rep1->element[i]->str);
            // Delete order from list of active orders
            redisReply *red_rep2 = redisCommand(red_con, "HDEL %s %s",
                                                REDIS_CUSTOMER_ALL_ORDERS,
                                                red_rep1->element[i]->str);
            if (red_rep2->str != NULL)
            {
                printf("%s: Unable to delete order fro, Redis: %s\n",
                       get_human_readable_time(),
                       red_rep2->str);
                freeReplyObject(red_rep2);
                return -1;
            }
            else
            {
                printf("%s: Order %s is deleted in Redis.\n",
                       get_human_readable_time(),
                       red_rep1->element[i]->str);
            }

            // Cleanup
            freeReplyObject(red_rep2);

            // Order details will be in memory, but there is no efficient way to delete them
            // So some sort of garbage collection will be needed
        }
    }

    // Cleanup
    freeReplyObject(red_rep1);

    // Success
    return 0;
}

void print_order_from_redis(uint64_t my_or_all)
{
    // Connect to redis
    server_t *addr_redis = get_server("REDIS_IP", "REDIS_PORT", IPPROTO_TCP);
    redisContext *red_con = redisConnect(addr_redis->ip, addr_redis->port);

    // Check connection to Redis
    if (red_con->err > 0)
    {
        // Get human readable time
        printf("%s: Redis error: %s\n", get_human_readable_time(), red_con->errstr);
        redisFree(red_con);
        free(addr_redis);
        exit(1);
    }

    // Select scope to collect orders
    char redis_request[100];
    memset(redis_request, '\0', sizeof(redis_request));

    // Select request for all orders
    if (my_or_all == 0)
    {
        sprintf(redis_request, "HKEYS %s", REDIS_CUSTOMER_ALL_ORDERS);
    }
    // Select request for my orders
    else
    {
        sprintf(redis_request, "HKEYS %s", REDIS_CUSTOMER_MY_ORDERS);
    }

    // Get headers for the existing orders
    redisReply *red_rep1 = redisCommand(red_con, redis_request);
    if (red_rep1->str != NULL)
    {
        printf("%s: Unable to get orders' list in Redis: %s\n",
               get_human_readable_time(),
               red_rep1->str);
        freeReplyObject(red_rep1);
        redisFree(red_con);
        free(addr_redis);
        exit(1);
    }

    // Get and print content of the detailed orders
    for (uint64_t i = 0; i < red_rep1->elements; i++)
    {
        redisReply *red_rep2 = redisCommand(red_con, "HVALS %s:%s",
                                            REDIS_CUSTOMER_ORDER_PREFIX,
                                            red_rep1->element[i]->str);

        if (red_rep2->str != NULL)
        {
            printf("%s: Unable to get order %s ifrom Redis.\n",
                   get_human_readable_time(),
                   red_rep1->str);
            freeReplyObject(red_rep1);
            redisFree(red_con);
            free(addr_redis);
            exit(1);
        }
        else
        {
            char op_text[10];
            memset(op_text, '\0', sizeof(op_text));
            if (strtol(red_rep2->element[3]->str, NULL, 10) == 0)
            {
                memcpy(op_text, "SELL", 4);
            }
            else
            {
                memcpy(op_text, "BUY", 3);
            }

            printf("  - symbol:          %s\n    operation:       %s\n    price per share: %s\n    quantity:        %s\n\n",
                   red_rep2->element[2]->str,
                   op_text,
                   red_rep2->element[4]->str,
                   red_rep2->element[5]->str);
        }
        freeReplyObject(red_rep2);
    }

    // Cleanup
    freeReplyObject(red_rep1);
    redisFree(red_con);
    free(addr_redis);
}

order_t *deserialize_exhange_confirmation(char *msg)
{
    /* Helper function to conver string to struct*/
    order_t *order = malloc(sizeof(order_t));
    if (order == NULL)
    {
        printf("%s: Unable to allocate memory for order\n", get_human_readable_time());
        return NULL;
    }

    // Loop through order
    char buffer[100];
    memset(buffer, '\0', sizeof(buffer));

    uint64_t sl = strlen(msg);
    uint64_t msg_pos = 0;
    for (uint64_t i = 0; i < sl - 1; i++)
    {
        // Add item to order struct
        if (msg[i] == ':')
        {
            if (msg_pos == 0)
            {
                order->t_server = strtoul(buffer, NULL, 10);
            }
            else
            {
                order->oid = strtoul(buffer, NULL, 10);
            }

            // Increse position counter
            msg_pos++;

            // Clean buffer
            memset(buffer, '\0', sizeof(buffer));
        }
        else
        {
            strncat(buffer, &msg[i], 1);
        }
    }

    return order;
}

int64_t process_completed_order_redis(redisContext *red_con, order_t *order)
{
    /* Helper function to process the order to redis */

    redisReply *red_rep1 = redisCommand(red_con, "HDEL %s %lu",
                                        REDIS_CUSTOMER_MY_ORDERS,
                                        order->oid);
    if (red_rep1->str != NULL)
    {
        printf("%s: Unable to delete order %lu from mine in Redis: %s\n", get_human_readable_time(), order->oid, red_rep1->str);
        freeReplyObject(red_rep1);
        return -1;
    }
    else
    {
        printf("%s: Order %lu is delete from mine in Redis.\n", get_human_readable_time(), order->oid);
    }
    freeReplyObject(red_rep1);

    // Return success
    return 0;
}

char *get_human_readable_time()
{
    /* Helper function to get human readable time*/
    int64_t t = time(NULL);
    char *time_str = ctime(&t);
    time_str[strlen(time_str) - 1] = '\0';

    return time_str;
}