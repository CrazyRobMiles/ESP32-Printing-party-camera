# Party Camera Software

To use the software just load the folder into PlatformIO and build it. All the required versions of the drivers are part of the project. 
## Adding a different printer
You only need to do this if your printer is not recognised by the thermal printer libaray. The [GitHub site ](https://github.com/bitbank2/Thermal_Printer) for the printer code has a list of printers that are already supported. 

To make the printer code work with my printer I had to modify a file in the library code and add the Bluetooth name of my printer. 

 The file to modify is Thermal_Printer.cpp in the folder *\software\.pio\libdeps\esp-wrover-kit\Thermal Printer Library* 

The printer names are defined on line 59 of the file:

```
const char *szBLENames[] = {(char *)"MPT-II",
```

I used the program BLE Scanner from [Blue Pixel Tech](https://www.bluepixeltech.com) on my mobile phone to find the Bluetooth name of the printer and then changed the first string in the file match this. 
## The camera and CPU Speed
The PlatformIO ini file for this project contains a setting that reduces the clock speed of the ESP32 to 16 MHz. This has been found to make the camera setup more reliable. If you reuse this code in another project you must make sure that you add this setting.
## The camera and the Bluetooth stack
The ESP32 code uses the Bluetooth libraries which are rather large. This project configures the build to produce the "Huge App" version of the code. This is selected in the PlatformIO.ini file If you reuse this code in another project you must make sure you add this setting. 