/* This code aims to send the buy/sell requests to Exchange and receive the result execution */

// Preprocessor directives
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <arpa/inet.h>

// Local code
#include "helper.h"
#include "comm.h"
#include "cli_args.h"

// Main function
int main(int argc, char *argv[])
{
    // Parse provided arguments
    order_t *order = process_cli_args(argc, argv);

    // Read environment variables
    server_t *server = get_server("EXCHANGE_ORDER_IP", "EXCHANGE_ORDER_PORT", IPPROTO_TCP);

    // Obtain UUID
    char *client_id = calloc(1, 37 * sizeof(char));
    get_or_create_uuid(client_id);

    // Print notification
    if (send_order(client_id, order, server) == 0)
    {
        printf("Order sent successfully\n");

        // Update Redis
        server_t *addr_redis = get_server("REDIS_IP", "REDIS_PORT", IPPROTO_TCP);
        redisContext *red_con = redisConnect(addr_redis->ip, addr_redis->port);
        if (add_order_to_redis(red_con, order, 1) < 0)
        {
            perror("Error: Cannot add the order to Redis: ");
        }
        redisFree(red_con);
    }
    else
    {
        printf("Error while sending order\n");
    }

    // Clean up
    free(client_id);
    free(order);
    free(server);
}