# fzkmmidi
FZ/KM midi (Fuzix-running PC with 1 chip)


# fzkmweb
FZ/KM web

## about FZ/KM midi
FZ/KM midi is a 1 chip PC for running Fuzix.

## how to compile
Use XC32 ver 1.32 (or later). Optimization level is 1. Stack and heap sizes are 512 bytes.

## how to use
Update the firmware to "fzkmidi.hex" by using Pickit. Download the "tomssbc-0.3.ide" from http://www.fuzix.org/ and copy to USB memory as file "FUZIXIMG.IDE".

## License
Most of codes (written in C) are provided with LGPL 2.1 license, but some codes are provided with MachiKania license that allows users to use the code and object only for non-profit purpose. The binary and source code also contains Fuzix Kernel (GPL 2.0) as a pre-installed operating system.