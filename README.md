# Discord Controlled Remote PC Power Switch

This project utilizes an ESP32 microcontroller to control the power switch of a remote PC via Discord commands. By integrating Discord with the ESP32, users can remotely power on, off, or reset their PC from anywhere with an internet connection. Keep in mind that this is a very quick and dirty solution that I put together in a few hours if I ever decide to make some updates to my code I will add them here.

## Setup

To check the computer you need to connect the ESP32's GPIO34- pin to something in the computer that outputs 3.3 V only when the computer is powered on. Originally I was supposed to do this by soldering a SATA power connector to the PCB and connecting it to the pin, but I had some problems with my soldering iron. Because of that, I decided just to connect the pin to my motherboard's TPM header 3.3 V pin since that only outputs power when the PC is powered on. This isn't the most elegant solution but hey it works. Instead of the TPM header, you could also use some unused header, in your motherboard such as a USB 2.0 header but you might need to add some resistors in between if the voltage isn't correct.

You should also take the ESP32s power from the PC to make sure they have common ground if you are hooking it up to the PC via pins.

### Components
* ESP32 Development Board
* Relay Module
* Jumper Wires
* Something to test if the pc is on or of


### Simple diagram of the contraption
This diagram was made using [wokwi](https://wokwi.com) 

![](https://github.com/Jonkke9/ESP32-Discord-power-switch/blob/main/Diagram.png)
