# 0001: Serial line framing

We start with line mode. Messages are up to 4 lines, 20 chars each, joined with `
` and terminated with a blank line `

`. Arduino parses until blank line then refreshes buffer. Later we can switch to binary framing with STX/ETX and checksum if noise becomes an issue.

Pros: trivial to debug with `pio device monitor`. Cons: less robust to stray bytes.