// Preprocessor directives
#include <stdint.h>
#include <hiredis/hiredis.h>

// Local code
#include "types.h"

// Declare function prototypes
order_t *deserialize_order_wire(char *message, uint64_t oid);
order_t *deserialize_order_redis(redisContext *red_con, char *redis_list);
cid_ip_t *deserialize_cid_ip_redis(redisContext *red_con, char *redis_list);