# Arduino Nano to HD44780 20x4 LCD wiring

Pin;Arduino Nano;LCD 20x4 (HD44780)

LCD Pin	Function
1	Ground (Vss)
2	+5 Volts (Vdd)
3	Contrast Adjust (Vee)
4	RS
5	R/W (or Ground)
6	Enable (E)
7	D0 (no used)
8	D1 (no used)
9	D2 (no used)
10	D3 (no used)
11	D4
12	D5
13	D6
14	D7
15	Anode (LED+)
16	Cathode (LED-)

## LCD wiring

# LCD;Arduino IO;Function
1 VSS;GND;VSS (GND)
2 VDD;+5V;VDD (+5V)
3 VO;10k pot wiper;VO (contrast) resistor devider
4 RS;D7;RS
5 RW;GND;RW (GND)
6 E;D8;E
11 D4;D9;D4
12 D5;D10;D5
13 D6;D11;D6
14 D7;D12;D7
15 A (LED+);+5V via 220Ω;LED+
16 K (LED-); 150ohm resistor -> GND;LED-

## Rotary encoder wiring (later phase)
Pin;Arduino Nano
A;D2
B;D3
Button;D4
GND;GND
VCC;+5V