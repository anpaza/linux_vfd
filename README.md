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

Additionaly, a highly configurable daemon program is provided that will
fill the LED display with various useful information.

What was tested and WILL work:

- The LED display on a X92 AMLogic S912-based Android TV box.
  This box uses an FD628 display controller and a common-anode LED display.
  This board has no input buttons connected to FD628.

What was not tested but SHOULD work:

- Display controllers based on the following chips should be 100% compatible:
  PT6964, SM1628, TM1623, FD268.

What was not tested but COULD work:

- The Linux input event code has not been tested yet due to missing hardware.
  However, I believe it should work, because I have tested it by modifying
  the code so that it thinks some hardware buttons are pressed.
