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
mqtt_broker *mqtt_connect(const char *broker_ip, const char *client_id,
                               uint16_t port, uint8_t connect_flags,
                               uint16_t keep_alive) {
    uint16_t client_id_len, remaining_len, var_header_len,
             payload_len, connect_msg_len, recv_len;

    mqtt_broker *broker = (mqtt_broker *)malloc(sizeof(mqtt_broker));

    if (broker == NULL) {
        return NULL;
    }
    else if (!broker_ip_valid(broker_ip)) {
        if (VERBOSE)
            fprintf(stderr, "Invalid broker_ip\n");
        free(broker);
        return NULL;
    }
    else if (!client_id_valid(client_id)) {
        if (VERBOSE)
            fprintf(stderr, "Invalid client_id\n");
        free(broker);
        return NULL;
    }

    /*
     * Save broker information
     */
    broker->connected = false;
    broker->port = port;
    broker->msg_id = 0;
    strcpy(broker->broker_ip, broker_ip);
    strcpy(broker->client_id, client_id);
    if ((broker->socket_fd = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
        if (VERBOSE)
            fprintf(stderr, "Unable to create socket\n");
        free(broker);
        return NULL;
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
        return NULL;
    }

    client_id_len = strlen(broker->client_id);

    remaining_len = 0;

    /*
     * Setup variable header
     */
    char var_header[] =
    {
        get_msb(MQTT_LEN),      // protocol length msb
        get_lsb(MQTT_LEN),      // protocol length lsb
        'M', 'Q', 'T', 'T',     // protocol name
        MQTT_V311,              // protocol level
        connect_flags,          // connect flags
        get_msb(keep_alive),    // time to keep alive MSB
        get_lsb(keep_alive)     // time to keep alive LSB
    };
    var_header_len = sizeof(var_header);
    remaining_len += var_header_len;

    /*
     * Setup payload
     */
    char payload[2 + client_id_len];
    payload[0] = 0;                                         // data length MSB
    payload[1] = client_id_len;                             // data length LSB
    memcpy(&payload[2], broker->client_id, client_id_len);  // data

    payload_len = sizeof(payload);
    remaining_len += payload_len;

    /*
     * Send MQTT connect message
     */

    // add fixed header
    connect_msg_len = 2 + remaining_len;
    char mqtt_connect_msg[connect_msg_len];
    // send CONNECT since we are connecting
    mqtt_connect_msg[0] = CONNECT << 4;     // MQTT control packet type << 4
    mqtt_connect_msg[1] = remaining_len;    // Remaining length of data

    // add variable header
    memcpy(&mqtt_connect_msg[2], var_header, var_header_len);

    // add payload
    memcpy(&mqtt_connect_msg[2] + var_header_len, payload, payload_len);

    /*
     * Send to broker
     */
    if (send(broker->socket_fd, mqtt_connect_msg, connect_msg_len, 0) < 0) {
        if (VERBOSE)
            fprintf(stderr, "Unable to send mqtt connect message to broker\n");
        free(broker);
        return NULL;
    }

    /*
     * Check for correct CONNACK (connection acknowledge) packet
     */
    char recv_buf[4];
    if ((recv_len = recv(broker->socket_fd, recv_buf, sizeof(recv_buf), 0)) < 0) {
        if (VERBOSE)
            fprintf(stderr, "Unable to receive from mqtt broker\n");
        free(broker);
        return NULL;
    }

    char recv_ctrl_packet = (recv_buf[0] >> 4) & 0xf;
    char recv_remaining_len = recv_buf[1];
    if (recv_ctrl_packet != CONNACK || recv_remaining_len != 2) {
        if (VERBOSE)
            fprintf(stderr, "Received packet is invalid\n");
        free(broker);
        return NULL;
    }

    broker->connected = true;
    return broker;
}

/*
 * Publishes a message to broker
 */
int mqtt_pub(mqtt_broker *broker,
             const char *topic, const char *msg,
             bool retain, bool dup, mqtt_qos_t qos) {
    uint16_t topic_len, msg_len, var_header_len, remaining_len;

    if (!broker->connected) {
        return -1;
    }

    topic_len = strlen(topic);
    msg_len = strlen(msg);

    /*
     * Setup variable header
     */
    // add 2 bytes for message id if QoS > 0
    var_header_len = 2 + topic_len + ((qos != QOS0) ? 2 : 0);
    remaining_len = var_header_len;

    char var_header[var_header_len];
    var_header[0] = get_msb(topic_len);
    var_header[1] = get_lsb(topic_len);
    memcpy(&var_header[2], topic, topic_len);

    if (qos != QOS0) {
        broker->msg_id += 1;
        var_header[var_header_len - 2] = get_msb(broker->msg_id);
        var_header[var_header_len - 1] = get_lsb(broker->msg_id);
    }

    // add message length
    remaining_len += msg_len;

    /*
     * Send MQTT publish message
     */

    // add fixed header
    char mqtt_pub_msg[2 + remaining_len];
    // MQTT control packet type | DUP | QoS | RETAIN
    mqtt_pub_msg[0] = (PUBLISH << 4) | (dup << 3) | (qos << 1) | (retain);
    mqtt_pub_msg[1] = remaining_len;
    memcpy(&mqtt_pub_msg[2], var_header, var_header_len);
    memcpy(&mqtt_pub_msg[2] + var_header_len, msg, msg_len);

    printf("%s\n", mqtt_pub_msg);

    return 1;
}

int mqtt_disconnect(mqtt_broker *broker) {
    if (!broker->connected) {
        return 0;
    }

    /*
     * DISCONNECT + no payload
     */
    char mqtt_disconnect_msg[2] =
    {
        DISCONNECT << 4,    // MQTT control packet type << 4
        0                   // no remaining length
    };

    /*
     * Send to broker
     */
    if (send(broker->socket_fd, mqtt_disconnect_msg, 2, 0) < 0) {
        if (VERBOSE)
            fprintf(stderr, "Unable to send mqtt connect message to broker\n");
        free(broker);
        return -1;
    }

    broker->connected = false;

    return 0;
}
