# ChromecastManager
HTTP Web interface for Chromecast Devices that handles all of the Protobuf, TLS, MDNS protocols leaving a nice simple HTTP interface

# Building
Ensure SSL Libraries are installed:

sudo apt-get install libssl-dev

make -       builds standard program (logs to syslog)

make debug - builds debug version (reports more info to stdout rather than
             to the syslog)

