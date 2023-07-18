// Preprocessor directives
#include <time.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <hiredis/hiredis.h>
#include <arpa/inet.h>

// Local code
#include "helper.h"

// Define aux functions
char *get_customer_id(char *message)
{
    /* Helper function to subtract the Customer ID from the message*/

    // Initialize customer id
    char *cid = (char *)calloc(1, MAX_MSG_LEN * sizeof(char));
    if (cid == NULL)
    {
        printf("%lu: Unable to allocate memory for customer id\n", time(NULL));
        return NULL;
    }

    // Extract customer id
    int i = 0;
    while (message[i] != ':')
    {
        cid[i] = message[i];
        i++;
    }

    // Return customer id
    return cid;
}

void update_cid_ip(cid_ip_t *cid_ip_map, char *cid, char *ip, redisContext *red_con)
{
    /* Helper function to update the CID to IP mapping*/

    cid_ip_t *head = cid_ip_map;
    bool is_updated = false;

    // Check if the existing mapping is blank
    if (head->cid[0] == '\0')
    {
        strcpy(head->cid, cid);
        strcpy(head->ip, ip);

        is_updated = true;
    }
    // In case it is not null, start futher comparisson
    else
    {
        // Check if the current client id is this
        while (strcmp(head->cid, cid) != 0)
        {
            // Check if the next mapping doesn't exist
            if (head->next == NULL)
            {
                // Create a new mapping
                head->next = malloc(sizeof(cid_ip_t));

                // Set Customer ID in new mapping
                head->next->cid = calloc(37, sizeof(char));
                strncpy(head->next->cid, cid, 36);

                // Set Customer IP in new mapping
                head->next->ip = calloc(16, sizeof(char));
                strncpy(head->next->ip, ip, 15);

                head->next->next = NULL;

                is_updated = true;
            }
            // Move to the next mapping
            else
            {
                head = head->next;
            }
        }
    }

    if (is_updated)
    {
        printf("%lu: Updated CID to IP mapping: CID '%s' with IP '%s'.\n", time(NULL), cid, ip);

        // Update the mapping in Redis
        redisReply *red_rep = redisCommand(red_con, "HSET %s %s %s", REDIS_EXCHANGE_C2IP, cid, ip);
    }
}

void free_cid_ip_map(cid_ip_t *cid_ip_map)
{
    /* Helper function to free the CID to IP mapping*/

    // Initialize head
    cid_ip_t *head = cid_ip_map;

    // Free the mapping
    while (head != NULL)
    {
        cid_ip_t *temp = head;
        head = head->next;
        free(temp->cid);
        free(temp->ip);
        free(temp);
    }
}

uint64_t add_order_to_redis(redisContext *red_con, order_t *order)
{
    /* Helper function to add active order to redis*/

    // Create Hash with order details
    uint64_t rr1 = add_order_to_redis_details(red_con, order);
    if (rr1 != 0)
    {
        return rr1;
    }

    // Add order to the hash queue
    uint64_t rr2 = add_order_to_redis_hash(red_con, order);
    if (rr2 != 0)
    {
        return rr2;
    }

    // Success
    return 0;
}

uint64_t add_order_to_redis_details(redisContext *red_con, order_t *order)
{
    /* Helper function to create Redis hash for an order */

    // Create Hash with order details
    redisReply *red_rep = redisCommand(red_con, "HSET %s:%lu cid %s t_client %lu t_server %lu symbol %s op %i price %.2f qty %i",
                                       REDIS_EXCHANGE_ORDER_PREFIX,
                                       order->oid,
                                       order->cid,
                                       order->t_client,
                                       order->t_server,
                                       order->symbol,
                                       order->operation,
                                       order->price,
                                       order->quantity);
    if (red_rep->str != NULL)
    {
        printf("%lu: Unable to create order details in Redis: %s\n", time(NULL), red_rep->str);
        freeReplyObject(red_rep);
        return 1;
    }
    else
    {
        printf("%lu: Order details are created in Redis.\n", time(NULL));
        freeReplyObject(red_rep);
    }

    // Success
    return 0;
}

uint64_t add_order_to_redis_hash(redisContext *red_con, order_t *order)
{
    /* Helper function to add an order to the Redis hash/queue with the orders */

    // Add order to the hash queue
    redisReply *red_rep = redisCommand(red_con, "HSET %s %lu %lu", REDIS_EXCHANGE_A_ORDERS, order->oid);
    if (red_rep->str != NULL)
    {
        printf("%lu: Unable to add order to the active queue in Redis: %s\n", time(NULL), red_rep->str);
        freeReplyObject(red_rep);
        return 1;
    }
    else
    {
        printf("%lu: Order is added to the active queue in Redis.\n", time(NULL));
        freeReplyObject(red_rep);
    }

    // Success
    return 0;
}

server_t *get_server(char *env_ip, char *env_port, uint64_t protocol)
{
    /* Helper function to get server details from environment variables*/

    // Allocate memory for server
    server_t *server = malloc(sizeof(server_t));

    // Get server IP
    char *ip = getenv(env_ip);
    if (ip == NULL)
    {
        printf("EXCHANGE_ORDER_IP environment variable not set\n");
        exit(1);
    }
    strncpy(server->ip, ip, 15);

    // Get server port
    char *port = getenv(env_port);
    if (port == NULL)
    {
        printf("EXCHANGE_ORDER_PORT environment variable not set\n");
        exit(1);
    }
    server->port = atoi(port);

    // Set the protocol
    server->protocol = protocol;

    return server;
}

uint64_t move_orders_to_exec_queue_redis(redisContext *red_con, order_t *orders)
{
    /* Function to move orders from active_orders hash to executed_orders */

    order_t *head = orders;

    // Page through all executed orders
    while (head != NULL)
    {
        // Ignore placeholder
        if (head->oid != 0)
        {
            // Add to executed orders hash
            redisReply *red_rep1 = redisCommand(red_con, "HSET %s %lu %lu",
                                                REDIS_EXCHANGE_E_ORDERS,
                                                head->oid,
                                                head->oid);

            if (red_rep1->str != NULL)
            {
                printf("%lu: Unable to add order %lu to the executed queue in Redis: %s\n", time(NULL), head->oid, red_rep1->str);
                freeReplyObject(red_rep1);
                return 1;
            }
            else
            {
                printf("%lu: Order %lu is added to the executed queue in Redis.\n", time(NULL), head->oid);
            }
            freeReplyObject(red_rep1);

            // Remove from active orders
            redisReply *red_rep2 = redisCommand(red_con, "HDEL %s %lu",
                                                REDIS_EXCHANGE_A_ORDERS,
                                                head->oid);
            if (red_rep2->str != NULL)
            {
                printf("%lu: Unable to delete order %lu from the active queue in Redis: %s\n", time(NULL), head->oid, red_rep2->str);
                freeReplyObject(red_rep2);
                return 1;
            }
            else
            {
                printf("%lu: Order %lu is deleted from the active queue in Redis.\n", time(NULL), head->oid);
            }
            freeReplyObject(red_rep2);
        }

        head = head->next;
    }

    // return success
    return 0;
}