"""
ESP-NOW receiver, zum Test des Inglenook Siding Remote Controllers
auf ESP32-C3 Super mini
"""
import network
import espnow
from machine import Timer, SoftUART, Pin, idle, reset, soft_reset, deepsleep, reset_cause, I2C
import esp32
import struct
import binascii
import utime
from micropython import const

# __DEBUG__ = const(0)
__DEBUG__ = const(1)

# receiver_mcu = "XIAO ESP32-C3"
# receiver_mcu = "ESP32-C3 super mini"
receiver_mcu = "ESP32-C3 super mini 7 Pins"

# ESP32-C3 (7 Pins) super mini vs XIAO
#    +---+---+---+                 +---+---+---+
#  2 |   |   |   | 5V            5 |   |   |   | 5V
#  3 |   +---+   | GND           6 |   +---+   | GND
#  4 |           | 3.3V          7 |           | 3.3V
#  5 |           | 10            8 |           | 4
#  6 | XIAO ESP  | 9             9 | ESP super | 3
#  7 |           | 8            10 |   mini    | 2
# 21 |TX       RX| 20           20 |RX       TX| 1
#    +-----------+              21 |           | 0
#                                  +-----------+

# XIAO RP2040
#    +---+---+---+
# 26 |   |   |   | 5V
# 27 |   +---+   | GND
# 28 |           | 3.3V
# 29 |           | 3
#  6 |   XIAO    | 4
#  7 |  RP 2040  | 2
#  0 |TX       RX| 1
#    +-----------+


USE_UART = True

if(receiver_mcu == "ESP32-C3 super mini 7 Pins"):
    UART_CHANNEL = 1
    TX_PIN = 1  #-->(9 an RPi RP2040, 1 an XIAO)
    RX_PIN = 20  #-->(8 an RPi RP2040, 0 an XIAO)
    CONNECTED_PIN = 6
    LED_PIN = 5  
    RESET_PIN = 4
elif(receiver_mcu == "XIAO ESP32-C3"):
    UART_CHANNEL = 1
    TX_PIN = 21
    RX_PIN = 20
    CONNECTED_PIN = 3
    LED_PIN = 2        
    RESET_PIN = 10
elif(receiver_mcu == "ESP32-C3 super mini"):
    UART_CHANNEL = 1
    TX_PIN = 21  #-->(9 an RPi RP2040, 1 an XIAO)
    RX_PIN = 20  #-->(8 an RPi RP2040, 0 an XIAO)
    CONNECTED_PIN = 6
    LED_PIN = 5  
    RESET_PIN = 4


LOW = const(0)
HIGH = not(LOW)


if (receiver_mcu == "XIAO ESP32-C3"):
    LAYOUT_NAME = "Mini-Rangierpuzzle\n"
elif (receiver_mcu == "ESP32-C3 super mini 7 Pins"):
    LAYOUT_NAME = "Mini-Rangierpuzzle\n"
elif (receiver_mcu == "ESP32-C3 super mini"):
    LAYOUT_NAME = "Teststrecke"

def log_print(string, end="\n"):
    if __DEBUG__:
        print(string, end)

def log_print_mac(host):
    p1, p2, p3, p4, p5, p6 = struct.unpack("BBBBBB", host)
    return f"b'\\x{hex(p1)[2:4]}\\x{hex(p2)[2:4]}\\x{hex(p3)[2:4]}\\x{hex(p4)[2:4]}\\x{hex(p5)[2:4]}\\x{hex(p6)[2:4]}'"

def encode_mac(host):
    k = []
    for c in host:
        k.append(int(c))
    return k


def shutdown(host=None):
    if host != None:
        log_print(f"Delete Host: {host}")
        try:
            e.del_peer(host)
        except OSError as error:
            log_print(error)
            pass
    if e.peer_count == 0:
        led_pin.on()      #                     1          0
    log_print("Gehe schlafen...")
    deepsleep()
    if reset_cause() == machine.DEEPSLEEP_RESET:
        log_print('Aufgewacht!')
    reset()
    
def uart_read():
    if uart != None:
        c = ""
        s = ""
        while (uart.any()):
            c = (uart.read()).decode('utf-8')
            if(not c == " "):
                s += c
        if s > "":
            return s
        else:
            return None

def start_sequence():
    for i in range(3):
        connected.on()
        utime.sleep(0.3)
        connected.off()
        utime.sleep(0.3)
    

connected = Pin(CONNECTED_PIN, Pin.OUT)
connected.off()
led = Pin(LED_PIN, Pin.OUT)
led.off()
start_sequence()

sta = network.WLAN(network.STA_IF)
sta.active(True)
mac = sta.config('mac')
mac_address = "b'" + ''.join('\\x%x' % b for b in mac) + "'"
log_print(mac_address)

