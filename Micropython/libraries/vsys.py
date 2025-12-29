from machine import Pin, ADC
import utime

def get_vsys():
    samples = 300
    reads = 10
    vsys = ADC(Pin(29))
    u = 0
    umax = 0
    t = utime.ticks_us()
    for j in range(0, reads):
        for i in range(0, samples):
            u += vsys.read_u16()
        u /= samples
        umax = max(u, umax)

    print(f"Zeit = {round((utime.ticks_us() - t)/1000)} ms  Samples = {samples:<4}  Uvsys = {round(u * 3300 / 65535)} mV  Umax = {round(umax * 3300 / 65535)} mV")
