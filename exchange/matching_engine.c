/* This file contains header for the code of building and matching trading trie */

// Preprocessor directives
#include <hiredis/hiredis.h>
#include <ctype.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>

// Local headers
#include "matching_engine.h"
#include "helper.h"

// Define aux functions
trading_trie_t *add_node_to_trie(char symbol)
{
    /* Helper function to initialize trie */
    trading_trie_t *tt = (trading_trie_t *)malloc(sizeof(trading_trie_t));
    if (tt == NULL)
    {
        printf("%lu: Unable to allocate memory for trading trie\n", time(NULL));
        return NULL;
    }

    // initialize next nodes
    for (int i = 0; i < N; i++)
    {
        tt->next[i] = NULL;
    }

    // initialize pointers to orders
    tt->sell_head = NULL;
    tt->sell_tail = NULL;
    tt->buy_head = NULL;
    tt->buy_tail = NULL;

    // initialize symbol
    tt->symbol = symbol;

    // return pointer to the trie
    return tt;
}

void match_trade(trading_trie_t *tt, order_t *order, redisContext *red_con, bool init)
{
    /* Helper function which either builds or matches the entrie in trie*/

    // Create a new pointer to tree for dynamic navigation
    trading_trie_t *tt_ptr = tt;

    // Initialize executed orders list
    order_t *executed_orders = calloc(1, sizeof(order_t));

    // Define flag that will indicate if the order was matched
    bool is_matched = false;

    // Go through the trie to find the symbol or to create one
    int i = 0;
    while (order->symbol[i] != '\0')
    {
        // Normalize char to upper case
        order->symbol[i] = toupper(order->symbol[i]);

        // Check if the next node exists and create if that is not
        if (tt_ptr->next[order->symbol[i] - 'A'] == NULL)
        {
            tt_ptr->next[order->symbol[i] - 'A'] = add_node_to_trie(order->symbol[i]);
        }
        // If exits, move to the next node
        tt_ptr = tt_ptr->next[order->symbol[i] - 'A'];

        i++;
    }

    // When we got to the leaf (final symbol), check if there are already orders to match
    // For buy operation
    if (order->operation == 1)
    {
        printf("%lu: Buy order from '%s' for '%s' with price '%f' and quantity '%lu'\n",
               time(NULL),
               order->cid,
               order->symbol,
               order->price,
               order->quantity);
        // Check if there are any sell orders
        if (tt_ptr->sell_head != NULL)
        {
            printf("%lu: Looking for existing orders in the sell queue for '%s' ...\n", time(NULL), order->symbol);

            // Loop through the sell orders
            order_t *sell_order = tt_ptr->sell_head;
            while (sell_order != NULL && !is_matched)
            {
                printf("%lu: There are orders in the sell queue for '%s'! Checking them...\n", time(NULL), order->symbol);
                printf("%lu: Sell order from '%s' for '%s' with price '%.2f' and quantity '%lu'\n",
                       time(NULL),
                       sell_order->cid,
                       sell_order->symbol,
                       sell_order->price,
                       sell_order->quantity);

                // Check if the buy price is higher or equal to the sell price
                if (order->price >= sell_order->price)
                {
                    printf("%lu: Buy price '%.2f' is higher than or equal to the sell price '%.2f', checking quantity...\n",
                           time(NULL),
                           order->price,
                           sell_order->price);
                    // Check if the buy quantity is equal to the sell quantity
                    if (order->quantity == sell_order->quantity)
                    {
                        printf("%lu: Buy quantity %lu equals to sell quantity %lu. Order is matched!\n",
                               time(NULL),
                               order->quantity,
                               sell_order->quantity);

                        // Remove from the sell list
                        // Relinking the right part of the node
                        if (sell_order->next != NULL)
                        {
                            sell_order->next->previous = sell_order->previous;
                        }
                        else
                        {
                            tt_ptr->sell_tail = sell_order->previous;
                        }

                        // Relinking the left part of the node
                        if (sell_order->previous != NULL)
                        {
                            sell_order->previous->next = sell_order->next;
                        }
                        else
                        {
                            tt_ptr->sell_head = sell_order->next;
                        }

                        // Adjust the price for more prefferable one (i.e, sell more expensive)
                        if (order->price > sell_order->price)
                        {
                            sell_order->price = order->price;
                        }

                        // Add to the executed orders list
                        order_t *temp = executed_orders;
                        executed_orders = sell_order;
                        executed_orders->next = temp;
                        executed_orders->previous = NULL;

                        // Move the buy order to the list of the executed orders
                        temp = executed_orders;
                        executed_orders = order;
                        executed_orders->next = temp;
                        executed_orders->previous = NULL;

                        // Communicate that order is mathced
                        is_matched = true;
                    }
                    // If the quantity is not equal, move to the next sell order
                    else
                    {
                        printf("%lu: Buy quantity %lu doesn't equal sell quantity %lu, moving to the next sell order.\n",
                               time(NULL),
                               order->quantity,
                               sell_order->quantity);

                        sell_order = sell_order->next;
                    }
                }
                // If the buy price is lower than the sell price, move to the next sell order
                else
                {
                    printf("%lu: Buy price '%.2f' is lower than the sell price '%.2f', moving to the next sell order.\n",
                           time(NULL),
                           order->price,
                           sell_order->price);

                    sell_order = sell_order->next;
                }
            }

            // If order is not matched, add it to the buy list
            if (!is_matched)
            {
                printf("%lu: There are no matching buy orders for by of '%s' of '%lu' at '%.2f'. Adding to the queue...\n",
                       time(NULL),
                       order->symbol,
                       order->quantity,
                       order->price);

                // Add to the begining of the list if there are no orders
                if (tt_ptr->buy_head == NULL)
                {
                    tt_ptr->buy_head = order;
                    tt_ptr->buy_tail = order;

                    printf("%lu: Order is added to the beginning of the buy queue for '%s'...\n", time(NULL), order->symbol);
                }
                // Add to the end, if there are orders
                else
                {
                    tt_ptr->buy_tail->next = order;
                    tt_ptr->buy_tail = order;

                    printf("%lu: Order is added to the end of the buy queue for '%s'...\n", time(NULL), order->symbol);
                }

                // Add to Redis
                if (!init)
                {
                    uint64_t add_redis_status = add_order_to_redis(red_con, order);
                    printf("%lu: Order is added to Redis with status '%lu'\n", time(NULL), add_redis_status);
                }
            }
        }
        // If there are no orders to match, add order to the trie
        else
        {
            printf("%lu: There are no orders in the sell queue for '%s', adding to the buy queue...\n", time(NULL), order->symbol);

            // Add to the begining of the list if there are no orders
            if (tt_ptr->buy_head == NULL)
            {
                tt_ptr->buy_head = order;
                tt_ptr->buy_tail = order;

                printf("%lu: Order is added to the beginning of the buy queue for '%s'...\n", time(NULL), order->symbol);
            }
            // Add to the end, if there are orders
            else
            {
                tt_ptr->buy_tail->next = order;
                tt_ptr->buy_tail = order;

                printf("%lu: Order is added to the end of the buy queue for '%s'...\n", time(NULL), order->symbol);
            }

            // Add to Redis
            if (!init)
            {
                uint64_t add_redis_status = add_order_to_redis(red_con, order);
                printf("%lu: Order is added to Redis with status '%lu'\n", time(NULL), add_redis_status);
            }
        }
    }

    // For sell operation
    else if (order->operation == 0)
    {
        printf("%lu: Sell order from '%s' for '%s' with price '%f' and quantity '%lu'\n",
               time(NULL),
               order->cid,
               order->symbol,
               order->price,
               order->quantity);

        // Check if there are any buy orders
        if (tt_ptr->buy_head != NULL)
        {
            printf("%lu: Looking for existing orders in the buy queue for '%s' ...\n", time(NULL), order->symbol);

            // Loop through the buy orders
            order_t *buy_order = tt_ptr->buy_head;
            while (buy_order != NULL && !is_matched)
            {
                printf("%lu: There are orders in the buy queue for '%s'! Checking them...\n", time(NULL), order->symbol);
                printf("%lu: Buy order from '%s' for '%s' with price '%.2f' and quantity '%lu'\n",
                       time(NULL),
                       buy_order->cid,
                       buy_order->symbol,
                       buy_order->price,
                       buy_order->quantity);

                // Check if the sell price is lower or equal to the buy price
                if (order->price <= buy_order->price)
                {
                    printf("%lu: Sell price '%.2f' is lower than or equal to the buy price '%.2f', checking quantity...\n",
                           time(NULL),
                           order->price,
                           buy_order->price);

                    // Check if the buy quantity is equal to the sell quantity
                    if (order->quantity == buy_order->quantity)
                    {

                        printf("%lu: Sell quantity %lu equals to buy quantity %lu. Order is matched!\n",
                               time(NULL),
                               order->quantity,
                               buy_order->quantity);

                        // Remove from the buy list
                        // Relinking the right part of the node
                        if (buy_order->next != NULL)
                        {
                            buy_order->next->previous = buy_order->previous;
                        }
                        else
                        {
                            tt_ptr->buy_tail = buy_order->previous;
                        }

                        // Relinking the left part of the node
                        if (buy_order->previous != NULL)
                        {
                            buy_order->previous->next = buy_order->next;
                        }
                        else
                        {
                            tt_ptr->buy_head = buy_order->next;
                        }

                        // Adjust the price for more prefferable one (i.e, buy cheaper)
                        if (order->price < buy_order->price)
                        {
                            buy_order->price = order->price;
                        }

                        // Add to the executed orders list
                        order_t *temp = executed_orders;
                        executed_orders = buy_order;
                        executed_orders->next = temp;
                        executed_orders->previous = NULL;

                        // Move the sell order to the list of the executed orders
                        temp = executed_orders;
                        executed_orders = order;
                        executed_orders->next = temp;
                        executed_orders->previous = NULL;

                        // Communicate that order is mathced
                        is_matched = true;
                    }
                    // If the quality is not equal, move to the next sell order
                    else
                    {
                        printf("%lu: Sell quantity %lu doesn't equal buy quantity %lu, moving to the next sell order.\n",
                               time(NULL),
                               order->quantity,
                               buy_order->quantity);

                        buy_order = buy_order->next;
                    }
                }
                // If the buy price is lower than the sell price, move to the next sell order
                else
                {
                    printf("%lu: Sell price '%.2f' is higher than the buy price '%.2f', moving to the next sell order.\n",
                           time(NULL),
                           order->price,
                           buy_order->price);

                    buy_order = buy_order->next;
                }
            }

            // If order is not matched, add it to the sell list
            if (!is_matched)
            {
                printf("%lu: There are no matching sell orders for by of '%s' of '%lu' at '%.2f'. Adding to the queue...\n",
                       time(NULL),
                       order->symbol,
                       order->quantity,
                       order->price);

                // Add to the begining of the list if there are no orders
                if (tt_ptr->sell_head == NULL)
                {
                    tt_ptr->sell_head = order;
                    tt_ptr->sell_tail = order;

                    printf("%lu: Order is added to the beginning of the sell queue for '%s'...\n", time(NULL), order->symbol);
                }
                // Add to the end, if there are orders
                else
                {
                    tt_ptr->sell_tail->next = order;
                    tt_ptr->sell_tail = order;

                    printf("%lu: Order is added to the end of the sell queue for '%s'...\n", time(NULL), order->symbol);
                }

                // Add to Redis
                if (!init)
                {
                    int add_redis_status = add_order_to_redis(red_con, order);
                    printf("%lu: Order is added to Redis with status '%i'\n", time(NULL), add_redis_status);
                }
            }
        }
        // If there are no orders to match, add order to the trie
        else
        {
            printf("%lu: There are no orders in the buy queue for '%s', adding to the buy queue...\n", time(NULL), order->symbol);

            // Add to the begining of the list if there are no orders
            if (tt_ptr->sell_head == NULL)
            {
                tt_ptr->sell_head = order;
                tt_ptr->sell_tail = order;

                printf("%lu: Order is added to the beginning of the sell queue for '%s'...\n", time(NULL), order->symbol);
            }
            // Add to the end, if there are orders
            else
            {
                tt_ptr->sell_tail->next = order;
                tt_ptr->sell_tail = order;

                printf("%lu: Order is added to the end of the sell queue for '%s'...\n", time(NULL), order->symbol);
            }

            // Add to Redis
            if (!init)
            {
                int add_redis_status = add_order_to_redis(red_con, order);
                printf("%lu: Order is added to Redis with status '%i'\n", time(NULL), add_redis_status);
            }
        }
    }

    // For cancell operation
    else if (order->operation == 2)
    {
        // Add some code
    }

    // If there are matched orders, push them to executed_orders in Redis
    if (is_matched)
    {
        uint64_t mr1 = add_order_to_redis_details(red_con, order);
        uint64_t mr2 = move_orders_to_exec_queue_redis(red_con, executed_orders);
    }

    // Print list of executed orders for debug purposes
    // print_executed_orders(executed_orders);

    // Cleanup
    free_order_list(executed_orders);
}

