all:
	clang main.c mqtt.c -o mqtt_test

mqtt:
	/usr/local/sbin/mosquitto -c /usr/local/etc/mosquitto/mosquitto.conf
