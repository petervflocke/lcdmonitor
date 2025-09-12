from dataclasses import dataclass

START = b"\x02"  # optional if you later want binary framing
END = b"\x03"  # not used in line mode yet

# Start with simple line mode: join lines with "\n" and end with an extra blank line


@dataclass
class Outbound:
    lines: list[str]

    def encode(self) -> bytes:
        # Truncate to 20 chars per LCD line
        norm = [(s[:20] if len(s) > 20 else s) for s in self.lines]
        return ("\n".join(norm) + "\n\n").encode()
