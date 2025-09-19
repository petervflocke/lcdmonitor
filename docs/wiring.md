# Arduino Nano to HD44780 20x4 LCD wiring

## LCD signal mapping

| LCD Pin | Label       | Description                               | Arduino Connection |
|--------:|-------------|-------------------------------------------|------------------------------------------|
| 1       | VSS         | Ground reference                          | GND                                      |
| 2       | VDD         | +5 V supply                               | +5V                                      |
| 3       | VO          | Contrast control via 10 kΩ potentiometer  | Potentiometer wiper (ends to +5V and GND)|
| 4       | RS          | Register select                           | D7                                       |
| 5       | RW          | Read/write (tied low for write)           | GND                                      |
| 6       | E           | Enable                                    | D8                                       |
| 7       | D0          | Data bit 0 (unused)                       | Leave unconnected                        |
| 8       | D1          | Data bit 1 (unused)                       | Leave unconnected                        |
| 9       | D2          | Data bit 2 (unused)                       | Leave unconnected                        |
| 10      | D3          | Data bit 3 (unused)                       | Leave unconnected                        |
| 11      | D4          | Data bit 4                                | D9                                       |
| 12      | D5          | Data bit 5                                | D10                                      |
| 13      | D6          | Data bit 6                                | D11                                      |
| 14      | D7          | Data bit 7                                | D12                                      |
| 15      | LED+ (A)    | Backlight anode                           | +5V through 220 Ω resistor               |
| 16      | LED− (K)    | Backlight cathode                         | GND                                      |

## LCD wiring               |

**Notes**
- Mount the 10 kΩ potentiometer between +5 V and GND, with the wiper on VO, to adjust LCD contrast.
- If your LCD module includes an onboard current-limiting resistor, you can omit the external resistors for pins 15 and 16.

## Rotary encoder wiring 

| Encoder Signal | Arduino Nano Pin | Notes                        |
|----------------|------------------|------------------------------|
| A              | D2               | Attach interrupt-capable pin |
| B              | D3               | Attach interrupt-capable pin |
| Button         | D4               | Add soft pull-up             |

Add hardware RC debouncing!
