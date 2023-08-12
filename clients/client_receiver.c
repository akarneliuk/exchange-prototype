/* Combined client for unicast/multicast traffic receiving */
// Preprocessor directives
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/poll.h>
#include <arpa/inet.h>
#include <hiredis/hiredis.h>
#include <linux/in.h>
#include <errno.h>
#include <byteswap.h>

// Local code
#include "helper.h"
#include "comm.h"

int main(void)
{
    // GENERAL: Get UUID
    char *client_id = calloc(1, 37 * sizeof(char));
    get_or_create_uuid(client_id);

    // POLL: Declare variables for polling
    struct pollfd open_fds[FOPEN_MAX];
    uint64_t max_fd_list_id, fd_ind;
    int64_t fd_ready;

    // POLL: Initialize list of pollable descriptors
    for (uint64_t i = 0; i < FOPEN_MAX; i++)
    {
        open_fds[i].fd = -1;
    }

    // TCP + UDP: Get connection details for TCP and UDP connections
    server_t *addr_mcast_group = get_server("EXCHANGE_MARKET_DATA_IP_MCAST_GROUP", "EXCHANGE_MARKET_DATA_L4_PORT", IPPROTO_UDP);
    server_t *addr_mcast_local = get_server("CUSTOMER_IP_ACCEPT_MCAST", "EXCHANGE_MARKET_DATA_L4_PORT", IPPROTO_UDP);
    server_t *addr_ucast_local = get_server("CUSTOMER_IP_ACCEPT_UCAST", "CUSTOMER_L4_PORT", IPPROTO_TCP);
    server_t *addr_redis = get_server("REDIS_IP", "REDIS_PORT", IPPROTO_TCP);

    // TCP + UDP: Initialize buffer
    ssize_t recv_bytes = 0;
    char recv_buf[MAX_MSG_LEN];
    memset(recv_buf, 0, sizeof(recv_buf));

    // TCP: Initialize socket
    int64_t tcp_listed_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (tcp_listed_fd < 0)
    {
        perror("Error: TCP: Cannot create socket: ");
        return 1;
    }
    printf("%s: TCP: socket created successfully\n", get_human_readable_time());

    // TCP: Initialize server listen address
    struct sockaddr_in tcp_server_addr;
    memset(&tcp_server_addr, 0, sizeof(tcp_server_addr));
    tcp_server_addr.sin_family = AF_INET;
    tcp_server_addr.sin_port = htons(addr_ucast_local->port);
    if (inet_pton(tcp_server_addr.sin_family, addr_ucast_local->ip, &tcp_server_addr.sin_addr) < 0)
    {
        perror("Error: TCP: Uncompatible IP Address: ");
        return 2;
    }

    // TCP: Prepare for customer socket/address initaialization
    int64_t tcp_client_fd;
    struct sockaddr_in tcp_client_addr;
    memset(&tcp_client_addr, 0, sizeof(tcp_client_addr));
    socklen_t tcp_client_addr_len = sizeof(tcp_client_addr);

    // TCP: Allow reusing the IP/Port to be more resilient during crashes
    uint64_t tcp_so_reuseaddr = 1;
    if (setsockopt(tcp_listed_fd, SOL_SOCKET, SO_REUSEADDR, &tcp_so_reuseaddr, sizeof(tcp_so_reuseaddr)) < 0)
    {
        perror("Error: TCP: Cannot set socket option: ");
        return 3;
    }

    // TCP: Bind socket
    if (bind(tcp_listed_fd, (struct sockaddr *)&tcp_server_addr, sizeof(tcp_server_addr)) < 0)
    {
        perror("Error: TCP: Cannot bind socket: ");
        return 4;
    }

    // TCP: Listen on socket
    if (listen(tcp_listed_fd, LISTENQ) < 0)
    {
        perror("Error: TCP: Cannot listen on socket: ");
        return 5;
    }

    // TCP + POLL: Add TCP Listen socket to poll list
    open_fds[0].fd = tcp_listed_fd;
    open_fds[0].events = POLLIN;

    // UDP: Initialize socket
    int64_t udp_listen_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_listen_fd < 0)
    {
        perror("Error: UDP: Cannot create socket: ");
        return 1;
    }
    printf("%s: UDP: socket created successfully\n", get_human_readable_time());

    // UDP: Initialize server listen address (MCAST Group)
    struct sockaddr_in udp_server_addr;
    memset(&udp_server_addr, 0, sizeof(udp_server_addr));
    udp_server_addr.sin_family = AF_INET;
    udp_server_addr.sin_port = htons(addr_mcast_group->port);
    if (inet_pton(udp_server_addr.sin_family, addr_mcast_group->ip, &udp_server_addr.sin_addr) < 0)
    {
        perror("Error: UDP: Uncompatible IP Address: ");
        return 2;
    }

    // UDP: Allow reuse of same port as it can be used in different MCAST Groups
    uint64_t udp_so_reuseaddr = 1;
    if (setsockopt(udp_listen_fd, SOL_SOCKET, SO_REUSEADDR, &udp_so_reuseaddr, sizeof(udp_so_reuseaddr)) < 0)
    {
        perror("Error: UDP: Cannot set socket option: ");
        return 3;
    }

    // UDP: Bind socket
    if (bind(udp_listen_fd, (struct sockaddr *)&udp_server_addr, sizeof(udp_server_addr)) < 0)
    {
        perror("Error: UDP: Cannot bind socket: ");
        return 4;
    }

    // UDP: Initialize request to join multicast (somehow <linux/in.h> doesn't work for me)
    struct ip_mreq
    {
        /* IP multicast address of group.  */
        struct in_addr imr_multiaddr;

        /* Local IP address of interface.  */
        struct in_addr imr_interface;
    };
    struct ip_mreq mreq;
    memset(&mreq, 0, sizeof(mreq));
    if (inet_pton(udp_server_addr.sin_family, addr_mcast_group->ip, &mreq.imr_multiaddr) < 0)
    {
        perror("Error: UDP: Uncompatible IP Address: ");
        return 2;
    }
    if (inet_pton(udp_server_addr.sin_family, addr_mcast_local->ip, &mreq.imr_interface) < 0)
    {
        perror("Error: UDP: Uncompatible IP Address: ");
        return 2;
    }

    // UDP: Join multicast group
    if (setsockopt(udp_listen_fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0)
    {
        perror("Error: Cannot join multicast group: ");
        return 5;
    }

    // UDP + POLL: Add socket for polling
    open_fds[1].fd = udp_listen_fd;
    open_fds[1].events = POLLIN;

    // POLL: Set the initial maximum descirptor array index
    max_fd_list_id = 1;

    // REDIS: Open connection
    redisContext *red_con = redisConnect(addr_redis->ip, addr_redis->port);
    if (red_con != NULL && red_con->err)
    {
        printf("%s: Error: %s\n", get_human_readable_time(), red_con->errstr);
        return 19;
    }
    printf("%s: Connected to Redis\n", get_human_readable_time());

    // GENERAL: Print confirmation ready to trade
    printf("Client %s is all set and ready to trade!\n", client_id);

    // GENERAL: Get midnight time
    uint64_t time_midnight = get_time_nanoseconds_midnight();

    // Start main loop
    while (1)
    {
        // Get the number of ready descriptors
        if ((fd_ready = poll(open_fds, max_fd_list_id + 1, -1)) < 0)
        {
            perror("Error: POLL: Cannot perform operation: ");
            return 6;
        }

        // TCP + POLL: Check if any new clients connected on TCP
        if (open_fds[0].revents & POLLIN)
        {
            // TCP: Initialize new client socket
            if ((tcp_client_fd = accept(tcp_listed_fd, (struct sockaddr *)&tcp_client_addr, &tcp_client_addr_len)) < 0)
            {
                perror("Error: TCP: Cannot accept customer's socket: ");
                return 7;
            }

            // TCP + POLL: If so far successful, add new customer socket to list to the first unused position
            for (fd_ind = 2; fd_ind < FOPEN_MAX; fd_ind++)
            {
                if (open_fds[fd_ind].fd < 0)
                {
                    open_fds[fd_ind].fd = tcp_client_fd;
                    open_fds[fd_ind].events = POLLIN;
                    break;
                }
            }

            // POLL: Validate if waiting list is full
            if (fd_ind == FOPEN_MAX)
            {
                perror("Error: POLL: Too many clients");
                return 8;
            }

            // POLL: Update the maximum ID of the used position in list for efficiency
            if (fd_ind > max_fd_list_id)
            {
                max_fd_list_id = fd_ind;
            }

            // POLL: Check if there are no more readable descriptors
            if (--fd_ready <= 0)
            {
                continue;
            }
        }

        // POLL: Check received data
        for (fd_ind = 1; fd_ind <= max_fd_list_id; fd_ind++)
        {
            // POLL: Skip element if there is no descriptors yet
            if (open_fds[fd_ind].fd < 0)
            {
                continue;
            }

            // POLL: Process ready socket (either read or error)
            if (open_fds[fd_ind].revents & (POLLIN | POLLERR))
            {
                // UDP: Process UDP multicast feed
                if (fd_ind == 1)
                {
                    if ((recv_bytes = recv(open_fds[fd_ind].fd, recv_buf, sizeof(recv_buf), 0)) < 0)
                    {
                        perror("Error: UDP: Cannot receive data");
                        return 9;
                    }
                    printf("MCAST FROM MARKET_DATA: %s\n", recv_buf);

                    // Cleanup the buffer
                    memset(recv_buf, 0, sizeof(recv_buf));
                }
                // TCP: Process TCP order gateway messages
                else
                {
                    // TCP: Case for connection reset by exchange
                    if ((recv_bytes = recv(open_fds[fd_ind].fd, recv_buf, sizeof(recv_buf), 0)) < 0)
                    {
                        if (errno == ECONNRESET)
                        {
                            if (close(open_fds[fd_ind].fd) < 0)
                            {
                                perror("Error: TCP: Cannot close socket: ");
                                return 10;
                            }
                            open_fds[fd_ind].fd = -1;
                        }
                    }
                    // TCP: Case for connection closed by exchange
                    else if (recv_bytes == 0)
                    {
                        if (close(open_fds[fd_ind].fd) < 0)
                        {
                            perror("Error: TCP: Cannot close socket: ");
                            return 10;
                        }
                        open_fds[fd_ind].fd = -1;
                    }
                    // TCP: Data received as usual
                    else
                    {
                        // Read data from exchange
                        struct order_gateway_request_message_t ogm_input;
                        memset(&ogm_input, 0, sizeof(ogm_input));

                        // Get exchange IP/port
                        struct sockaddr_in og_addr;
                        socklen_t og_addr_len = sizeof(og_addr);
                        memset(&og_addr, 0, sizeof(og_addr));
                        if (getsockname(open_fds[fd_ind].fd, (struct sockaddr *)&og_addr, &og_addr_len) < 0)
                        {
                            perror("Error: TCP: Cannot get socket addr order gateway");
                            return 13;
                        }
                        char og_ip_readable[INET_ADDRSTRLEN];
                        memset(og_ip_readable, 0, sizeof(og_ip_readable));
                        if (inet_ntop(AF_INET, &og_addr.sin_addr, og_ip_readable, INET_ADDRSTRLEN) == NULL)
                        {
                            perror("Error: TCP: Uncompatible IP Address: ");
                            return 2;
                        }
                        printf("OG | %s:%i | Received execution notification.\n",
                               og_ip_readable,
                               htons(og_addr.sin_port));

                        // Ensure that order gateway message fits the size
                        if (recv_bytes != sizeof(ogm_input))
                        {
                            perror("Error: TCP: Corrupted message from order gateway: ");
                            return 12;
                        }
                        memcpy(&ogm_input, recv_buf, sizeof(ogm_input));

                        // Get orders
                        order_t *order = deserialize_exhange_confirmation_2(&ogm_input);

                        // Prepare response message
                        struct order_gateway_response_message_t ogm_output;
                        memset(&ogm_output, 0, sizeof(ogm_output));
                        ogm_output.order_id = ogm_input.order_id;
                        ogm_output.ts_ack = bswap_64(get_time_nanoseconds_since_midnight(time_midnight));
                        ogm_output.status = 'A';

                        // Send confirmation to order gateway
                        if (send(open_fds[fd_ind].fd, &ogm_output, sizeof(ogm_output), 0) < 0)
                        {
                            perror("Error: TCP: Cannot send message: ");
                            return 11;
                        }
                        printf("OG | %s:%i | Sent acknowledgement.\n",
                               og_ip_readable,
                               htons(og_addr.sin_port));

                        // Shutdown the connection
                        shutdown(open_fds[fd_ind].fd, SHUT_WR);

                        // Update Redis
                        if (process_completed_order_redis(red_con, order) < 0)
                        {
                            perror("Error: TCP Cannot process Redis data: ");
                            free(order);
                        }

                        // Cleanup
                        memset(recv_buf, 0, sizeof(recv_buf));
                        free(order);
                    }
                }

                // POLL: No more readable descriptors
                if (--fd_ready <= 0)
                {
                    break;
                }
            }
        }
    }

    // Close socket
    close(tcp_listed_fd);
    close(udp_listen_fd);

    // Close connection to Redis
    redisFree(red_con);

    // Clean up
    free(addr_mcast_group);
    free(addr_mcast_local);
    free(addr_ucast_local);
    free(addr_redis);
}