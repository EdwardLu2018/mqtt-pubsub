#include <stdio.h>

#include "mqtt.h"

int main(void) {
    mqtt_broker_info *broker;

    mqtt_connect(broker, "127.0.0.1", "this_is_a_test", 1883, CLEAN_SESSION, 60U);

    return 0;
}
