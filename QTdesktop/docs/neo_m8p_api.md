# NEO-M8P GNSS API component

This component wraps the u-blox NEO-M8P navigation receiver. Serial I/O is handled by `signal_edge_ui_api.dll`, and NMEA parsing is done internally.

## Files

- `src/gnss/gnss_types.h`: public data structures and receiver configuration.
- `src/gnss/nmea_parser.h/.cpp`: NMEA checksum validation and GGA/RMC parsing.
- `src/gnss/neo_m8p_receiver.h/.cpp`: receiver API, DLL polling, NMEA dispatch.

## Public API

```cpp
gnss::NeoM8pReceiver receiver;

gnss::NeoM8pConfig config;
config.device = "SRP-Jeston";
config.ip = "Localhost";
config.channelId = 0;
config.updateRateHz = 5;

connect(&receiver, &gnss::NeoM8pReceiver::fixUpdated, this, [](const gnss::GnssFix &fix) {
    if (!fix.valid) {
        return;
    }

    qDebug() << fix.latitudeDeg
             << fix.longitudeDeg
             << fix.altitudeM
             << fix.satellites
             << fix.hdop;
});

receiver.open(config);
```

## Main capabilities

- Open and close the GNSS device via `ZD_ServerCreate` / `ZD_ServerDestroy`.
- Poll GNSS data via `ZD_GetGnssData` at configured rate.
- Parse `$GxGGA` and `$GxRMC` sentences into `GnssFix`.
- Emit `fixUpdated()` when a valid navigation sentence is received.
- Emit `rawSentenceReceived` with raw NMEA for diagnostics.
- Emit `errorOccurred` on API or connection failures.

## Integration note

If `fixUpdated()` is connected across threads, call `qRegisterMetaType<gnss::GnssFix>("gnss::GnssFix")` during startup.
