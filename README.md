# `esp_hass`

A Home assistant WebSocket API client for `esp-idf`.

## Features

The client:

* automatically connects to a Home Assistant and authenticates itself
* can subscribe to events
* can send commands
* allows a user-provided event handler to handle received events

## Supported `esp-idf` versions

* `master`
* `4.4.x`

Older versions, such as `4.2.x` should work, but are not tested in the CI.

## Examples

Examples are under [examples](examples) directory.

## Documentation

See [include/esp_hass.h](include/esp_hass.h).

## Usage

Clone this component into your project.

```console
mkdir components
cd components
git clone https://github.com/trombik/esp_hass
```

Import this component with `EXTRA_COMPONENT_DIRS` in your top-level
`CMakeLists.txt`. See details at
[Build System](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/build-system.html).

Include `esp_hass.h`.

Create a message handler function. This function is a callback function to
handle messages from Home Assistant.

Create a `esp_hass_client_handle_t`. The variable keeps an internal structure,
and is passed to all `esp_hass_*` functions.

Create a `esp_hass_config_t`, and initialize it with the Home Assistant URL,
including TLS configuration.

Create two FreeRTOS queues for events and commands. They are used to pass
`esp_hass_message_t`, messages from Home Assistant.

Initialize the client by `esp_hass_init()`.

Register the message handler with `esp_hass_event_handler_register()`.

Connect to the network. This must be implemented in your code.

Start the client by `esp_hass_client_start()`.  The client automatically
connects to the WebSocket URL, and authenticated itself. Use
`esp_hass_client_is_connected()` and `esp_hass_client_is_authenticated()` to
get the current connection status.

Here is an excerpt from an example. The code is not guaranteed to be correct
(because it is not tested in the CI), but illustrates the idea.

```c
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <esp_crt_bundle.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_hass.h>

#define MESSAGE_QUEUE_LEN (5)

static const char *TAG = "example";

static void
message_handler(void *args, esp_event_base_t base, int32_t id, void *event_data)
{
	esp_hass_client_handle_t client = NULL;
	esp_hass_message_t *msg = NULL;

	client = (esp_hass_client_handle_t)args;
	msg = (esp_hass_message_t *)event_data;

    /* do something with msg */
}

void
app_main(void)
{
	int event_queue_len = MESSAGE_QUEUE_LEN;
	int result_queue_len = MESSAGE_QUEUE_LEN;
	esp_err_t err = ESP_FAIL;
	esp_hass_client_handle_t client = NULL;
	esp_websocket_client_config_t ws_config = { 0 };
	QueueHandle_t event_queue = NULL;
	QueueHandle_t result_queue = NULL;

	ws_config.uri = "https://hass.example.org/api/websocket";
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)

	/* use default CA bundle */
	ws_config.crt_bundle_attach = esp_crt_bundle_attach;

	/* set timeouts here to surpress warnings from WEBSOCKET_CLIENT */
	ws_config.reconnect_timeout_ms = 10000;
	ws_config.network_timeout_ms = 10000;
#endif

	event_queue = xQueueCreate(event_queue_len,
	    sizeof(struct esp_hass_message_t *));
	if (event_queue == NULL) {
		ESP_LOGE(TAG, "xQueueCreate(): Out of memory");
		goto init_fail;
	}
	result_queue = xQueueCreate(result_queue_len,
	    sizeof(struct esp_hass_message_t *));
	if (result_queue == NULL) {
		ESP_LOGE(TAG, "xQueueCreate(): Out of memory");
		goto init_fail;
	}

	/* use ESP_HASS_CONFIG_DEFAULT() macro to initialize
	 * esp_hass_config_t for forward compatibiity.
	 */
	esp_hass_config_t config = ESP_HASS_CONFIG_DEFAULT();
	config.access_token = CONFIG_EXAMPLE_HASS_ACCESS_TOKEN;
	config.timeout_sec = 30;
	config.ws_config = &ws_config;
	config.event_queue = event_queue;
	config.result_queue = result_queue;

	ESP_LOGI(TAG, "Initializing hass client");
	client = esp_hass_init(&config);
	if (client == NULL) {
		ESP_LOGE(TAG, "esp_hass_init(): failed");
	}

	/* register message_handler */
	err = esp_hass_event_handler_register(client, message_handler);
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "esp_hass_event_handler_register(): %s",
		    esp_err_to_name(err));
		goto fail;
	}

	ESP_LOGI(TAG, "Starting hass client");
	err = esp_hass_client_start(client);
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "esp_hass_client_start(): %s",
		    esp_err_to_name(err));
		goto start_fail;
	}

	ESP_LOGI(TAG, "Waiting for WebSocket connection");
	do {
		vTaskDelay(1000 / portTICK_PERIOD_MS);
	} while (!esp_hass_client_is_connected(client));

	ESP_LOGI(TAG, "Waiting for client to be authenticated");
	do {
		vTaskDelay(1000 / portTICK_PERIOD_MS);
	} while (!esp_hass_client_is_authenticated(client));

    /* do something */
}
```

To call a service, use `esp_hass_call_service()` with
`esp_hass_call_service_config_t`.

## Branches

`main` is the latest development branch. All PRs should target this branch.

Use prefixes defined in [.github/pr-labeler.yml](.github/pr-labeler.yml) in
branch name, such as `bugfix-issue-2`, `feat-new-feature`, or `doc-README`.
These prefixes are used by
[pr-labeler-action](https://github.com/TimonVS/pr-labeler-action), which
automatically lables PRs.  If a PR does not follow the branch naming
convention, the PR will not be labeled, and will not be included in release
notes. In that case, the PR must be labeled manually.

## Labels for Pull Requests

Labels are applied to PRs automatically by matching branch name.

* `bugfix`, a PR that fixes bugs
* `documentation`, a PR that includes documentation
* `enhancement`, a PR that implements, or adds a feature

These labels are used by
[release-drafter](https://github.com/release-drafter/release-drafter#change-template-variables),
which automatically generates a draft release note.

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md).

## Authors

* [Tomoyuki Sakurai](https://github.com/trombik)
