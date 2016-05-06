USBHostLinux

By Richard Barella Jr.

Credit to Manuel Di Cerbo for original code (2011) "simplectrl.c"

Used to interface with the Android UI.

Currently, the program just sends data and waits for responses to test communication.


### Instructions to Run C-code (from Linux) - for use with associated Android Studio Project:

- install usbutils and libusb (tested version: 0.1.12 :: for Debian distro's: sudo apt-get install libusb-dev, or see http://www.libusb.org/ for more information)
- copy additional file in directory (USBMessage.h - provides specific data formatting and interpretation functions for USB communication)
- Extra: Possibly update VID and PID in USBHostLinux.c to your android's VID and PID (#define VID 0x22b8, for example). Use "lsusb -v | less" and find your device (look at the idVendor & idProduct ==> replace VID and PID with numbers)
- run "make"
- run "sudo ./USBHostLinux" (verify android device is plugged into a usb port, the app is loaded, and it is not in accessory mode yet)
 - first input prompt: integer between 0 and 99
 - second input prompt: integer between 0 and 7
  * NOTE: Since communication structure waits for a recieve after a send, it will block indefinately. To pass this, just click one of the buttons (turn signal, lights, wipers, etc.) and the c-code should recieve the information (and continue).