void print_trie(trading_trie_t *tt)
{
    /* Helper function to print the trie for debug purpose*/

    // Prints the nodes of the trie
    if (!tt)
        return;

    trading_trie_t *temp = tt;
    printf("%c -> ", temp->symbol);
    for (uint64_t i = 0; i < N; i++)
    {
        print_trie(temp->next[i]);
    }
}

void print_executed_orders(order_t *executed_orders)
{
    /* Helper function to print the executed orders for debug purpose*/

    printf("%lu: EXECUTED ORDERS:\n", time(NULL));

    order_t *head = executed_orders;
    while (head != NULL)
    {
        printf("- cid: %s\n  oid: %lu\n  t_client: %lu\n  t_server: %lu\n  symbol: %s\n  op: %lu\n  price: %f\n  qty: %lu\n",
               head->cid,
               head->oid,
               head->t_client,
               head->t_server,
               head->symbol,
               head->operation,
               head->price,
               head->quantity);
        head = head->next;
    }
}

void free_trie(trading_trie_t *tt)
{
    /* Helper function to clean up the memory used in trie*/
    for (uint64_t i = 0; i < N; i++)
    {
        if (tt->next[i] != NULL)
        {
            free_trie(tt->next[i]);
            // free(tt->next[i]);
        }
    }

    // Cleanup queues
    if (tt->buy_head != NULL)
    {
        free_order_list(tt->buy_head);
    }

    if (tt->sell_head != NULL)
    {
        free_order_list(tt->sell_head);
    }

    // Cleanup
    free(tt);
}

void free_order_list(order_t *executed_orders)
{
    /* Helper function to clean up the memory used in executed orders*/

    // Go through the list to clean memory
    order_t *head = executed_orders;
    while (head != NULL)
    {
        order_t *temp = head;
        head = head->next;
        free(temp);
    }
}