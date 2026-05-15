# Security Policy

Never commit real Wi-Fi credentials, API tokens, GitHub tokens, NAS/SMB/WebDAV credentials, private keys, wallet private keys, seed phrases, `.env`, `secrets.h`, or `credentials.json`.

The import found inline credential patterns in firmware/config notes and copied only sanitized versions. Real source files were not edited. Use `configs/examples/secrets.example.h` or `configs/examples/config.example.json` as templates and keep real values outside git.

Firmware paths that use Wi-Fi, Stratum, or continuous worker loops are not safe default entrypoints. Verify cold-compute policy, board configuration, and network endpoints before running.
