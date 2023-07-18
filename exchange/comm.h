// Preprocessing
#include <stdint.h>
#include <hiredis/hiredis.h>

// Local code
#include "types.h"

// Decalre function prototypes
uint64_t receive_orders(
    server_t *addr_order,
    u_int64_t orders,
    trading_trie_t *tt,
    cid_ip_t *cid_ip_map,
    redisContext *red_con);