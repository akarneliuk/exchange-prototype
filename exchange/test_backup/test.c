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

        // printf("ORDER:\n  cid: %s\n  oid: %lu\n  t_client: %lu\n  t_server: %lu\n  symbol: %s\n  op: %lu\n  price: %.2f\n  qty: %lu\n",
        //        temp_order->cid,
        //        temp_order->oid,
        //        temp_order->t_client,
        //        temp_order->t_server,
        //        temp_order->symbol,
        //        temp_order->operation,
        //        temp_order->price,
        //        temp_order->quantity);

        // Load item to the trading trie with flag init=true to avoid re-loading orders to Redis
        match_trade(tt, temp_order, red_con, true);

        // Set orders num to the vaule of the last existing
        if (head == NULL)
        {
            orders = temp_order->oid;
        }
    }

    printf("Last order id: %lu\n", orders);

    // Cleanup
    free_trie(tt);
    redisFree(red_con);
    free(addr_redis);

    // Success
    return 0;
}