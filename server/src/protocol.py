from dataclasses import dataclass

START = b""  # optional if you later want binary framing
END = b""    # not used in line mode yet

# For simplicity we start with line mode: join lines with 
 and end with 


@dataclass
class Outbound:
    lines: list[str]

    def encode(self) -> bytes:
        # Truncate to 20 chars per LCD line
        norm = [(s[:20] if len(s) > 20 else s) for s in self.lines]
        return ("
".join(norm) + "

").encode()
