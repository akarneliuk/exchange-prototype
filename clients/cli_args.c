// Preprocessor directives
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Local code
#include "types.h"
#include "helper.h"

// Define auxiliary functions
order_t *process_cli_args(int argc, char *argv[])
{
    // Execution without arguments
    if (argc == 1)
    {
        printf("You haven't provided any arguments. Run `%s help` for details.\n", argv[0]);
        exit(0);
    }

    // Print help
    else if (argc == 2 && memcmp(argv[1], "help", 4) == 0)
    {
        printf("The following operations are available in the client:\n");

        // Print help to get quotes
        printf("\n - Run `%s list` to list all open quotes\n", argv[0]);
        printf("     * Add flag `--my` to list only your own open quotes\n");

        // Print help to buy/sell shares
        printf("\n - Run `%s buy/sell SYMBOL AMOUNT PRICE` to send order to buy/sell shares.\n", argv[0]);
        printf("     * SYMBOL - string with values `buy` or `sell`\n");
        printf("     * AMOUNT - positive interger with possible range \n");
        printf("     * PRICE  - positive float, truncated rounded to 2 digits after dot\n");
        printf("Example: %s buy AAPL 100 100.00\n", argv[0]);

        // Print help to cancel order
        printf("\n - Run `%s cancel ID` to cancel the particular order.\n", argv[0]);
        printf("     * ID     - is your unique order ID, which you can get in the list above.\n\n");

        exit(0);
    }

    // Print the content of all quotes
    else if (argc == 2 && memcmp(argv[1], "list", 4) == 0)
    {
        // Print all the orders
        printf("All orders listed now:\n");
        print_order_from_redis(0);
        exit(0);
    }

    // Print the content of the own quotes
    else if (argc == 3 && memcmp(argv[1], "list", 4) == 0 && memcmp(argv[2], "--my", 4) == 0)
    {
        // Print only own orders
        printf("All YOUR orders listed now:\n");
        print_order_from_redis(1);
        exit(0);
    }

    // Return order to buy/sell shares
    else if (argc == 5 && (memcmp(argv[1], "buy", 3) == 0 || memcmp(argv[1], "sell", 4) == 0))
    {

        // Check that amount is positive integer
        if (strtol(argv[3], NULL, 10) <= 0)
        {
            printf("ERROR: AMOUNT should be positive integer\n");
            exit(1);
        }

        // Check that price is positive float
        if (strtof(argv[4], NULL) <= 0)
        {
            printf("ERROR: PRICE should be positive float\n");
            exit(1);
        }

        order_t *order = calloc(1, sizeof(order_t));
        if (order == NULL)
        {
            printf("ERROR: Unable to allocate memory for order\n");
            exit(2);
        }
        memset(order->symbol, '\0', sizeof(order->symbol));

        order->t_client = time(NULL);
        order->operation = get_operation(argv[1]);
        order->quantity = (int32_t)strtol(argv[3], NULL, 10);
        order->price = strtof(argv[4], NULL);

        // Copy no more than 10 characters
        uint64_t copy_len = strlen(argv[2]) < 10 ? strlen(argv[2]) : 10;
        memcpy(order->symbol, argv[2], copy_len);

        return order;
    }

    // Return order to cancel
    else if (argc == 3 && memcmp(argv[1], "cancel", 6) == 0 && strtol(argv[2], NULL, 10) > 0)
    {
        order_t *order = calloc(1, sizeof(order_t));
        if (order == NULL)
        {
            printf("ERROR: Unable to allocate memory for order\n");
            exit(2);
        }
        order->t_client = time(NULL);
        order->operation = get_operation(argv[1]);
        order->oid = strtol(argv[2], NULL, 10);

        return order;
    }

    // Fail if so far no matches
    else
    {
        printf("Invalid arguments. Run `%s help` for details.\n", argv[0]);
        exit(1);
    }
}