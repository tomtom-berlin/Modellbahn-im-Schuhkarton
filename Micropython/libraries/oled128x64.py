#from machine import Pin, SoftI2C
from machine import Pin, I2C
from libraries.ssd1309 import Display
from libraries.xglcd_font import XglcdFont as fm

# einbinden der OLED-Anzeige via I2C an Pin 5 (SCL) und 4 (SDA)
class OLED128x64:

    def __init__(self, scl_pin=5, sda_pin=4):
        self.i2c = I2C(0, scl=Pin(scl_pin), sda=Pin(sda_pin))
#        self.i2c = SoftI2C(scl=Pin(scl_pin), sda=Pin(sda_pin), freq=100000)
        self.oled = Display(i2c=self.i2c, address=0x3c)
#         self.oled = Display(i2c=self.i2c, address=0x78)

    def display_text(self, col=0, line=0, text=""):
        self.oled.draw_text(col, line, text, self.font, rotate=0)
        self.oled.present()

    def get_text_height(self):
        text_height = self.font.height
        return text_height

    def start_screen(self):
        self.set_font("PerfectPixel_18x25.c", 18, 25)
        self.display_text(21, 5,  "Digital")
        self.display_text(12, 37, "Rangieren")

    def set_font(self, font_name="FixedFont5x8.c", bbox_w=5, bbox_h=8):
        self.font = fm(f"/libraries/fonts/{font_name}", bbox_w, bbox_h)

    def clear(self):
        self.oled.clear()

    def cleanup(self):
        self.oled.cleanup()