reset_pin = Pin(RESET_PIN, Pin.IN, Pin.PULL_UP)

"""
typedef struct {
  int address;
  uint16_t cv;
  uint8_t direction_on_off;
  uint8_t speed_ascpect_function_value;
} SUBCOMMAND_TYPE;

typedef struct {
  MESSAGE_TYPE msg_type;
  uint8_t mac[6];
  int throttle_id;
  int size;
  SUBCOMMAND_TYPE subcommand;
} COMMAND_TYPE
"""

recv_format = "bi6siL224s"
send_format = "biBBBBBBiL224s"

log_print("Starte ESP-Now")
e = espnow.ESPNow()
# e.active(False)
# utime.sleep(100)
e.active(True)
old_host = None
        
connected.off()

emerg = False;
restart = False;
stop = False;

uart = None
if USE_UART:
    log_print("Starte UART")
    uart = UART(UART_CHANNEL, baudrate=9600, tx=Pin(TX_PIN), rx=Pin(RX_PIN))

while True:
    if(reset_pin.value() == LOW):
        begin = utime.ticks_ms()
        while (reset_pin.value() == LOW):
            pass
        if(utime.ticks_ms() - begin > 2000):
            connected.off()
            e.active(False)
            led.off()
            utime.sleep(0.5)
            reset()
    peers, enc = e.peer_count()
    if(peers > 0):
        led.off()
        connected.on()
    else:
        led.on()
        connected.off()
    host, msg = e.recv(1000)
    if (host): # and (host in clients)):
        host_mac = ''.join('\\x%x' % b for b in host)
        if(host_mac != old_host):
            log_print("HOST ", host_mac)
            old_host = host_mac
        if msg:             # msg == None if timeout in recv()
            msg_type, client_id, broadcast_mac, packet_num, length, text = struct.unpack(recv_format, msg)
            log_print(f"Packet {packet_num}: {'Pairing Request' if msg_type == 0 else 'Data' } from Client # {client_id}[{log_print_mac(broadcast_mac)}] received {length} bytes, text = {text[:length].decode('utf-8')}")
            mac_sender = encode_mac(mac)
            if(client_id > 0):
                if (msg_type == 0): 
                    try:
                        e.add_peer(host)
                    except OSError as error:
                        log_print(error)
                        if error == (-12395, "ESP_ERR_ESPNOW_EXIST"):
                            e.del_peer(host)
                            e.active(False)
                            utime.sleep(100)
                            e.active(True)
                            e.add_peer(host)
                    
                    answer = struct.pack(send_format, 0,
                                             ~client_id,
                                             mac_sender[0],
                                             mac_sender[1],
                                             mac_sender[2],
                                             mac_sender[3],
                                             mac_sender[4],
                                             mac_sender[5],
                                             packet_num,
                                             9, LAYOUT_NAME
                                             )
                    try:
                        log_print("Send ", struct.unpack(send_format, answer))
                        e.send(host, answer, False)
                        utime.sleep_ms(100)
                    except OSError as error:
                        log_print(error)

                elif peers > 0:
                    try:
                        peer = e.get_peer(host)
                        host = peer[0]
                        command = text[:length].decode('utf-8')
                        log_print(command)
                        if(command[:3] == "<<<" and command[-3:len(command)] == ">>>"):
                            if command == "<<<QUIT>>>":
                                log_print("shutdown()")
                                stop = True
                            elif command == "<<<EMERG>>>":
                                log_print("emerg()")
                                emerg = True
                                
                            elif command == "<<<RESET>>>":
                                log_print("reset()")
                                restart = True
                                
                            log_print(f"Weitergeben ='{command}'")
                            if uart != None:
                                uart.write(command)
                            
                        if (stop or restart or emerg):
                            if(stop):
                                shutdown(host)
                            else:
                                soft_reset()
                            stop = False
                            restart = False
                            emerg = False
                        
                        if uart != None:
                            response = uart_read()
                        elif i2c != None:
                            response = i2c_read()
                        if response != None:
                            log_print(f"Zurueck ='{response}'")
                            answer = struct.pack(send_format, 1,
                                 ~client_id,
                                 mac_sender[0],
                                 mac_sender[1],
                                 mac_sender[2],
                                 mac_sender[3],
                                 mac_sender[4],
                                 mac_sender[5],
                                 packet_num,
                                 len(response), response
                                 )
                            e.send(host, answer, False)
                        

                    except OSError as error:
                        log_print(error)
                        if error == (-12393, "ESP_ERR_ESPNOW_NOT_FOUND"):
                            pass

                else:
                    pass




