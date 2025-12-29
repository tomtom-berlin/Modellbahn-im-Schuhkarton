from micropython import const
from machine import Pin
import time

BTN_1 = const(14)
BTN_2 = const(15)
LED_1 = const(16)
LED_2 = const(17)

isr_flag = [False, False]
isr_pin = 99

pin14 = Pin(BTN_1, Pin.IN)
pin15 = Pin(BTN_2, Pin.IN)
pin16 = Pin(LED_1, Pin.OUT)
pin17 = Pin(LED_2, Pin.OUT)

pin16.off()
pin17.off()

def isr(pin):
    global isr_flag
    global isr_pin
    if pin == Pin(BTN_1):
        isr_flag[0] = not isr_flag[0]
        isr_pin = BTN_1
    else:
        isr_flag[1] = not isr_flag[1]
        isr_pin = BTN_2

pin14.irq(handler = isr, trigger = Pin.IRQ_FALLING)
pin15.irq(handler = isr, trigger = Pin.IRQ_FALLING)

def test_buttons():
    global isr_flag
    global isr_pin
    while True:
        print(f"Interrupt: {isr_flag} auf Pin {isr_pin}")
        if isr_pin == 14:
            pin16.value(not pin16.value())
        elif isr_pin == 15:
            pin17.value(not pin17.value())
        isr_pin = 99
        time.sleep(1)
