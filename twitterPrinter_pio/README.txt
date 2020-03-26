
NOTE: This is a Platform IO project folder. Please visit https://platformio.org/ if you do not have an IDE with Platform IO installed already. 


This project is intended to be used with a thermal printer like the one shown here: https://www.adafruit.com/product/597

The serial pins for this project are D5 and D6 for RX and TX respectivly. This can be changed to your preferred pins though.


To use this program, 
1. flash your ESP type device and wait for it to boot up. The first time it boots it will not have any wifi or mqtt settings. 

2. To fix this, join the network created by the device (it should be something like "NEW-ESP-HOTSPOT")

3. Once connected, open http://192.168.1.1 in your browser and set the desired topic, then go to http://192.168.1.1/config and set your wifi and mqtt credentials

4. Now you should be all set to go. You can test by running the twitterScraper.py program included in this package and it should print out a recent tweet. 

