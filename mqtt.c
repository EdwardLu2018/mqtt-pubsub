/*
 * MQTT publish and subscribe client.
 * Written by Edward Lu
 */

#include "mqtt.h"

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

#include <string.h>

#include <netdb.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>

/*
 * Based on MQTT Version 3.1.1
 * OASIS Standard
 *
 * http://docs.oasis-open.org/mqtt/mqtt/v3.1.1/os/mqtt-v3.1.1-os.html
 */

#define VERBOSE 1

#define MQTT_LEN    4       // len(MQTT)
#define MQTT_V311   0x4     // The value of the protocol level field for
                            // version 3.1.1 is 4 (0x04)

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
// static int hostname_valid(const char *hostname) {
//     return strlen(hostname) + 1 < BROKERIP_LEN;
// }

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
mqtt_broker *mqtt_connect(const char *hostname, const char *client_id,
                          uint16_t port, uint8_t connect_flags,
                          uint16_t keep_alive) {
    struct hostent *server;
    uint16_t client_id_len, remaining_len, var_header_len,
             payload_len, connect_msg_len, recv_len;

    mqtt_broker *broker = (mqtt_broker *)malloc(sizeof(mqtt_broker));

    if (broker == NULL) {
        return NULL;
    }
    // else if (!hostname_valid(hostname)) {
    //     if (VERBOSE)
    //         fprintf(stderr, "Invalid hostname\n");
    //     free(broker);
    //     return NULL;
    // }
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
    broker->pub_id = 0;
    broker->sub_id = 0;
    strcpy(broker->hostname, hostname);
    strcpy(broker->client_id, client_id);
    if ((broker->socket_fd = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
        if (VERBOSE)
            fprintf(stderr, "Unable to create socket\n");
        free(broker);
        return NULL;
    }

    /*
     * Get server by DNS
     */
    if ((server = gethostbyname(broker->hostname)) == NULL) {
        if (VERBOSE)
            fprintf(stderr, "Unable to connect to MQTT server\n");
        free(broker);
        return NULL;
    }

    /*
     * Setup and connect to socket
     */
    memset((char *) &broker->addr, '\0', sizeof(broker->addr));
    broker->addr.sin_family = AF_INET;
    broker->addr.sin_addr.s_addr = *(uint32_t *)(server->h_addr);
    broker->addr.sin_port = htons(broker->port);
    broker->addrlen = sizeof(broker->addr);

    if ((connect(broker->socket_fd, (SA *)&broker->addr,
        broker->addrlen)) < 0) {
        if (VERBOSE)
            fprintf(stderr, "Unable to connect to broker\n");
        free(broker);
        return NULL;
    }

    /*
     * Set socket recv timeout
     */
    struct timeval tv;
    tv.tv_sec = 30; // 30 sec timeout
    tv.tv_usec = 0;
    setsockopt(broker->socket_fd, SOL_SOCKET, SO_RCVTIMEO,
            (char *)&tv, sizeof(struct timeval));

    client_id_len = strlen(broker->client_id);

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
    remaining_len = var_header_len;

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
    mqtt_connect_msg[0] = (uint8_t)(CONNECT << 4);  // MQTT control packet type
    mqtt_connect_msg[1] = remaining_len;            // Remaining length of data

    // add variable header
    memcpy(&mqtt_connect_msg[2], var_header, var_header_len);

    // add payload
    memcpy(&mqtt_connect_msg[2] + var_header_len, payload, payload_len);

    /*
     * Send to broker
     */
    if (send(broker->socket_fd, mqtt_connect_msg, connect_msg_len, 0) < 0) {
        if (VERBOSE)
            fprintf(stderr, "Unable to send CONNECT message to broker\n");
        free(broker);
        return NULL;
    }

    /*
     * Check for correct CONNACK (connection acknowledge) packet
     */
    char recv_buf[4], recv_ctrl_packet, recv_remaining_len;
    if ((recv_len = recv(broker->socket_fd, recv_buf,
        sizeof(recv_buf), 0)) < 0) {
        if (VERBOSE)
            fprintf(stderr, "Unable to receive from mqtt broker\n");
        free(broker);
        return NULL;
    }

    recv_ctrl_packet = (uint8_t)(recv_buf[0] >> 4) & 0xf;
    recv_remaining_len = recv_buf[1];
    if (recv_ctrl_packet != CONNACK || recv_remaining_len != 2) {
        if (VERBOSE)
            fprintf(stderr, "Received packet is invalid CONNACK\n");
        free(broker);
        return NULL;
    }
    // check CONNACK flags
    else if ((recv_buf[2] & 1) != 0) { // Bit 0 session present flag
        if (VERBOSE)
            fprintf(stderr, "Acknowledge flag is invalid CONNACK\n");
        free(broker);
        return NULL;
    }
    // check CONNACK return codes
    // 0x00 is connection accepted
    else if (recv_buf[3] != 0) {
        if (VERBOSE)
            fprintf(stderr, "Return code is invalid CONNACK\n");
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
    uint16_t topic_len, msg_len, var_header_len, remaining_len,
             pub_msg_len, recv_len;

    if (broker == NULL || !broker->connected) {
        if (VERBOSE)
            fprintf(stderr, "Broker not set up\n");
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
        broker->pub_id += 1;
        var_header[var_header_len - 2] = get_msb(broker->pub_id);
        var_header[var_header_len - 1] = get_lsb(broker->pub_id);
    }

    // add message length
    remaining_len += msg_len;

    /*
     * Send MQTT publish message
     */
    // add fixed header
    pub_msg_len = 2 + remaining_len;
    char mqtt_pub_msg[pub_msg_len];
    // MQTT control packet type | DUP | QoS | RETAIN
    mqtt_pub_msg[0] = (uint8_t)(PUBLISH << 4) | (dup << 3) |
                      (qos << 1) | (retain);
    mqtt_pub_msg[1] = remaining_len;
    // add variable header
    memcpy(&mqtt_pub_msg[2], var_header, var_header_len);
    // add payload (which is just the message to be sent)
    memcpy(&mqtt_pub_msg[2] + var_header_len, msg, msg_len);

    /*
     * Send to broker
     */
    if (send(broker->socket_fd, mqtt_pub_msg, pub_msg_len, 0) < 0) {
        if (VERBOSE)
            fprintf(stderr, "Unable to send PUBLISH message to broker\n");
        return -1;
    }

    /*
     * Check for correct recved packet
     */
    char buf[4], recv_ctrl_packet, recv_remaining_len;

    // For QoS level 1, must receive a PUBACK (publish acknowledge)
    if (qos == QOS1) {
        if ((recv_len = recv(broker->socket_fd, buf, sizeof(buf), 0)) < 0) {
            if (VERBOSE)
                fprintf(stderr, "Unable to receive from mqtt broker\n");
            return -1;
        }

        recv_ctrl_packet = (uint8_t)(buf[0] >> 4) & 0xf;
        recv_remaining_len = buf[1];
        if (recv_ctrl_packet != PUBACK || recv_remaining_len != 2) {
            if (VERBOSE)
                fprintf(stderr, "Received packet is invalid PUBACK\n");
            return -1;
        }
        else if (((uint8_t)(buf[2] << 4) | buf[3]) != broker->pub_id) {
            if (VERBOSE)
                fprintf(stderr, "Packet identifer doesn't match PUBACK\n");
            return -1;
        }
    }
    // For QoS level 2, must receive a PUBREC (publish receive),
    // send a PUBREL (publish release), and receive a PUBCOMP (publish complete)
    else if (qos == QOS2) {
        // receive PUBREC
        if ((recv_len = recv(broker->socket_fd, buf, sizeof(buf), 0)) < 0) {
            if (VERBOSE)
                fprintf(stderr, "Unable to receive from mqtt broker\n");
            return -1;
        }

        recv_ctrl_packet = (uint8_t)(buf[0] >> 4) & 0xf;
        recv_remaining_len = buf[1];
        if (recv_ctrl_packet != PUBREC || recv_remaining_len != 2) {
            if (VERBOSE)
                fprintf(stderr, "Received packet is invalid PUBREC\n");
            return -1;
        }
        else if (((uint8_t)(buf[2] << 4) | buf[3]) != broker->pub_id) {
            if (VERBOSE)
                fprintf(stderr, "Packet identifer doesn't match PUBREC\n");
            return -1;
        }

        // send PUBREL
        buf[0] = (uint8_t)(PUBREL << 4) | (2); // PUBREL + reserved
        buf[1] = 2; // MSB of length + LSB of lengh (length = 2)
        buf[2] = get_msb(broker->pub_id);
        buf[3] = get_lsb(broker->pub_id);

        if (send(broker->socket_fd, buf, sizeof(buf), 0) < 0) {
            if (VERBOSE)
                fprintf(stderr, "Unable to send PUBREL message to broker\n");
            return -1;
        }

        // receive PUBCOMP
        if ((recv_len = recv(broker->socket_fd, buf, sizeof(buf), 0)) < 0) {
            if (VERBOSE)
                fprintf(stderr, "Unable to receive from mqtt broker\n");
            return -1;
        }

        recv_ctrl_packet = (uint8_t)(buf[0] >> 4) & 0xf;
        recv_remaining_len = buf[1];
        if (recv_ctrl_packet != PUBCOMP || recv_remaining_len != 2) {
            if (VERBOSE)
                fprintf(stderr, "Received packet is invalid PUBCOMP\n");
            return -1;
        }
        else if (((uint8_t)(buf[2] << 4) | buf[3]) != broker->pub_id) {
            if (VERBOSE)
                fprintf(stderr, "Packet identifer doesn't match PUBCOMP\n");
            return -1;
        }
    }

    return 0;
}

/*
 * Subscribes to a topic on a broker
 */
int mqtt_sub(mqtt_broker *broker, const char *topic, mqtt_qos_t qos) {
    uint16_t topic_len, var_header_len, payload_len, remaining_len,
             sub_msg_len, recv_len;

    if (broker == NULL || !broker->connected) {
        if (VERBOSE)
            fprintf(stderr, "Broker not set up\n");
        return -1;
    }

    broker->sub_id++;

    topic_len = strlen(topic);

    /*
     * Setup variable header
     */
    // add 2 bytes for message id if QoS > 0
    char var_header[] =
    {
        get_msb(broker->sub_id),    // packet identifer MSB
        get_lsb(broker->sub_id)     // packet identifer LSB
    };
    var_header_len = 2;
    remaining_len = var_header_len;

    /*
     * Setup payload
     */
    // length msb + lsb + topic length + QoS
    payload_len = 2 + strlen(topic) + 1;
    remaining_len += payload_len;

    char payload[payload_len];
    payload[0] = get_msb(topic_len);
    payload[1] = get_lsb(topic_len);
    memcpy(&payload[2], topic, topic_len);
    payload[payload_len - 1] = qos;

    /*
     * Send MQTT subcribe message
     */
    // add fixed header
    sub_msg_len = 2 + remaining_len;
    char mqtt_sub_msg[sub_msg_len];
    mqtt_sub_msg[0] = (uint8_t)(SUBSCRIBE << 4 | 2);
    mqtt_sub_msg[1] = remaining_len;
    // add variable header
    memcpy(&mqtt_sub_msg[2], var_header, var_header_len);
    // add payload
    memcpy(&mqtt_sub_msg[2] + var_header_len, payload, payload_len);

    /*
     * Send to broker
     */
    if (send(broker->socket_fd, mqtt_sub_msg, sub_msg_len, 0) < 0) {
        if (VERBOSE)
            fprintf(stderr, "Unable to send SUBSCRIBE message to broker\n");
        return -1;
    }

    /*
     * Check for correct SUBACK (subscribe acknowledge) packet
     */
    char recv_buf[5], recv_ctrl_packet, recv_remaining_len;

    if ((recv_len = recv(broker->socket_fd, recv_buf,
        sizeof(recv_buf), 0)) < 0) {
        if (VERBOSE)
            fprintf(stderr, "Unable to receive from mqtt broker\n");
        return -1;
    }

    recv_ctrl_packet = (uint8_t)(recv_buf[0] >> 4) & 0xf;
    recv_remaining_len = recv_buf[1]; // should be 3 (recv_len=5 - header=2)
    if (recv_ctrl_packet != SUBACK || recv_remaining_len != 3) {
        if (VERBOSE)
            fprintf(stderr, "Received packet is invalid SUBACK\n");
        return -1;
    }
    else if (((uint8_t)(recv_buf[2] << 4) | recv_buf[3]) != broker->sub_id) {
        if (VERBOSE)
            fprintf(stderr, "Packet identifer doesn't match SUBACK\n");
        return -1;
    }
    else if (qos != recv_buf[4]) {
        if (VERBOSE)
            fprintf(stderr, "Return code is invalid SUBACK\n");
        return -1;
    }

    return 0;
}

/*
 * Unsubscribes to a topic on a broker
 */
int mqtt_unsub(mqtt_broker *broker, const char *topic) {
    uint16_t topic_len, var_header_len, payload_len, remaining_len,
             sub_msg_len, recv_len;

    if (broker == NULL || !broker->connected) {
        if (VERBOSE)
            fprintf(stderr, "Broker not set up\n");
        return -1;
    }

    topic_len = strlen(topic);

    /*
     * Setup variable header
     */
    // add 2 bytes for message id if QoS > 0
    char var_header[] =
    {
        get_msb(broker->sub_id),    // packet identifer MSB
        get_lsb(broker->sub_id)     // packet identifer LSB
    };
    var_header_len = 2;
    remaining_len = var_header_len;

    /*
     * Setup payload
     */
    // length msb + lsb + topic length + QoS
    payload_len = 2 + strlen(topic);
    remaining_len += payload_len;

    char payload[payload_len];
    payload[0] = get_msb(topic_len);
    payload[1] = get_lsb(topic_len);
    memcpy(&payload[2], topic, topic_len);

    /*
     * Send MQTT subcribe message
     */
    // add fixed header
    sub_msg_len = 2 + remaining_len;
    char mqtt_sub_msg[sub_msg_len];
    mqtt_sub_msg[0] = (uint8_t)(UNSUBSCRIBE << 4 | 2);
    mqtt_sub_msg[1] = remaining_len;
    // add variable header
    memcpy(&mqtt_sub_msg[2], var_header, var_header_len);
    // add payload
    memcpy(&mqtt_sub_msg[2] + var_header_len, payload, payload_len);

    /*
     * Send to broker
     */
    if (send(broker->socket_fd, mqtt_sub_msg, sub_msg_len, 0) < 0) {
        if (VERBOSE)
            fprintf(stderr, "Unable to send UNSUBSCRIBE message to broker\n");
        return -1;
    }

    /*
     * Check for correct UNSUBACK (unsubscribe acknowledge) packet
     */
    char recv_buf[4], recv_ctrl_packet, recv_remaining_len;

    if ((recv_len = recv(broker->socket_fd, recv_buf,
        sizeof(recv_buf), 0)) < 0) {
        if (VERBOSE)
            fprintf(stderr, "Unable to receive from mqtt broker\n");
        return -1;
    }

    recv_ctrl_packet = (uint8_t)(recv_buf[0] >> 4) & 0xf;
    recv_remaining_len = recv_buf[1];
    if (recv_ctrl_packet != UNSUBACK || recv_remaining_len != 2) {
        if (VERBOSE)
            fprintf(stderr, "Received packet is invalid UNSUBACK\n");
        return -1;
    }
    else if (((uint8_t)(recv_buf[2] << 4) | recv_buf[3]) > broker->sub_id) {
        if (VERBOSE)
            fprintf(stderr, "Packet identifer doesn't match UNSUBACK\n");
        return -1;
    }

    return 0;
}

/*
 * Ping the server, used in Keep Alive processing
 */
int mqtt_ping(mqtt_broker *broker) {
    if (broker == NULL || !broker->connected) {
        if (VERBOSE)
            fprintf(stderr, "Broker not set up\n");
        return -1;
    }

    /*
     * Setup packet, which is just the fixed header
     */
    char mqtt_ping_msg[] =
    {
        (uint8_t)(PINGREQ << 4),
        0
    };

    /*
     * Send to broker
     */
    if (send(broker->socket_fd, mqtt_ping_msg, 2, 0) < 0) {
        if (VERBOSE)
            fprintf(stderr, "Unable to send PINGREQ message to broker\n");
        free(broker);
        return -1;
    }

    /*
     * Check for correct PINGRESP (ping response) packet
     */
    char recv_buf[2], recv_len, recv_ctrl_packet, recv_remaining_len;

    if ((recv_len = recv(broker->socket_fd, recv_buf, 2, 0)) < 0) {
        if (VERBOSE)
            fprintf(stderr, "Unable to receive from mqtt broker\n");
        return -1;
    }

    recv_ctrl_packet = (uint8_t)(recv_buf[0] >> 4) & 0xf;
    recv_remaining_len = recv_buf[1];
    if (recv_ctrl_packet != PINGRESP || recv_remaining_len != 0) {
        if (VERBOSE)
            fprintf(stderr, "Received packet is invalid PINGRESP\n");
        return -1;
    }

    return 0;
}

/*
 * Get data of last subscribed topic
 */
int mqtt_get_data(mqtt_broker *broker, mqtt_data_t *data) {
    uint16_t recv_len, remaining_len, recv_remaining_len, var_header_len;
    char recv_buf[MAXPACKET_LEN], recv_ctrl_packet;

    if ((recv_len = recv(broker->socket_fd, recv_buf, MAXPACKET_LEN, 0)) < 0) {
        if (VERBOSE)
            fprintf(stderr, "Receive data failure\n");
        return -1;
    }

    recv_ctrl_packet = (uint8_t)(recv_buf[0] >> 4) & 0xf;
    remaining_len = recv_buf[1];
    if (recv_ctrl_packet != PUBLISH) {
        if (VERBOSE)
            fprintf(stderr, "Received packet is invalid\n");
        return -1;
    }

    /*
     * Parse buffer
     */
    // fixed header = Control packet|dup|Qos|retain + remaining length
    data->qos = (recv_buf[0] >> 1) & 0b11;

    // variable header = topic length msb + lsb + topic + packet id msb + lsb
    data->topic_len = (int)(recv_buf[2] << 4) | (recv_buf[3]);
    memcpy(data->topic, &recv_buf[4], data->topic_len);
    data->topic[data->topic_len] = '\0';
    var_header_len = 2 + data->topic_len;

    if (data->qos != QOS0) { // QoS1 and Q0S2 have message id
        data->msg_id = (int)(recv_buf[data->topic_len + 4] << 4) |
                             recv_buf[data->topic_len + 5];
        var_header_len += 2;
    }
    else {
        data->msg_id = -1;
    }

    // payload is the rest
    // - length can be calculated by subtracting the length of the variable
    // header from the remaining length field that is in the fixed header
    data->payload_len = remaining_len - var_header_len;
    memcpy(data->payload, &recv_buf[var_header_len + 2], data->payload_len);

    char buf[4];

    // For QoS level 1, must send a PUBACK (publish acknowledge)
    if (data->qos == QOS1) {
        buf[0] = (uint8_t)(PUBACK << 4);
        buf[1] = 2;
        buf[2] = get_msb(data->msg_id);
        buf[3] = get_lsb(data->msg_id);

        if (send(broker->socket_fd, buf, sizeof(buf), 0) < 0) {
            if (VERBOSE)
                fprintf(stderr, "Unable to send PUBACK message to broker\n");
            return -1;
        }
    }
    // For QoS level 2, must send a PUBREC (publish receive),
    // receive a PUBREL (publish release), and send a PUBCOMP (publish complete)
    else if (data->qos == QOS2) {
        // send PUBREC
        buf[0] = (uint8_t)(PUBREC << 4);
        buf[1] = 2;
        buf[2] = get_msb(data->msg_id);
        buf[3] = get_lsb(data->msg_id);

        if (send(broker->socket_fd, buf, sizeof(buf), 0) < 0) {
            if (VERBOSE)
                fprintf(stderr, "Unable to send PUBREC message to broker\n");
            return -1;
        }

        // receive PUBREL
        if ((recv_len = recv(broker->socket_fd, buf, sizeof(buf), 0)) < 0) {
            if (VERBOSE)
                fprintf(stderr, "Unable to receive from mqtt broker\n");
            return -1;
        }

        recv_ctrl_packet = (uint8_t)(buf[0] >> 4) & 0xf;
        recv_remaining_len = buf[1];
        if (recv_ctrl_packet != PUBREL || recv_remaining_len != 2) {
            if (VERBOSE)
                fprintf(stderr, "Received packet is invalid PUBREL\n");
            return -1;
        }

        // send PUBCOMP
        buf[0] = (uint8_t)(PUBCOMP << 4);
        buf[1] = 2;
        buf[2] = get_msb(data->msg_id);
        buf[3] = get_lsb(data->msg_id);

        if (send(broker->socket_fd, buf, sizeof(buf), 0) < 0) {
            if (VERBOSE)
                fprintf(stderr, "Unable to send PUBCOMP message to broker\n");
            return -1;
        }

    }

    return recv_len;
}

/*
 * Disconnects broker
 */
int mqtt_disconnect(mqtt_broker *broker) {
    if (broker == NULL || !broker->connected) {
        return 0;
    }

    /*
     * DISCONNECT + no payload
     */
    char mqtt_disconnect_msg[2] =
    {
        (uint8_t)(DISCONNECT << 4), // MQTT control packet type << 4
        0                           // no remaining length
    };

    /*
     * Send to broker
     */
    if (send(broker->socket_fd, mqtt_disconnect_msg, 2, 0) < 0) {
        if (VERBOSE)
            fprintf(stderr, "Unable to send DISCONNECT message to broker\n");
        free(broker);
        return -1;
    }

    broker->connected = false;

    return 0;
}

/*
 * Frees memory taken by broker structure
 */
int free_broker(mqtt_broker *broker) {
    if (broker != NULL) {
        close(broker->socket_fd);
        free(broker);
        return 0;
    }
    return -1;
}
