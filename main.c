/*
 * Tests for MQTT publish and subscribe client.
 * Written by Edward Lu
 */

#include <stdio.h>
#include <stdbool.h>

#include "mqtt.h"

int main(void) {
    mqtt_data_t *mqtt_data;
    int recv_len;
    mqtt_broker *broker;

    broker = mqtt_connect("127.0.0.1", "this_is_a_test", 1883,
                          CLEAN_SESSION, 60U);
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

    fprintf(stderr, "Running sub test for topic tests/test1 [QOS0]:\n");
    if (mqtt_sub(broker, "tests/test1", QOS0) < 0) {
        fprintf(stderr, "QOS0 subcribe failed\n");
        return -1;
    }

    recv_len = mqtt_get_data(broker, mqtt_data);
    fprintf(stderr, "QoS%d (should be QoS0)\n", mqtt_data->qos);
    fprintf(stderr, "msg_id = %d (should be -1, since QoS == 0)\n",
                    mqtt_data->msg_id);
    fprintf(stderr, "topic = %s (should be tests/test1)\n", mqtt_data->topic);
    fprintf(stderr, "payload = ");
    for(size_t i = 0; i < mqtt_data->payload_len; ++i)
        fprintf(stderr, "%c", mqtt_data->payload[i]);
    fprintf(stderr, " (should be msg1)\n\n");

    fprintf(stderr, "Running sub test for topic tests/test2 [QOS1]:\n");
    if (mqtt_sub(broker, "tests/test2", QOS1) < 0) {
        fprintf(stderr, "QOS0 subcribe failed\n");
        return -1;
    }

    recv_len = mqtt_get_data(broker, mqtt_data);
    fprintf(stderr, "QoS%d (should be QoS1)\n", mqtt_data->qos);
    fprintf(stderr, "msg_id = %d (should be 1)\n", mqtt_data->msg_id);
    fprintf(stderr, "topic = %s (should be tests/test2)\n", mqtt_data->topic);
    fprintf(stderr, "payload = ");
    for(size_t i = 0; i < mqtt_data->payload_len; ++i)
        fprintf(stderr, "%c", mqtt_data->payload[i]);
    fprintf(stderr, " (should be msg2)\n\n");


    fprintf(stderr, "Running sub test for topic tests/test3 [QOS2]:\n");
    if (mqtt_sub(broker, "tests/test3", QOS2) < 0) {
        fprintf(stderr, "QOS0 subcribe failed\n");
        return -1;
    }

    recv_len = mqtt_get_data(broker, mqtt_data);
    fprintf(stderr, "QoS%d (should be QoS2)\n", mqtt_data->qos);
    fprintf(stderr, "msg_id = %d (should be 2)\n", mqtt_data->msg_id);
    fprintf(stderr, "topic = %s (should be tests/test3)\n", mqtt_data->topic);
    fprintf(stderr, "payload = ");
    for(size_t i = 0; i < mqtt_data->payload_len; ++i)
        fprintf(stderr, "%c", mqtt_data->payload[i]);
    fprintf(stderr, " (should be msg3)\n");

    if (mqtt_disconnect(broker) < 0) {
        fprintf(stderr, "disconnect failed\n");
        return -1;
    }

    free_broker(broker);

    return 0;
}
