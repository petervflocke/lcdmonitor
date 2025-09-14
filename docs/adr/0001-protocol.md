# 0001: Serial line framing

We start with line mode. Messages are up to 4 lines, 20 chars each, joined with `
` and terminated with a blank line `

`. Arduino parses until blank line then refreshes buffer. Later we can switch to binary framing with STX/ETX and checksum if noise becomes an issue.

Pros: trivial to debug with `pio device monitor`. Cons: less robust to stray bytes.

## Commands v1 (Phase 6)

- Frame format (server → Arduino):
  - First line: `COMMANDS v1`
  - Following lines: `<id> <label>` (server truncates to 20 chars; Arduino parses first space as separator). Up to 12 entries are stored.
  - Arduino appends an implicit `Exit` entry in the UI (does not require a frame line).
- Request/selection (Arduino → server):
  - `REQ COMMANDS` when entering Commands mode (long press). Server responds with the latest commands frame.
- `SELECT <id>` on double press (except when `Exit` is selected). Server logs the selection and may optionally execute a configured command if enabled.
- Feedback:
  - For now, feedback is logging-only on the server (`INFO` level with `--verbose`). No LCD acknowledgement is rendered to keep the UI minimal; shutdown/reboot may terminate before feedback could be displayed anyway. We may add an optional one-line `OK`/`FAIL` toast later.

### Execution model

- Default: execution disabled. Run the daemon with `--allow-exec` to enable.
- Backend: `--exec-driver=systemd|shell` (default: `systemd`).
  - `systemd`: uses `systemd-run --user` to spawn a transient unit per selection. Output goes to journald and can be inspected with `journalctl --user -u lcdcmd-<id>-<ts>`.
  - `shell`: runs the configured `exec` string via `/bin/sh -lc` (less safe; for development only).
