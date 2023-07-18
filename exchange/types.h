/* This file contains custom data types used in this app*/

// Preprocessor directives
#include <stdint.h>
#include <arpa/inet.h>

// Statics
// Communication data
#define MAX_MSG_LEN 1024
#define EXCHANGE_ORDER_PROTOCOL IPPROTO_TCP
#define EXCHANGE_MCAST_PROTOCOL IPPROTO_UDP
#define CUSTOMER_PROTOCOL IPPROTO_TCP

// Redis data
#define REDIS_EXCHANGE_A_ORDERS "active_orders"
#define REDIS_EXCHANGE_E_ORDERS "executed_orders"
#define REDIS_EXCHANGE_C2IP "c2ip"
#define REDIS_EXCHANGE_ORDER_PREFIX "order"

// Trie data
#define N 26

// Custom data types
#ifndef _MY_HEADER_H_
#define _MY_HEADER_H_

typedef struct order_t
{
    char cid[37];
    uint64_t oid;
    uint64_t t_client;
    uint64_t t_server;
    char symbol[11];
    uint64_t operation;
    float price;
    uint64_t quantity;
    struct order_t *next;
    struct order_t *previous;
} order_t;

typedef struct trading_trie_t
{
    char symbol;
    struct trading_trie_t *next[N];
    struct order_t *sell_head;
    struct order_t *sell_tail;
    struct order_t *buy_head;
    struct order_t *buy_tail;

} trading_trie_t;

typedef struct cid_ip_t
{
    char *cid;
    char *ip;
    struct cid_ip_t *next;
} cid_ip_t;

typedef struct server_t
{
    char ip[16];
    uint64_t protocol;
    uint64_t port;
} server_t;

#endif /* _MY_HEADER_H_ */