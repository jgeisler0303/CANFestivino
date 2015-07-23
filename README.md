# History from 07/27/2015
* Extensive changes to reduce SRAM usage
* Removed need for extra timers
* Added write access flag to object dict entry callback
* objdictedit from this repo needed to generate suitable object dictionary definition
* mcp_can, BlinkPattern, digitalWriteFast and Timer libraries from my repos needed to successfully compile
* MCP2515 chip select pin set in CO_can_Arduino.cpp
* Usage: define `CO<red_led_pin, green_led_pin> co;` (-1 if no led needed), call `co.CO_Init();` in `setup()`, call `co.CO_Cycle();` in `loop()`
* See example

# History from 11/02/2015
This is a first working prototype with only the most necessary changes to the original CANFestival code to make it run as an Arduino library.

[Here](https://github.com/jgeisler0303/AGCON/tree/master/Software/Arduino) is an intermediate state of this library before I started to compile it using the Arduino IDE.

The example uses [my fork](https://github.com/jgeisler0303/CAN_BUS_Shield) of the [Seeed Studio CAN bus library](https://github.com/Seeed-Studio/CAN_BUS_Shield). Beware that my example expects the CS of the MCP2515 on a different pin than the Seeed CAN bus shield.

The object dictionary that is implemented by the example must be edited with [my special version](https://github.com/jgeisler0303/AGCON/tree/master/Software/CanFestival/objdictgen) of the CANFestival tool in order to generate code that doesn't conflict with Arduino conventions.

Further documentation to follow.
