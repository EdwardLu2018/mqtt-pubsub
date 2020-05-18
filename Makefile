all:
	clang main.c mqtt.c -o mqtt_test

broker:
	/usr/local/sbin/mosquitto -c /usr/local/etc/mosquitto/mosquitto.conf
