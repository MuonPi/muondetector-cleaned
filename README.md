# muondetector 

Software for a [Raspberry Pi based muon detector system](https://MuonPi.org) using a u-blox GNSS module for precise timing. For more information visit our web page [www.MuonPi.org](https://MuonPi.org) and our [Mediawiki](https://wiki.muonpi.org/index.php?title=Main_Page).

## ABSTRACT

This is a software solution for operating a Raspberry Pi mini computer and the u-blox NEO-M8 GNSS module's "timemark" feature together with a plastic scintillator + SiPM-based detector system to detect muons with a time stamping accuracy of up to a 20ns. Therefore, the software has to communicate with the Ublox GPS module through a serial interface using the ubx protocol. The good time accuracy is needed for correlating several independent detector units for the reconstruction of atmospheric muon showers resulting from ultra high-energy cosmic particles impinging on the earth's atmosphere. The software must be easy-to-use and runs in the background while synchronizing accumulated data with a central server.

## DOWNLOAD

The latest binaries can be found as Debian packages with patch-notes and other release specific information in the "Releases" folder.

## INSTALLATION 

### Raspberry Pi setup

1. Enable serial connections on the Raspberry Pi (use either one of the following):
   - Use `sudo raspi-config` > Interfacing options > Serial > Login Shell "no" > Enable serial port hardware "yes"
   - Manually add "enable_uart=1" to /boot/config.txt
2. Enable I2C communication on the Raspberry Pi:
   - `sudo raspi-config` > Interfacing options > I2C > Enable "yes"

### Installation from latest stable release (recommended)

Version 1.1.2 is used as an example. Installing .deb Debian packages: `sudo apt install <./filename>` or on debian jessie `sudo gdebi <'filename'>`. An internet connection might be needed for automatically installing additional needed dependencies.
On your Raspberry Pi:
1. Install "libmuondetector-shared_1.1.2-raspbian.deb" 
2. Install "muondetector-daemon_1.1.2-raspbian.deb"
Depending on the device of you choice, install the GUI for controlling the software via network connection or on the Raspberry Pi itself:
3. Install either one of the following depending on your system of choice:
   - "muondetector-gui_1.1.2-raspbian.deb" on a Raspberry Pi
   - "muondetector-gui_1.1.2-ubuntu_bionic-x64.deb" on a Ubuntu 18.xx machine 
   - "muondetector-gui_1.1.2-windows-x64.zip" on a 64-bit Windows machine 

### Installation from source

The steps to building the daemon are as follows:
1. Install all dependencies
2. Enter the build directory
3. run `cmake ../source`
6. run `make package`
7. enter `output/packages`
8. install the debian packages found there with `sudo apt install ./<filename>.deb`

#### Options
Possible options are: 

`MUONDETECTOR_BUILD_GUI` This defaults to `ON`

`MUONDETECTOR_BUILD_DAEMON` This defaults to `ON` on a raspberry pi system and `OFF` otherwise. Note that you can not turn it on on a non-raspberry pi system.

## TROUBLESHOOTING AND DEPENDENCIES:  

### Dependencies

When trying to create a Makefile with qmake (qt version 5.7.1 on raspbian) there will probably be errors. To install just get it from repository with apt-get install:

- "Project ERROR: unknown module(s) in QT: serialport" there is a missing library "libqt5serialport5-dev".
- "Project ERROR: unknown module(s) in QT: quickwidgets" there is a missing library "c".
- "Project ERROR: unknown module(s) in QT: svg" there is a missing library "libqt5svg5-dev".
- You also need to install "libcrypto++-dev libcrypto++-doc libcrypto++-utils"
- You also need to install "libqwt-qt5-dev" or "libqwt-dev".
- You may also install "lftp" for uploading acquired data to our server.
- For TDC7200 it may be required to manually add "dtoverlay=spi0-hw-cs" to /boot/config.txt

Cheat-Sheet Copy&Paste:

`sudo apt install qt5-default pyqt5-dev qt5-qmake libqt5serialport5-dev libqt5svg5-dev libqwt-qt5-dev libqwt-dev libcrypto++-dev libcrypto++-doc libcrypto++-utils libcrypto++-dev libcrypto++-doc libcrypto++-utils lftp libmosquitto-dev qtdeclarative5-dev libconfig++-dev`

### Troubleshooting

#### Version > 1.1.2

It may be when starting for the first time that, if the daemon is not started as a service, the data folder structure cannot be written due to insufficient rights. Starting the daemon with sudo-er rights should be avoided. Here, the folder structure can be created by hand to solve the issue: copy paste the hashed folder name from the error output of the daemon when started without folder structure and create the folder with `sudo mkdir /var/muondetector/[Hashed Name]` with `notUploadedData` and `uploadedData` as sub-folders. Then, change the user rights with `sudo chown -R pi:pi /var/muondetector`. When restarted, the daemon should be able to write the data. 

## RUNNING THE SOFTWARE

### Version < 1.1.2
On your Raspberry Pi, do:
1. `sudo pigpiod -s 1` for setting the GPIO sampling rate to 1 MHz
2. start the daemon with `muondetector_daemon <device> [options]` where device is your serial interface (either "/dev/ttyS0" for Raspberry Pi 3 & 4 or "/dev/ttyAMA0" for Raspberry Pi 2). The options can be reviewed by adding -h. It is recommended to use the -c option on first start (if the configuration is not yet written to eeprom).
On your network device or on your Raspberry Pi: 
3. Start the gui with `muondetector_gui` on the device of your choice and measure some tasty muons!

### Version >= 1.1.2

1. `sudo pigpiod -s 1` for setting the GPIO sampling rate to 1 MHz
2. start the daemon with `muondetector_daemon <device> [options]` where device is your serial interface (either "/dev/ttyS0" for Raspberry Pi 3 & 4 or "/dev/ttyAMA0" for Raspberry Pi 2). The options can be reviewed by adding `-h`. It is recommended to use the `-c` option on first start (if the configuration is not yet written to eeprom). To log on the MQTT service, add `-l` and enter your user name and password when asked. If you don't have a user account on our server yet, please send a mail to <support@muonpi.org>. With `-- id [station_id]` (integer from 0 to 9) you can set a unique station id in case you operate more than one station with one user account.  
On your network device or on your Raspberry Pi: 
3. Start the gui with `muondetector_gui` on the device of your choice and measure some tasty muons!

