#include <stdio.h>
#include <stdbool.h>

#include "mqtt.h"

int main(void) {
    mqtt_broker *broker =
        mqtt_connect("127.0.0.1", "this_is_a_test", 1883, CLEAN_SESSION, 60U);
    if (broker == NULL) {
        fprintf(stderr, "connect failed\n");
    }

    if (mqtt_pub(broker, "tests/test1", "msg1", true, false, QOS0) < 0) {
        fprintf(stderr, "QOS0 publish failed\n");
    }

    if (mqtt_pub(broker, "tests/test2", "msg2", true, false, QOS1) < 0) {
        fprintf(stderr, "QOS1 publish failed\n");
    }

    if (mqtt_pub(broker, "tests/test2", "msg3", true, false, QOS2) < 0) {
        fprintf(stderr, "QOS2 publish failed\n");
    }

    if (mqtt_disconnect(broker) < 0) {
        fprintf(stderr, "disconnect failed\n");
    }

    return 0;
}
