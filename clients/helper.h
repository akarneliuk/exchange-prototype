// Preprocessor directives
#include <stdint.h>
#include <hiredis/hiredis.h>

// Local code
#include "types.h"

// Declare function prototypes
void get_or_create_uuid(char *uuid);
uint64_t get_operation(char *op);
server_t *get_server(char *env_ip, char *env_port, uint64_t protocol);
order_t *get_orders_from_tape(char *tape);
void free_order_list(order_t *order);
int64_t add_order_to_redis(redisContext *red_con, order_t *order, uint64_t my_or_all);
int64_t delete_inactive_quotes_from_redis(redisContext *red_con, uint64_t last_time);
void print_order_from_redis(uint64_t my_or_all);
order_t *deserialize_exhange_confirmation(char *msg);
int64_t process_completed_order_redis(redisContext *red_con, order_t *order);
order_t *deserialize_exchange_confirmation_2(struct order_gateway_request_message_t *ogm);
char *get_human_readable_time();
uint64_t get_time_nanoseconds_midnight();
uint64_t get_time_nanoseconds_since_midnight(uint64_t midnigt);