/*
  JANUS ADV_ELITE RC1 — PATCHWORK FLASH CANDIDATE
  ------------------------------------------------
  Purpose: first hardware-flash candidate for the single final ADV_Elite.

  Preserved doctrine:
  - one ADV, one body;
  - real sensor truth != prediction != simulation;
  - Blind Eye is ESP-NOW input (it has no microphone);
  - anomaly detector is always on and independent from Love/House/game;
  - 1488 is the manual extended M2R gate;
  - J is manual LoRa gate; LOVE context = HOUSE && LORA;
  - ENTER is absolute audio mute;
  - [ ] share one brightness axis for display + enabled LED;
  - L gates only the LED;
  - foreground O/Z/R/D/A never stops P0/P1/P2 core work;
  - game/pet/visuals are FICTIONAL or VISUALIZATION and never OBSERVED_REAL.

  This file is mirrored from firmware/adv_elite_rc1/ADV_Elite_RC1.ino.
  See that file for the complete RC1 source used to replace the old Elite Zero body.
*/

#include "../adv_elite_rc1/ADV_Elite_RC1.ino"
