# Beacon Preserve Policy

`firmware/beacon/Beacon_A1.ino` is the focused Beacon entrypoint.

The preserve rule means the public firmware should not be reduced until behavior has been checked on hardware. Before simplifying, identify which code is responsible for:

- beacon state;
- ESP-NOW packet exchange;
- UI or indicator behavior;
- timing loops;
- memory or telemetry paths;
- optional sensor or audio paths.

When changing Beacon behavior, document removed functions and packet changes in the audit files.
