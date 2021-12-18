# ESE519_FinalProject_Team9
This is the code of final project-multifunctional water dispenser- in ESE519 Team 9 (Xiyue Luo &amp; Shuyi Ying)
The project contains 3 libraries and a main file.
The devpost link is https://devpost.com/software/multi-functional-water-dispenser
# DS1307 Libray
In this library, several communiacation protocols are allowed, such as I2C and SPI etc. Here, I2C protocol is used to get information from DS1307.
DS1307.h and DS103.c uses the library of I2C, and then transfer the received data to the human-like time information.
# LCD Library
In this library, ST7735R driver (ST7735R.h and ST7735R.c) is used to build SPI communiaction between LCD and Arduino Uno. 
LCD_.h and LCD_.c uses the library of ST7735R and control the display on LCD.
# print Library
In this library, UART communication is built to print information in serial monitor, which is convenient for debugging.
