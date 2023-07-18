/* Test with reading orders from file*/

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

    // Load orders
    FILE *fptr = fopen("test_input.txt", "r");
    if (fptr == NULL)
    {
        printf("%lu: Unable to open test_input.txt\n", time(NULL));
        return 1;
    }

    // Open connection to Redis
    redisContext *red_con = redisConnect(addr_redis->ip, addr_redis->port);
    if (red_con != NULL && red_con->err)
    {
        printf("%lu: Error: %s\n", time(NULL), red_con->errstr);
        return 19;
    }
    printf("%lu: Connected to Redis\n", time(NULL));

    // Simulate read from wire
    char *buf = calloc(MAX_MSG_LEN, sizeof(char));
    while (fscanf(fptr, "%s", buf) != EOF)
    {
        orders++;

        order_t *order = deserialize_order_wire(buf, orders);
        printf("ORDER:\n  cid: %s\n  oid: %lu\n  t_client: %lu\n  t_server: %lu\n  symbol: %s\n  op: %lu\n  price: %.2f\n  qty: %lu\n",
               order->cid,
               order->oid,
               order->t_client,
               order->t_server,
               order->symbol,
               order->operation,
               order->price,
               order->quantity);

        // Test trading trie
        match_trade(tt, order, red_con, false);

        // Reallocate buffer
        free(buf);
        buf = calloc(MAX_MSG_LEN, sizeof(char));
    }
    free(buf);

    // Cleanup
    free_trie(tt);
    fclose(fptr);
    redisFree(red_con);

    // Success
    return 0;
}