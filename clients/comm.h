// Preprocessing
#include <stdint.h>

// Local code
#include "types.h"

// Statics
#define MAX_MSG_LEN 1024

// Decalre function prototypes
uint64_t send_order(char *client_id, order_t *order, server_t *server);