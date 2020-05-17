#ifndef MQTT_H
#define MQTT_H

#include <stdlib.h>
#include <stdint.h>

#include <string.h>

#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>

#define BROKERIP_LEN    16      // ipv4 schema
#define CLIENTID_LEN    24      // between 1 and 23 + null terminator
#define MAXPACKET_LEN   8192

typedef struct {
    char connected;
    int socket_fd;
    int port;
    struct sockaddr_in addr;
    socklen_t addrlen;
    char broker_ip[BROKERIP_LEN];
    char client_id[CLIENTID_LEN];
} mqtt_broker_info;

/* Connect flags */
#define RESERVED        0b0
#define CLEAN_SESSION   0b10
#define WILL_FLAG       0b100
#define WILL_QOS        0b1000
#define WILL_RETAIN     0b10000
#define PASSWORD_FLAG   0b100000
#define USERNAME_FLAG   0b1000000

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

int mqtt_connect(mqtt_broker_info *broker,
                 const char *broker_ip, const char *client_id,
                 uint16_t port, uint8_t connect_flags, uint16_t keep_alive);
int mqtt_pub(mqtt_broker_info *broker, const char *topic, const char *data);
int mqtt_sub(mqtt_broker_info *broker, const char *topic);

#endif // MQTT_H
