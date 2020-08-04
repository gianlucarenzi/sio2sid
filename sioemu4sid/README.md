# misc
This is a simple SIO protocol written to check if the ARDUINO SIO Protocol works

Usage: bin/sioemu /dev/ttyS1

The protocol is very simple but quite time critical and the USB Serial converter used in the Arduino cannot have some registers needed to startup or stop some delays for SIO protocol. But so far seems to work good. The speed limit is the 19200 baud rate updating 31 bytes every vertical blank and it needs more or less 15/16 msecs to manage a single registers write. So it is quite time critical, but nevertheless it works fairly good.

I hope to be clear enough as English is not my native spoken language.

Some comments are left in Italian, so feel free to change to English and request for a push to myself writing me an e-mail:

icjtqrNOSPAM@gmail.com

obviously, removing the NOSPAM from the username.

Regards,
Gianluca
