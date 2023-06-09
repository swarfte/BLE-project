# BLE-project

include experiment 1~3

## tested envirenment

> -   manjaro KDE kernel 6.1.31-2
> -   python 3.18.6
> -   node.js 14.21.3
> -   esp-idf v4.1.3

## before start

> -   modify the esp-idf

![image](./image/1.png)

![image](./image/2.png)

![image](./image/3.png)

> -   modify the port in index.js
>     -   windows COM\*
>     -   linux /dev/ttyUSB* or /dev/ttyACM*
>     -   mac /dev/cu.\*

## common command

> -   idf.py -p [port] flash
> -   idf.py -p [port] monitor
> -   idf.py -p [port] erase_flash
