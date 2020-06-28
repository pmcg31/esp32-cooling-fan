<p align="center">
  <a href="https://www.espressif.com/en/products/hardware/esp32/resources">
    <img alt="Espressif" src="https://www.espressif.com/sites/all/themes/espressif/logo.svg" width="500" />
  </a>
  <a href="https://www.arduino.cc/en/main/software">
    <img alt="Arduino" src="https://www.arduino.cc/en/pub/skins/arduinoWide/img/ArduinoAPP-01.svg" width="100" />
  </a>
</p>
<h1 align="center">
  ESP32 Cooling Fan with PID control
</h1>

ESP32 project for a PID controlled 4-pin cooling fan using AdaFruit si7021 breakout with integrated web interface and OTA firmware updating

## ⚙️ Gear you need

### 🛠 Hardware

You will need:

 - ESP32 development board. I used an [ESP32-DevKitC](https://www.amazon.com/gp/product/B0811LGWY2/ref=ppx_yo_dt_b_asin_title_o09_s00?ie=UTF8&psc=1). Any other development board with an ESP32 module on it should work, but the pin assignments could possibly be different.
 - [AdaFruit si7021 breakout board](https://www.amazon.com/Adafruit-Si7021-Temperature-Humidity-Breakout/dp/B01M0BJ139/ref=sr_1_1?dchild=1&keywords=si7021+adafruit&qid=1593365571&sr=8-1)
 - A 4-pin computer fan. I used a [Noctua NF-A 12x25](https://www.amazon.com/Noctua-NF-A12x25-PWM-Premium-Quality-Quiet/dp/B07C5VG64V/ref=sxts_sxwds-bia-wc-p13n1_0?crid=1X9EJ5YZR0YP7&cv_ct_cx=noctua+120mm+fan&dchild=1&keywords=noctua+120mm+fan&pd_rd_i=B07C5VG64V&pd_rd_r=d3cc2313-865c-403a-8c1c-7a752b181207&pd_rd_w=nymBV&pd_rd_wg=Oyg5k&pf_rd_p=1da5beeb-8f71-435c-b5c5-3279a6171294&pf_rd_r=HGYDSD7V1T9XXXM9N3BY&psc=1&qid=1593365715&sprefix=noctua%2Caps%2C275&sr=1-1-70f7c15d-07d8-466a-b325-4be35d7258cc), but any 4-pin fan should work just fine.
 - A P-channel MOSFET and an NPN transistor, and some resistors. More on this later. 
 - A power source that can supply 12V and 5V. The version of this I built was powered directly from a 12V solar storage bank, and I used [this board](https://www.amazon.com/gp/product/B079N9BFZC/ref=ppx_yo_dt_b_asin_title_o00_s00?ie=UTF8&psc=1) to provide 5V.

### 📀 Software

You need the [Arduino IDE](https://www.arduino.cc/en/main/software), of course. It must be set up for ESP32 development, including the sketch data uploader. Check out [my guide](https://ideaup.online/blog/esp32-set-up-on-arduino/) if you need help!

You will also need the ESPAsyncWebServer and ESPAsyncTCP libraries. I have a [guide to downloading and setting those up](https://ideaup.online/blog/esp32-webserver-with-websockets/), too!

Finally you will need the `Arduino_JSON` library and the `Adafruit Si7021 Library`. Both are available from the Arduino Library Manager in the Arduino IDE. I don't have a guide for these yet, but installation is easy. Click on `Tools` and then `Libraries...` in the IDE. This will bring up the library manager. Search for each of the two libraries above by name, and install the latest version by clicking the `Install` button that appears when you hover over the library in the list. That's it!

## 🧩 Getting set up

Once you have this project downloaded there are still a couple of things to do.

### 👫 Hookups

I still need to put together a circuit diagram for this...coming soon!

### 📡 WiFi credentials

You will need to rename the file `sample.config.json` to `config.json` and move it to the `data` directory. Edit the file to reflect the ssid and key for your network. The ESP32 will connect to this network and attempt to establish an mDNS responder.

## 🚀 Launching the project

Upload sketch data to the board first, then upload the sketch. Watch the serial monitor for WiFi to connect. If the mDNS responder was set up successfully, browse to [http://dfan.local](http://dfan.local) to see a UI with real-time updates. If the mDNS didn't work, point your browser to the IP printed in the serial monitor instead.

Windows users: Windows 10 (and possibly earlier versions) does not do mDNS by default, meaning that the '.local' addresses will not work. Ironically, downloading and installing the [Apple BonJour print services](https://support.apple.com/kb/dl999?locale=en_US) enables mDNS.

<p align="center" style="padding-top: 50">🍀 Good Luck! 🍀
