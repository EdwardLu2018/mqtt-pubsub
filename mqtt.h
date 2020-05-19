#ifndef MQTT_H
#define MQTT_H

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include <string.h>

#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>

#define BROKERIP_LEN    16      // ipv4 schema
#define CLIENTID_LEN    24      // between 1 and 23 + null terminator
#define MAXPACKET_LEN   1024

/* Connect flags */
#define CLEAN_SESSION   0b10
#define WILL_FLAG       0b100
#define WILL_RETAIN     0b10000
#define PASSWORD_FLAG   0b100000
#define USERNAME_FLAG   0b1000000

/* MQTT broker struct */
typedef struct {
    char connected;
    int socket_fd;
    int port;
    int pub_id;
    int sub_id;
    struct sockaddr_in addr;
    socklen_t addrlen;
    char broker_ip[BROKERIP_LEN];
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
typedef enum { QOS0, QOS1, QOS2 } mqtt_qos_t;

/* MQTT data struct */
typedef struct {
    mqtt_qos_t qos;
    int msg_id;
    int topic_len;
    int payload_len;
    char topic[MAXPACKET_LEN];
    char payload[MAXPACKET_LEN];
} mqtt_data_t;

mqtt_broker *mqtt_connect(const char *broker_ip, const char *client_id,
                               uint16_t port, uint8_t connect_flags,
                               uint16_t keep_alive);
int mqtt_pub(mqtt_broker *broker,
             const char *topic, const char *msg,
             bool retain, bool dup, mqtt_qos_t qos);
int mqtt_sub(mqtt_broker *broker, const char *topic, mqtt_qos_t qos);
int mqtt_get_data(mqtt_broker *broker, mqtt_data_t *data);
int mqtt_disconnect(mqtt_broker *broker);

#endif // MQTT_H
