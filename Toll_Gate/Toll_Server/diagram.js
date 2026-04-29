{
  "version": 1,
  "author": "Elamaran V",
  "editor": "wokwi",
  "parts": [
    { "type": "board-esp32-devkit-c-v4", "id": "esp", "top": 0, "left": 0, "attrs": {} },
    { "type": "board-ssd1306", "id": "oled1", "top": -50, "left": 150, "attrs": { "i2cAddress": "0x3c" } }
  ],
  "connections": [
    [ "esp:TX", "$serialMonitor:RX", "", [] ],
    [ "esp:RX", "$serialMonitor:TX", "", [] ],
    [ "esp:GND.1", "oled1:GND", "black", [ "v0" ] ],
    [ "esp:3V3", "oled1:VCC", "red", [ "v0" ] ],
    [ "esp:21", "oled1:SDA", "green", [ "v0" ] ],
    [ "esp:22", "oled1:SCL", "yellow", [ "v0" ] ]
  ],
  "dependencies": {}
}