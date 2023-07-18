// Preprocessing
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <hiredis/hiredis.h>

// Local code
#include "helper.h"
#include "comm.h"
#include "matching_engine.h"
#include "serializers.h"

// Main function
int main(void)
{
    // Get connection details
    server_t *addr_redis = get_server("REDIS_IP", "REDIS_PORT", IPPROTO_TCP);
    server_t *addr_order = get_server("EXCHANGE_ORDER_IP", "EXCHANGE_ORDER_PORT", IPPROTO_TCP);

    // Initialize matching engine
    uint64_t orders = 0; // Later this will be reading existing order queue from Redis

    // Initialize trading trie
    trading_trie_t *tt = add_node_to_trie('\0');

    // Open connection to Redis
    redisContext *red_con = redisConnect(addr_redis->ip, addr_redis->port);
    if (red_con != NULL && red_con->err)
    {
        printf("%lu: Error: %s\n", time(NULL), red_con->errstr);
        return 19;
    }
    printf("%lu: Connected to Redis\n", time(NULL));

    // Read orders from Redis
    order_t *order = deserialize_order_redis(red_con, REDIS_EXCHANGE_A_ORDERS);

    // Load orders read from Redis in Trading Trie
    order_t *head = order;
    while (head)
    {
        // Create temp pointer to be able to NULL the next field
        order_t *temp_order = head;

        // Page head and NULL next in temp_order, which is added to trie
        head = head->next;
        temp_order->next = NULL;

        // Load item to the trading trie with flag init=true to avoid re-loading orders to Redis
        match_trade(tt, temp_order, red_con, true);

        // Set orders num to the vaule of the last existing
        if (head == NULL)
        {
            orders = temp_order->oid;
        }
    }

    printf("Next order ID is: %lu\n", orders);

    // Initialize customer's IP to CID mapping
    cid_ip_t *cid_ip_map = malloc(sizeof(cid_ip_t));
    cid_ip_map->cid = calloc(37, sizeof(char));
    cid_ip_map->ip = calloc(16, sizeof(char));
    cid_ip_map->next = NULL;

    // Print welcome message
    printf("%lu: Exchange Order Server started!\n", time(NULL));
    printf("%lu: So far %lu orders to match\n", time(NULL), orders);

    // Launch the server to recive orders
    receive_orders(addr_order, orders, tt, cid_ip_map, red_con);

    // Cleanup
    free_trie(tt);
    redisFree(red_con);
    free(addr_redis);
}