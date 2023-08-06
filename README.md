# Stock Exchange Prototype

## Part 1: CS50 Final project
Originally this project as a part of the course "CS50" at the Harvard University. 

#### Video Demo: https://youtu.be/vo_BSJMRFsM

#### Description
This project is created as part of the course "CS50" at the Harvard University. It contains the prototype of the stock exchange, which allows the customers to buy/sell stocks via central platform (exchange). The project is created in C and uses Redis DB as a backend for storing the orders and trading market_data. C was chosen as the most appropriate langauge for creating networking apps due to its capability to operate network sockets. Moreover, C has much better performance than any other language if the application is written correctly, and the speed is the essence in the trading world. Redis was chosen as a tool to provide easy and efficient communication between different apps on each side.

##### Highlights
This project covers the following aspects of Computer Science and Software Development:
- Network communication between applications relying (development and usage of sockets in C)
    - IPv4 multicast with UDP
    - IPv4 unicast with TCP
- Communication between the applications within the host using Redis DB (development and usage of Redis DB in C)
    - Creating data structures in Redis DB and updating them
    - Reading data structures from Redis DB
- Efficient data processing and analysis in C for matching engine:
    - Usage of the Trie to create the tree of the orders using ticker symbol characters as trie nodes with two double linked list of buy/sell orders queues as keys of the trie nodes.
- Dynamic input to C progamms:
    - Usage of Linux environment variables to pass the configuration to the applications
    - Usage of the CLI arguments to view/place orders by human operators

##### Structure
The project contains two main parts:
- **Exchange side**. This set of applications runs on the exchange side and is responsible for:
    - Receiving orders from clients.
    - Matching the orders and executing them including notification customers, which orders are executed, about the execution.
    - Sending the trading market_data to all the clients with all the active trades.
- **Customer side**. The set of applications that runs on the customer side and is responsible for:
    - Receiving the trading market_data from the exchange.
    - Sending the orders to the exchange.
    - Receiving the notification from the exchange about the execution of the order. 

###### Exchange side
This part contains three applications:
- `market_data`: This is trading market_data that contains the actuall buy/sell prices for the traded symbols. It is refreshed every 1 second and is sent to the clients via IPv4 multicast on the custom port.
- `order`: This is the matching engine, which receives the customer requests, when they want to buy or sell the stocks based on the current prices. It matches the requests and either buy/sell stocks if the correspoding matching oposite order is found or adds the order to Redis DB so that adds it to announcmement. sends the response to the customer via TCP/unicast.
- `exec`: This app is responsible for executing the orders. It polls the Redis DB every 500 ms and checks if there are any orders to be executed. If yes, then it executes them and sends the response to the customer via TCP/unicast.

###### Customer side
This part contains three application:
- `client_s`: This is the trading application that allows the user to buy/sell stocks. This application sends the request to the exchange and receives the response if the order was added to the orderbook or not.
- `client_l_m`: This is the client application that receives the trading market_data from the exchange via IPv4 multicast and updates the local Redis DB with the current offers (operation, symbol, price, quantity).
- `client_l_u`: This is the client application that receives the unicast notification from the exchange when the order is executed and updates the local Redis DB.

###### Communication
Network communication is a crucial part of this project. Therefore, the followig communication flows were introduced: 
1. market_data sends data to:
    1. IPv4 multicast address: `239.11.22.33`
    2. UDP Port: `11001`
2. Order receive data on:
    1. IPv4 IP address of the host
    2. TCP Port: `11001`
3. Exec sends data to:
    1. IPv4 IP address of the customer
    2. TCP Port: `11002`
4. Clients receive data on:
    1. IPv4 multicast address: `239.11.22.33`
    2. UDP Port: `11001`
5. Clients sends data to (order):
    1. IPv4 address of order service
    2. TCP Port: `11001`
6. Clients receive data on (notification that trade is executed):
    1. IPv4 IP address of the host
    2. TCP Port: `11002`

