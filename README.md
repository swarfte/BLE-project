# BLE-project

include experiment 1~3

## tested envirenment

> -   manjaro KDE kernel 6.1.31-2
> -   python 3.8.16
> -   node.js 14.21.3
> -   esp-idf 4.1.3
> -   nRF Mesh 3.3.0

## before start

> -   modify the **esp-idf** folder (by default , the folder in the home directory in linux system)

![image](./image/1.png)

![image](./image/2.png)

![image](./image/3.png)

> -   modify the [port] in **index.js**
>     -   **windows**: COM\*
>     -   **linux**: /dev/ttyUSB\* or /dev/ttyACM\*
>     -   **mac**: /dev/cu.\*
> -   run the setup command

## How to flash the project

> 1. cd to the folder which you want to flash (it should contain the main folder)
> 2. use a usb cable connect the gateway and laptop
> 3. modify in the main.c file, to **uncomment** the M5STACK or GAMEBOY define before **flash** ( for experiment 2)
> 3. use the **erase_flash** command to erase the device
> 4. use the **flash** command to flash the device
> 5. use the **monitor** command to check the flash action whether success or not

## common command

> -   **. $HOME/esp/esp-idf/export.sh** (setup command)
> -   sudo idf.py clean
> -   sudo idf.py build
> -   sudo idf.py -p [port] -b 115200 erase_flash
> -   sudo idf.py -p [port] -b 115200 flash
> -   sudo idf.py -p [port] -b 115200 monitor

## Precautions

> -   **insert the sd card** before you turn on the client node for saveing the data
> -   use a usb cable connect the gateway and laptop and **start the index.js** which in the serial folder to save the data
