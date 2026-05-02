// web_server.h — AsyncWebServer serving a mobile-friendly config UI.
#pragma once

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include "net_manager.h"
#include "relay_controller.h"
#include "ui.h"

class WebUI {
public:
    WebUI(NetManager& net, RelayController& relays, UI& ui);
    void begin();

private:
    NetManager&      _net;
    RelayController& _relays;
    UI&              _ui;
    AsyncWebServer   _server;

    String buildIndexHtml();
    String buildStateJson();
};
