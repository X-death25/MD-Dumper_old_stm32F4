#!/bin/bash

make
make bin
mv *.bin out
make clean
./WeAct_HID_Flash-CLI erase

