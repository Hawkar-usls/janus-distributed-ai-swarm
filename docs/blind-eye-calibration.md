# Blind Eye Calibration

`firmware/blind_eye/BLIND_EYE.ino` is the focused Blind Eye entrypoint.

## Review Priorities

- Separate current presence from remembered presence.
- Separate prediction from current sensor truth.
- Check TMOS/PIR/STHS34PF80 calibration values on hardware.
- Verify anti-ghost behavior against empty-room and edge-motion cases.
- Confirm that prediction increases confidence or warning state without declaring current human presence by itself.

Hardware logs should include sensor baseline, threshold, current reading, prediction error, and presence state.
