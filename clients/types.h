/* This file contains custom data types used in this app*/

// Preprocessor directives
#include <stdint.h>

// Statics
#define MAX_MSG_LEN 1024
#define REDIS_CUSTOMER_ALL_ORDERS "customer_all_orders"
#define REDIS_CUSTOMER_MY_ORDERS "customer_my_orders"
#define REDIS_CUSTOMER_ORDER_PREFIX "c-order"
#define LISTENQ 10

// Data types
#ifndef _MY_HEADER_H_
#define _MY_HEADER_H_

typedef struct order_t
{
    uint64_t t_client;
    uint64_t t_server;
    uint64_t oid;
    char symbol[10];
    uint64_t operation;
    uint64_t quantity;
    float price;
    struct order_t *next;
} order_t;

typedef struct server_t
{
    char ip[16];
    uint64_t protocol;
    uint64_t port;
} server_t;

// Message specifications
typedef struct order_gateway_request_message_t
{
    uint64_t order_id;
    uint64_t ts_placed;
    uint64_t ts_executed;
    char status;

} __attribute__((packed)) order_gateway_request_message_t;

typedef struct order_gateway_response_message_t
{
    uint64_t order_id;
    uint64_t ts_ack;
    char status;

} __attribute__((packed)) order_gateway_response_message_t;

#endif /* _MY_HEADER_H_ */