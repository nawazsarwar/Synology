# Original Synology Firmware (.pat)

Unmodified, official Synology DSM firmware images, kept here for reference and
re-flashing. **These are Synology's proprietary files — not redistributed** (the
whole directory is git-ignored).

| File | Device | Notes |
|---|---|---|
| `DSM_DS207_1639.pat`    | DS207 (PowerPC) | The box being modernized. DSM 3.1-1639. |
| `DSM_DS207+_1636.pat`   | DS207+          | Close sibling, kept for comparison. |
| `DSM_DS216+II_90080.pat`| DS216+II        | Modern reference image (x86) for protocol/config reference. |

To reinstall stock DSM: use Synology Assistant (Intel/Rosetta on Apple Silicon)
and point it at the DS207 image.
