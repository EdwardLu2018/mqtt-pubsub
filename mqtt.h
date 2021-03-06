#ifndef MQTT_H
#define MQTT_H

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include <string.h>

#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>

#define HOSTNAME_LEN    255
#define CLIENTID_LEN    24      // between 1 and 23 + null terminator
#define MAXPACKET_LEN   255

/* Connect flags */
#define CLEAN_SESSION   0b10
#define WILL_FLAG       0b100
#define WILL_RETAIN     0b10000
#define PASSWORD_FLAG   0b100000
#define USERNAME_FLAG   0b1000000

/* MQTT broker struct */
typedef struct {
    bool connected;
    int socket_fd;
    uint16_t port;
    uint16_t pub_id;
    uint16_t sub_id;
    struct sockaddr_in addr;
    socklen_t addrlen;
    char hostname[HOSTNAME_LEN];
    char client_id[CLIENTID_LEN];
} mqtt_broker;

/* Control packet */
typedef enum {
    UNDEF,
    CONNECT,
    CONNACK,
    PUBLISH,
    PUBACK,
    PUBREC,
    PUBREL,
    PUBCOMP,
    SUBSCRIBE,
    SUBACK,
    UNSUBSCRIBE,
    UNSUBACK,
    PINGREQ,
    PINGRESP,
    DISCONNECT
} control_packet_t;

/* Quality of service */
typedef enum { QOS0, QOS1, QOS2, FAILURE=0x80} mqtt_qos_t;

/* MQTT data struct */
typedef struct {
    mqtt_qos_t qos;
    int msg_id;
    int topic_len;
    int payload_len;
    char topic[MAXPACKET_LEN];
    char payload[MAXPACKET_LEN];
} mqtt_data_t;

mqtt_broker *mqtt_init(const char *broker_ip, const char *client_id,
                        uint16_t port);
int mqtt_connect(mqtt_broker *broker, uint8_t connect_flags,
                    uint8_t keep_alive);
int mqtt_pub(mqtt_broker *broker,
             const char *topic, const char *msg,
             bool retain, bool dup, mqtt_qos_t qos);
int mqtt_sub(mqtt_broker *broker, const char *topic, mqtt_qos_t qos);
int mqtt_unsub(mqtt_broker *broker, const char *topic);
int mqtt_ping(mqtt_broker *broker);
int mqtt_get_data(mqtt_broker *broker, mqtt_data_t *data);
int mqtt_disconnect(mqtt_broker *broker);
int free_broker(mqtt_broker *broker);

#endif // MQTT_H
