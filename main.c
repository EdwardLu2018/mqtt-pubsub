#include <stdio.h>
#include <stdbool.h>

#include "mqtt.h"

int main(void) {
    mqtt_data_t *data;
    int recv_len;
    mqtt_broker *broker;

    broker = mqtt_connect("127.0.0.1", "this_is_a_test", 1883, CLEAN_SESSION, 60U);
    if (broker == NULL) {
        fprintf(stderr, "connect failed\n");
        return -1;
    }

    if (mqtt_pub(broker, "tests/test1", "msg1", true, false, QOS0) < 0) {
        fprintf(stderr, "QOS0 publish failed\n");
        return -1;
    }

    if (mqtt_pub(broker, "tests/test2", "msg2", true, false, QOS1) < 0) {
        fprintf(stderr, "QOS1 publish failed\n");
        return -1;
    }

    if (mqtt_pub(broker, "tests/test3", "msg3", true, false, QOS2) < 0) {
        fprintf(stderr, "QOS2 publish failed\n");
        return -1;
    }

    if (mqtt_sub(broker, "tests/test1", QOS1) < 0) {
        fprintf(stderr, "QOS0 subcribe failed\n");
        return -1;
    }

    recv_len = mqtt_get_data(broker, data);
    fprintf(stderr, "QoS%d\n", data->qos);
    fprintf(stderr, "msg_id = %d\n", data->msg_id);
    fprintf(stderr, "topic = %s\n", data->topic);
    fprintf(stderr, "payload = ");
    for(size_t i = 0; i < data->payload_len; ++i)
        fprintf(stderr, "%c", data->payload[i]);
    fprintf(stderr, "\n\n");

    if (mqtt_sub(broker, "tests/test2", QOS1) < 0) {
        fprintf(stderr, "QOS0 subcribe failed\n");
        return -1;
    }

    recv_len = mqtt_get_data(broker, data);
    fprintf(stderr, "QoS%d\n", data->qos);
    fprintf(stderr, "msg_id = %d\n", data->msg_id);
    fprintf(stderr, "topic = %s\n", data->topic);
    fprintf(stderr, "payload = ");
    for(size_t i = 0; i < data->payload_len; ++i)
        fprintf(stderr, "%c", data->payload[i]);
    fprintf(stderr, "\n\n");

    if (mqtt_sub(broker, "tests/test3", QOS1) < 0) {
        fprintf(stderr, "QOS0 subcribe failed\n");
        return -1;
    }

    recv_len = mqtt_get_data(broker, data);
    fprintf(stderr, "QoS%d\n", data->qos);
    fprintf(stderr, "msg_id = %d\n", data->msg_id);
    fprintf(stderr, "topic = %s\n", data->topic);
    fprintf(stderr, "payload = ");
    for(size_t i = 0; i < data->payload_len; ++i)
        fprintf(stderr, "%c", data->payload[i]);
    fprintf(stderr, "\n\n");

    if (mqtt_disconnect(broker) < 0) {
        fprintf(stderr, "disconnect failed\n");
        return -1;
    }

    return 0;
}
