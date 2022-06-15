# `hello_world` example

## What the example does

1. Configures `esp_hass_client`
1. Connects to WiFi network
1. Starts `esp_hass_client`, which automatically establishes WebSocket
   connection to `EXAMPLE_HASS_URI`, and performs authentication
1. Subscribes to all events
1. When the button is single-clicked, calls a service, and print the result
   (modify entity ID, domain, and service to call by `make menuconfig`)
1. Prints events received from the Home Assistant for the entity id
1. Cleans up and exits the test after `CONFIG_EXAMPLE_STOP_AFTER_MINUTE`
   minutes

## Requirements

A tactile button switch (normaly open) must be connected to a GPIO pin, 14 by
default. The other pin of the button must be connected to common GND.

See also [README.md](../hello_world/README.md) for the `hello_world` example.

## Configuration

Configure the example by `idf.py menuconfig`. The configurations for the
example is under `Example Configuration`.

## Known issues

The example works `esp-idf` version v4.x and newer but certification
verification with the CA bundle does not work. You need to modify the example
to include a CA certificate.

The example is being tested with `esp-idf` master branch. The example can be
compiled with other older versions, but is not tested.
