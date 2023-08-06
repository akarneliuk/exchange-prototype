// Preprocessing
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <hiredis/hiredis.h>

// Local code
#include "serializers.h"
#include "helper.h"

// Define aux functions
order_t *deserialize_order_wire(char *message, uint64_t oid)
{
    // Initialize result
    order_t *order = calloc(1, sizeof(order_t));

    // Initialize resources for parsing
    uint64_t n = strlen(message);
    uint64_t c = 0;
    char *buf = calloc(MAX_MSG_LEN, sizeof(char));

    // Parse order
    uint64_t bi = 0;
    for (uint64_t i = 0; i < n; i++)
    {
        // Add the character to the buffer
        if (message[i] != ':')
        {
            buf[bi] = message[i];
            bi++;
        }
        // In case delimiter found
        else
        {
            // Customer ID
            if (c == 0)
            {
                strncpy(order->cid, buf, 36);
            }
            // Customer timestmap
            else if (c == 1)
            {

                order->t_client = strtol(buf, NULL, 10);
            }
            // Operation
            else if (c == 2)
            {
                order->operation = strtol(buf, NULL, 10);
            }
            // Symbol
            else if (c == 3)
            {
                memset(order->symbol, '\0', sizeof(order->symbol));
                uint64_t j = 0;
                while (buf[j] != '\0')
                {
                    order->symbol[j] = buf[j];
                    j++;
                }
            }
            // Qunatity
            else if (c == 4)
            {
                order->quantity = atoi(buf);
            }
            c++;

            // Reset buffer
            free(buf);
            buf = calloc(MAX_MSG_LEN, sizeof(char));

            // Reset index for buffer
            bi = 0;
        }
    }
    // Price
    order->price = strtof(buf, NULL);

    // Set server-side data and default fields
    order->oid = oid;
    order->t_server = get_time_nanoseconds_since_midnight(get_time_nanoseconds_midnight());
    order->next = NULL;
    order->previous = NULL;

    // Cleanup
    free(buf);

    return order;
}

order_t *deserialize_order_redis(redisContext *red_con, char *redis_list)
{
    /*  Helper function to read orders from Redis */

    // Get list of open orders
    redisReply *red_rep1 = redisCommand(red_con, "HKEYS %s", redis_list);

    // Set pointer to null and free it if there are no orders
    if (red_rep1->elements == 0)
    {
        freeReplyObject(red_rep1);
        return NULL;
    }
    // Otherwise, load orders in the linked list
    else
    {
        // Get head
        order_t *head = malloc(sizeof(order_t));
        if (head == NULL)
        {
            perror("ERROR: Cannot allocate memory\n");
            free(head);
            exit(1);
        }

        // Set char memory for zero
        memset(head->cid, '\0', 37);
        memset(head->symbol, '\0', 10);

        // Get tail
        order_t *tail = head;

        for (uint64_t i = 0; i < red_rep1->elements; i++)
        {
            // Get order details from Redis
            redisReply *red_rep2 = redisCommand(red_con, "HVALS %s:%s",
                                                REDIS_EXCHANGE_ORDER_PREFIX,
                                                red_rep1->element[i]->str);

            // Set order details to struct
            tail->oid = atol(red_rep1->element[i]->str);
            strncpy(tail->cid, red_rep2->element[0]->str, 36);
            tail->t_client = atol(red_rep2->element[1]->str);
            tail->t_server = atol(red_rep2->element[2]->str);
            strncpy(tail->symbol, red_rep2->element[3]->str, 10);
            tail->operation = atol(red_rep2->element[4]->str);
            tail->price = atof(red_rep2->element[5]->str);
            tail->quantity = atol(red_rep2->element[6]->str);
            tail->previous = NULL;
            tail->next = NULL;

            freeReplyObject(red_rep2);

            // Allocate memory for new node only if this is not the last element
            if (i != red_rep1->elements - 1)
            {
                // Create new end node
                order_t *new_order = calloc(1, sizeof(order_t));

                //  Update tail to point to new order
                tail->next = new_order;
                tail = tail->next;
            }
        }

        // Cleanup
        freeReplyObject(red_rep1);

        // Return sequenced orders
        return head;
    }
}

cid_ip_t *deserialize_cid_ip_redis(redisContext *red_con, char *redis_list)
{
    /*  Helper function to read customer to IP mapping from Redis */

    // Get list of open orders
    redisReply *red_rep1 = redisCommand(red_con, "HGETALL %s", redis_list);

    // Set pointer to null and free it if there are no orders
    if (red_rep1->elements == 0)
    {
        freeReplyObject(red_rep1);
        return NULL;
    }

    // Build mapping
    cid_ip_t *head = malloc(sizeof(cid_ip_t));
    if (head == NULL)
    {
        printf("ERROR: Cannot allocate memory\n");
        free(head);
        exit(1);
    }

    cid_ip_t *tail = head;
    // Loop through all entries returned by Redis
    for (uint64_t i = 0; i < red_rep1->elements; i += 2)
    {
        // Get customer ID
        tail->cid = calloc(37, sizeof(char));
        memcpy(tail->cid, red_rep1->element[i]->str, 36);

        // Get IP
        tail->ip = calloc(16, sizeof(char));
        memcpy(tail->ip, red_rep1->element[i + 1]->str, 15);

        // Allocate memory for new node only if this is not the last element
        if (i != red_rep1->elements - 2)
        {
            // Create new end node
            cid_ip_t *new_mapping = malloc(sizeof(cid_ip_t));

            //  Update tail to point to new order
            tail->next = new_mapping;
            tail = tail->next;
        }
        // Explicitly null next at the end of the linked list
        else
        {
            tail->next = NULL;
        }
    }

    // Cleanup
    freeReplyObject(red_rep1);

    // Return orders
    return head;
}