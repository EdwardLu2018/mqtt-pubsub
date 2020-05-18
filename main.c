#include <stdio.h>

#include "mqtt.h"

int main(void) {
    mqtt_broker *broker =
        mqtt_connect("127.0.0.1", "this_is_a_test", 1883, CLEAN_SESSION, 60U);

    mqtt_disconnect(broker);

    return 0;
}
