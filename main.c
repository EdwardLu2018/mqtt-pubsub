#include <stdio.h>
#include <stdbool.h>

#include "mqtt.h"

int main(void) {
    mqtt_broker *broker =
        mqtt_connect("127.0.0.1", "this_is_a_test", 1883, CLEAN_SESSION, 60U);
    if (broker == NULL) {
        fprintf(stderr, "connect failed\n");
    }

    if (mqtt_pub(broker, "tests/test1", "msg1", true, true, QOS1) < 0) {
        fprintf(stderr, "pub failed\n");
    }

    if (mqtt_disconnect(broker) < 0) {
        fprintf(stderr, "disconnect failed\n");
    }

    return 0;
}
