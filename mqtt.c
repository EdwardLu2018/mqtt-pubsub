#include "mqtt.h"

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#include <string.h>

#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>

#define VERBOSE 1

#define MQTT_LEN    4       // MQTT
#define MQTT_V311   0x04    // The value of the Protocol Level field for the
                            // version 3.1.1 of the protocol is 4 (0x04)

/* Typedef for convenience */
typedef struct sockaddr SA;

/* Small helper functions */
static char get_msb(int byte) {
    return (byte >> 8) & 0xff;
}

static char get_lsb(int byte) {
    return byte & 0xff;
}

/*
 * IPV4 Schema must be at most xxx.xxx.xxx.xxx + NULL (15 + 1)
 */
static int broker_ip_valid(const char *broker_ip) {
    return strlen(broker_ip) + 1 < BROKERIP_LEN;
}

/*
 * The Server MUST allow ClientIds which are between 1 and 23 UTF-8 encoded
 * bytes in length, and that contain only the characters
 * '0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ'
 */
static int client_id_valid(const char *client_id) {
    return strlen(client_id) + 1 < CLIENTID_LEN;
}

/*
 * Connects and sets up mqtt broker with specified params
 */
int mqtt_connect(mqtt_broker_info *broker,
                 const char *broker_ip, const char *client_id,
                 uint16_t port, uint8_t connect_flags, uint16_t keep_alive) {
    broker = (mqtt_broker_info *)malloc(sizeof(mqtt_broker_info *));

    if (broker == NULL) {
        return -1;
    }
    else if (!broker_ip_valid(broker_ip)) {
        if (VERBOSE)
            fprintf(stderr, "Invalid broker_ip\n");
        free(broker);
        return -1;
    }
    else if (!client_id_valid(client_id)) {
        if (VERBOSE)
            fprintf(stderr, "Invalid client_id\n");
        free(broker);
        return -1;
    }

    /*
     * Save broker information
     */
    broker->connected = 0;
    broker->port = port;
    strncpy(broker->broker_ip, broker_ip, BROKERIP_LEN);
    strncpy(broker->client_id, client_id, CLIENTID_LEN);
    if ((broker->socket_fd = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
        if (VERBOSE)
            fprintf(stderr, "Unable to create socket\n");
        free(broker);
        return -1;
    }

    /*
     * Setup and connect to socket
     */
    broker->addr.sin_family = AF_INET;
    broker->addr.sin_addr.s_addr = inet_addr(broker->broker_ip);
    broker->addr.sin_port = htons(broker->port);
    broker->addrlen = sizeof(broker->addr);

    if ((connect(broker->socket_fd, (SA *)&broker->addr, broker->addrlen)) < 0) {
        if (VERBOSE)
            fprintf(stderr, "Unable to connect to broker\n");
        free(broker);
        return -1;
    }

    uint16_t remaining_len = 0;

    /*
     * Setup variable header
     */
    char variable_header[] =
    {
        0,                      // protocol length msb
        MQTT_LEN,               // protocol length lsb
        'M', 'Q', 'T', 'T',     // protocol name
        MQTT_V311,              // protocol level
        connect_flags,          // connect flags
        get_msb(keep_alive),    // time to keep alive MSB
        get_lsb(keep_alive)     // time to keep alive LSB
    };
    uint16_t var_header_len = sizeof(variable_header);
    remaining_len += var_header_len;

    /*
     * Setup payload
     */
    uint8_t client_id_len = strlen(broker->client_id);
    char payload[2 + client_id_len];
    payload[0] = 0;                                         // data length MSB
    payload[1] = client_id_len;                             // data length LSB
    memcpy(&payload[2], broker->client_id, client_id_len);  // data

    uint16_t payload_len = sizeof(payload);
    remaining_len += payload_len;

    /*
     * Send MQTT connect message
     */

    // add fixed header
    uint16_t connect_msg_len = 2 + remaining_len;
    char mqtt_connect_msg[connect_msg_len];
    // send CONNECT since we are connecting
    mqtt_connect_msg[0] = CONNECT << 4;     // MQTT control packet type << 4
    mqtt_connect_msg[1] = remaining_len;    // Remaining length of data

    // add variable header
    memcpy(&mqtt_connect_msg[2], variable_header, var_header_len);

    // add payload
    memcpy(&mqtt_connect_msg[2] + var_header_len, payload, payload_len);

    /*
     * Send to broker
     */
    if (send(broker->socket_fd, mqtt_connect_msg, connect_msg_len, 0) < 0) {
        if (VERBOSE)
            fprintf(stderr, "Unable to send mqtt connect message to broker\n");
        free(broker);
        return -1;
    }

    /*
     * Check for correct CONNACK (connection acknowledge) packet
     */
    char recv_buf[4];
    uint64_t recv_len;
    if ((recv_len = recv(broker->socket_fd, recv_buf, sizeof(recv_buf), 0)) < 0) {
        if (VERBOSE)
            fprintf(stderr, "Unable to receive from mqtt broker\n");
        free(broker);
        return -1;
    }

    char recv_ctrl_packet = (recv_buf[0] >> 4) & 0xf;
    char recv_remaining_len = recv_buf[1];
    if (recv_ctrl_packet != CONNACK || recv_remaining_len != 2) {
        if (VERBOSE)
            fprintf(stderr, "Received packet is invalid\n");
        free(broker);
        return -1;
    }

    return 0;
}
