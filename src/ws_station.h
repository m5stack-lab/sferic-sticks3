#pragma once

// WebSocket station <-> collector transport (ADR-0009, docs/mcp/ws-transport.md).
// Compiled only in the sferic-ws build (-DSFERIC_WS). One persistent
// station-initiated WebSocket; commands come in as {"type":"cmd",...} frames
// and go through the same dispatch::handle() the serial link uses.
//
// Wi-Fi note: the StickS3 has one 2.4 GHz radio. While a wifi.* stream tool
// runs it takes the radio (promiscuous / WIFI_OFF), which drops this uplink
// until the capture ends and we reconnect. Non-wifi tools (ble/ir/sensors/
// subghz/nrf24) are unaffected. See the transport doc.

#ifdef SFERIC_WS

class WsStation {
 public:
  void begin();  // start Wi-Fi STA + open the socket
  void loop();   // pump the WS client; call once per loop()
  bool linkUp() const;

 private:
  bool wifiWasUp_ = false;
};

extern WsStation wsStation;

#endif  // SFERIC_WS