##### Requirements
In order for the application to run, the following Linux packages are required:
- For UUID generation:
    - `libuuid1`
    - `uuid-dev`
- For Redis DB:
    - `redis-server`
    - `libhiredis-dev`
- For build:
    - `make`
    - `gcc`
    - `clang`
    - `build-essential`

Everything besides Redis is installed using the following command:
```
$ sudo apt-get install -y git make gcc clang build-essential uuid uuid-dev
```

For Redis dependencies, which andatory for redis C client, it was decided to build them locally. There is a separate guide `redis/README.md`, which explains how to build it.

##### Usage
For each side ti is decided to build application locally; however, it is possible to use binary files compiled during the development, as the development host runs the same operationg system Ubuntu Linux 22.04.2LTS and the same architecture `x86_64` as the target host on exchange/client sides.

###### Exchange side
Build the application:
```
$ make
```

Prepare the environment variables per the network topology and source them:
```
$ tee env.sh << __EOF__
#!/usr/bin/env bash
#!/usr/bin/env bash
export EXCHANGE_ORDER_IP="192.168.51.31"
export EXCHANGE_ORDER_PORT="11001"
export EXCHANGE_market_data_IP="239.11.22.33"
export EXCHANGE_market_data_PORT="11001"
export EXCHANGE_market_data_SOURCE_IP="192.168.51.31"
export CUSTOMER_PORT="11002"
export REDIS_IP="127.0.0.1"
export REDIS_PORT="6379"
__EOF__


$ source env.sh
```

First, launch the `market_data` application:
```
$ ./market_data
```

Then, launch the `exec` application:
```
$ ./exec
```

Finally, launch the `order` application:
```
$ ./order
```

###### Customer side
Build the application:
```
$ make
```

Prepare the environment variables per the network topology and source them:
```
$ tee env.sh << __EOF__
#!/usr/bin/env bash
export EXCHANGE_ORDER_IP="192.168.51.31"
export EXCHANGE_ORDER_PORT="11001"
export EXCHANGE_market_data_IP="239.11.22.33"
export EXCHANGE_market_data_PORT="11001"
export CUSTOMER_PORT="11002"
export CUSTOMER_IP_ACCEPT_MULTICAST="192.168.51.32"
export CUSTOMER_IP_ACCEPT_UNICAST="192.168.51.32"
export REDIS_IP="127.0.0.1"
export REDIS_PORT="6379"
__EOF__


$ source env.sh
```
*It was decided to give user possibility to distinguish interfaces, where they receive the unicast and multicast traffic, if they want to do so.*

First, launch the `client_l_m` application:
```
$ ./client_l_m
```

Then, launch the `client_l_u` application:
```
$ ./client_l_u
```

Finally, interact with the exchange using `client_s` application with the corresponding CLI arguments:
```
$ ./client_s
```

##### Logs
Each application prints logs in the stdout to verify its operation and provide some visibility for users. Arguably, in production many logs can be truncated as printing to stdout is a costly operation. 

## Part 2: Life after CS50
After the course was finished, it was decided to continue the development of the project and make it more realistic. Stay tuned.

### after-cs50-1 sprint
- Replace `inet_addr` to `inet_pton` to provide IPv4/IPv6 support.
- Introduce `perror` to print the error messages.
- Replace time functions to `clock_gettime` to provide nanosecond precision.
- Replace `malloc()` with `calloc()` to initialize the memory with zeros where appropriate.
- Add `memset()` upon all structs initialization to zero all values.
- Rename `tape` to `market_data` to be more consistent with the terminology.
- Migrated from `clang` to `gcc` as that seem to be a standard for C development.
- Added `SO_REUSEADDR` to the socket options to allow the socket to be reused for TCP servers

### after-cs50-2 sprint
- Add `shutdown()` to close the socket properly for TCP sessions.
- Use `poll()` on `exec` to send data to the clients.
- Split `order` into `order_gateway` (user-facing application) and `trading_unit` (matching engine part).

### after-cs50-3 sprint
- Implement `Nasdaq Basic` protocol to provide the market_data to the clients.