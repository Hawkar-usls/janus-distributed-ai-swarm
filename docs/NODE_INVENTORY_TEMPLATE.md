# Node Inventory Template

Use this template to record the home swarm without exposing private network data.

```json
{
  "node_id": "Buzz",
  "role": "master",
  "hardware": "ESP32-S3 / M5Stack / other",
  "firmware": "version string",
  "transport": ["ESP-NOW", "Wi-Fi"],
  "observer_only": false,
  "pool_facing": true,
  "nas_facing": true,
  "notes": "Sanitized notes only."
}
```

Known roles:

- master
- worker
- sensor
- display
- cockpit
- camera_presence
- nas_brain
- pc_hrain_observer

Unknown nodes should be listed as `needs_user_inventory` rather than guessed.
