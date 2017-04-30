This is a Linux driver for a family of similar LED display controllers
which are used in some boards to drive 7-segment LED displays and,
optionally, provide input support for up to a few tens of buttons.

The bus between the CPU and the display driver IC is accessed purely by
bit-banging three GPIO pins (which are defined in the DTS file,
example included).

Driver supports arbitrary character output (limited by 7 segment geometry),
an additional bitmap overlay (which can be used to light additional icons
on the display), variable brightness, on/off, Linux input device support
for button input.

Currently, the driver supports the following ICs:
PT6964, SM1628, TM1623, FD268
