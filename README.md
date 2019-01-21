# FFA320-Connector

This plugin adds a link between the FlightFactor A320 
internal variables to X-Plane Commands and Datarefs.
You can define your own commands and datarefs in the config.cfg.

## Download the stable version

If you are not interested in the source code and you just
want to download the compiled version, visit this link:
https://github.com/Luuk3333/FFA320Connector/releases. Download the .zip, extract it and copy the 'FFA320Connector' folder to the FFA320's plugin folder.

## Config files
You can define your own command, comdef and dataref entries in a new '.cfg' config file in the same directory as the 'config.cfg' file.

## Build Instructions
### On Windows
To build the FFA320 Connector, you need Visual C++ 2013 or
later. Installing Visual Studio 2013 is recommended.
### On MacOS
To build the FFA320 Connector on MacOS, you need Xcode 8.1
or higher. C++11 compiler is required.

### Installing
When building with Visual Studio or Xcode the .xpl file will be copied to /bin/FFA320Connector/64. To install the built plugin, copy the /bin/FFA320Connector folder to the FFA320's plugin folder.
