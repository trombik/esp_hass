# `hello_world` example

## What the example does

1. Configures `esp_hass_client`
1. Connects to WiFi network
1. Starts `esp_hass_client`, which automatically establishes WebSocket
   connection to `EXAMPLE_HASS_URI`, and performs authentication
1. Subscribes to all events
1. Calls a service, and print the result (modify entity ID, domain, and
   service to call by `make menuconfig`)
1. Prints events received from the Home Assistant
1. Cleans up and exits the test after the loop

## Requirements

The certificate of the `EXAMPLE_HASS_URI` is signed by a known CA in the
CA bundle. See
[ESP x509 Certificate Bundle](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/protocols/esp_crt_bundle.html)
for the CA bundle.

A _Long-Lived Access Tokens_. See
[Your Account Profile](https://www.home-assistant.io/docs/authentication/#your-account-profile)
section in the official documentation to create it.

## Configuration

Configure the example by `idf.py menuconfig`. The configurations for the
example is under `Example Configuration`.

## Known issues

The example works `esp-idf` version v4.x and newer but certification
verification with the CA bundle does not work. You need to modify the example
to include a CA certificate.

The example is being tested with `esp-idf` master branch. The example can be
compiled with other older versions, but is not tested.
