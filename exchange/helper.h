// Preprocessor directives
#include <stdint.h>
#include <arpa/inet.h>
#include <hiredis/hiredis.h>

// Local code
#include "types.h"

// Declare function prototypes
char *get_customer_id(char *message);
void update_cid_ip(cid_ip_t *cid_ip_map, char *cid, char *ip, redisContext *red_con);
void free_cid_ip_map(cid_ip_t *cid_ip_map);
uint64_t add_order_to_redis(redisContext *red_con, order_t *order);
uint64_t add_order_to_redis_details(redisContext *red_con, order_t *order);
uint64_t add_order_to_redis_hash(redisContext *red_con, order_t *order);
uint64_t move_orders_to_exec_queue_redis(redisContext *red_con, order_t *orders);
server_t *get_server(char *env_ip, char *env_port, uint64_t protocol);