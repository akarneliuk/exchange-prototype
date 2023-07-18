/* This file contains header for the bespoke logic of building trading trie */

// Preprocessor directives
#include <stdbool.h>
#include <hiredis/hiredis.h>

// Local headers
#include "types.h"

// Declare function prototypes
trading_trie_t *add_node_to_trie(char symbol);
void match_trade(trading_trie_t *tt, order_t *order, redisContext *red_con, bool init);
void free_trie(trading_trie_t *tt);
void free_order_list(order_t *executed_orders);
void print_trie(trading_trie_t *tt);
void print_executed_orders(order_t *executed_orders);