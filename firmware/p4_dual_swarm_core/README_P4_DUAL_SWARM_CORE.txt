JANUS P4 DUAL SWARM CORE v1.1A_pea4_memory

This is the full P4-side sketch for the planned Janus dual-P4 mechanics.

Roles:
  P4_A:
    HASH / SCOUT / INTENTION
    Main compute searcher. Produces best_z, proof frames, H/s, anchors.

  P4_B:
    MIRROR / VERIFY / MEMORY
    Verifies peer frames, mirrors the job seed, keeps anchor memory.

  P4_C:
    COORDINATOR / RELAY
    Reserved relay role if a third P4 is used.

Important:
  ESP32-P4 has no native Wi-Fi/Bluetooth.
  This sketch intentionally avoids WiFi.h and esp_now.h.
  A companion ESP32-C6/S3 should forward JP4 frames over ESP-NOW/Wi-Fi/pool.

How to flash:
  1. For first P4:
     #define JANUS_DEFAULT_ROLE "P4_A"

  2. For second P4:
     #define JANUS_DEFAULT_ROLE "P4_B"

  3. Optional UART companion:
     #define JANUS_USE_UART_BRIDGE 1
     Set JANUS_UART_RX_PIN and JANUS_UART_TX_PIN for your board wiring.

Serial commands:
  help
  status
  role A
  role B
  mode AUTO
  mode HASH
  mode MIRROR
  mode VERIFY
  job 1234abcd 24 testjob
  seed cafebabe
  save
  clearstate
  reset

Bridge frame:
  JP4,1,node,role,mode,seq,nonce,salt,job,z,digest,hps*CSUM

Mechanics included:
  - dual role split
  - adaptive AUTO mode
  - SHA-256 compute stream
  - best_z / proof events
  - job seed and target_z
  - peer heartbeat frame
  - checksum verification
  - mirror verification queue
  - anchor memory ring
  - blackboard score
  - intention score
  - UART/Serial bridge protocol
  - NVS memory for role, mode, best_z, best digest, job seed and counters
  - commands save / clearstate

What still belongs on the companion:
  - ESP-NOW transport
  - Wi-Fi
  - real pool network connection
  - forwarding JP4 frames between P4_A, P4_B, ADV, StickS3, Beacon

PEA4 note:
  This file is the compute/protocol core. It is not the stock-like display
  shell. The shell lives in:

    LOCAL_SKETCHBOOK_PATH

  Keep both tracks separate until display/touch shell and JP4 compute core are
  stable. Then merge them into a future PEA4_TITAN_SHELL_CORE.
