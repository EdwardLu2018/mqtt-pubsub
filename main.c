/*
 * Tests for MQTT publish and subscribe client.
 * Written by Edward Lu
 */

#include <stdio.h>
#include <stdbool.h>
#include <assert.h>
#include <string.h>

#include "mqtt.h"

int main(void) {
    mqtt_data_t *mqtt_data;
    int recv_len;
    mqtt_broker *broker;

    broker = mqtt_connect("127.0.0.1", "this_is_a_test", 1883,
                          CLEAN_SESSION, 60U);
    assert(broker != NULL);

    assert(mqtt_pub(broker, "tests/test1", "msg1", true, false, QOS0) >= 0);
    assert(mqtt_pub(broker, "tests/test2", "msg2", true, false, QOS1) >= 0);
    assert(mqtt_pub(broker, "tests/test3", "msg3", true, false, QOS2) >= 0);

    assert(mqtt_sub(broker, "tests/test1", QOS0) >= 0);
    recv_len = mqtt_get_data(broker, mqtt_data);
    assert(recv_len >= 0);
    assert(mqtt_data->qos == QOS0);
    assert(mqtt_data->msg_id == -1);
    assert(strcmp(mqtt_data->topic, "tests/test1") == 0);
    assert(mqtt_data->payload_len == strlen("msg1"));
    assert(strncmp(mqtt_data->payload, "msg1", strlen("msg1")) == 0);

    assert(mqtt_ping(broker) >= 0);

    assert(mqtt_sub(broker, "tests/test2", QOS1) >= 0);
    recv_len = mqtt_get_data(broker, mqtt_data);
    assert(recv_len >= 0);
    assert(mqtt_data->qos == QOS1);
    assert(mqtt_data->msg_id == 1);
    assert(strcmp(mqtt_data->topic, "tests/test2") == 0);
    assert(mqtt_data->payload_len == strlen("msg2"));
    assert(strncmp(mqtt_data->payload, "msg2", strlen("msg2")) == 0);

    assert(mqtt_sub(broker, "tests/test3", QOS2) >= 0);
    recv_len = mqtt_get_data(broker, mqtt_data);
    assert(recv_len >= 0);
    assert(mqtt_data->qos == QOS2);
    assert(mqtt_data->msg_id == 2);
    assert(strcmp(mqtt_data->topic, "tests/test3") == 0);
    assert(mqtt_data->payload_len == strlen("msg3"));
    assert(strncmp(mqtt_data->payload, "msg3", strlen("msg3")) == 0);

    assert(mqtt_unsub(broker, "tests/test1") >= 0);
    assert(mqtt_unsub(broker, "tests/test2") >= 0);
    assert(mqtt_unsub(broker, "tests/test3") >= 0);

    assert(mqtt_disconnect(broker) >= 0);
    assert(free_broker(broker) >= 0);

    return 0;
}
