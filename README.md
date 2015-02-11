This is a first working prototype with only the most necessary changes to the original CANFestival code to make it run as an Arduino library.

[Here](https://github.com/jgeisler0303/AGCON/tree/master/Software/Arduino) is an intermediate state of this library before I started to comipe it using the Arduino IDE.

The example uses [my fork](https://github.com/jgeisler0303/CAN_BUS_Shield) of the [Seeed Studio CAN bus library](https://github.com/Seeed-Studio/CAN_BUS_Shield). Beware that my example expects the CS of the MCP2515 on a different pin than the Seeed CAN bus shield.

The object dictionary that is implemented by the example must be edited with [my special version](https://github.com/jgeisler0303/AGCON/tree/master/Software/CanFestival/objdictgen) of the CANFestival tool in order to generate code that doesn't conflict with Arduino conventions.

Further documentation to follow.
