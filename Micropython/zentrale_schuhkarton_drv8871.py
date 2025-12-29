# Zentrale
# Version 0.76ß 2025-12-24
#

from classes.operationmode import OPERATIONS as OP
from classes.servicemode import SERVICEMODE as SM
from classes.prompt import PROMPT
from classes.map import MAP
import time
from machine import Timer, Pin, UART, idle, reset, PWM, deepsleep
import os, re
from tools.byte_print import int2bin
from micropython import const
from classes.parser import DCCPARSER as DCCP
import rp2

# __DEBUG__ = const(0)
__DEBUG__ = const(1)
# __DEBUG__ = const(2)
# __DEBUG__ = const(3)

AUTO_DETECTION = const(False)  # Lok erkennen beim Einschalten

mcu = "XIAO-RP2040"
# mcu = "XIAO-RP2350"
# mcu = "RPi RP2040"
# mcu = "RPi RP2350"

# XIAO RP2040
#    +---+---+---+
# 26 |   |   |   | 5V
# 27 |   +---+   | GND
# 28 |           | 3.3V
# 29 |           | 3
#  6 |   XIAO    | 4
#  7 |  RP 2040  | 2
#  0 |           | 1
#    +-----------+

USE_UART = True

HIGH_CURRENT = 750

LED_HIGH_CURRENT = None
LED_LOW_CURRENT = None
RESET_PIN = None
EMERG_PIN = None

if(mcu == "RPi RP2040"):
#-------------------------------------------
    # hier Verbindungen einstellen
    # am RP pico / pico2 / Waveshare 1.28
    DIR_PIN = 19
    BRAKE_PIN = 20
    PWM_PIN = 21
    POWER_PIN = 22
    ACK_PIN = 27
    H_BRIDGE = "LM18200D"

elif(mcu == "XIAO-RP2040"):
    # am XIAO-RP2040
    DIR_PIN = 27
    BRAKE_PIN = 28
    PWM_PIN = 29
    POWER_PIN = 3
    ACK_PIN = 26
    H_BRIDGE = "DRV8871"
    # LEDs zeigen Strom an: 
    LED_HIGH_CURRENT = 7 # rot, medium = rot + gruen
    LED_LOW_CURRENT = 6 # gruen
    RESET_PIN = 2
    EMERG_PIN = 4
    # leuchtet im Gehaeuse, ausmachen
    USER_LED_PINS = [25, 16, 17]
    for i in USER_LED_PINS:
        pin = Pin(i, Pin.OUT)
        pin.on()
    
    
if mcu == "XIAO-RP2040" or mcu == "XIAO-RP2350":
    UART_CH = 0
    TX_PIN = 0
    RX_PIN = 1
elif mcu == "RPi RP2040" or mcu == "RPi RP2350":
    UART_CH = 1
    TX_PIN = 4
    RX_PIN = 5

#-------------------------------------------

def log_print(string="", end=""):
    if __DEBUG__:
        if(end == ""):
            print(string)
        else:
            print(string, end=end)

LOW = const(0)
HIGH = not(LOW)
max_speed = 125

AUTO_SLEEP_TIME = const(15 * 60000)

###########################################

speedsteps = None
loco = None
address = None
use_long_address = False
functions = []
    
op = None
sm = None
uart = None
client_uart = False
client_kbd = False
########################
        

def finish():
    if not timer_current == None:
        timer_current.deinit()
    if not (op == None):
        op.emergency_stop()
        op.power_off()
        op.end()
    if uart != None:
        uart.deinit()
    if pwm_low_current != None:
        pwm_low_current.duty_u16(0)
    if pwm_high_current != None:
        pwm_high_current.duty_u16(0)
    deepsleep()

def get_loco_profile():
    def read_cv(cv):
        cv_val = 0
        for cv_val in range(256):
            repetitions = 3
            while repetitions > 0:
                sm.verify(cv, cv_val)
                if sm.ack():
                    return cv_val
                else:
                    repetitions -= 1
                
        if cv_val == 255 and repetitions == 0:
            sm.end()
            raise(ValueError(f"Lesen von CV {cv} nicht erfolgreich"))
        return cv_val

    def read(cv):
        if directmode == False:
            return read_cv(cv)
        cv_val = 0
        chk_val = -1
        repetitions = sm.REPETITIONS
        while chk_val != cv_val and repetitions >= 0:
            for bit in range(8):
                sm.verify_bit(cv, bit, 1)
                if sm.ack():
                    cv_val |= 1 << bit
            sm.verify(cv, cv_val)
            if sm.ack():
                chk_val = cv_val
            else:
                repetitions -= 1
        if repetitions < 0:
            sm.end()
            raise(ValueError(f"Lesen von CV {cv} nicht erfolgreich"))
        return cv_val

    def test_directmode_support():
        sm.verify_bit(8, 7, 1)
        directmode_support = sm.ack()
        sm.verify_bit(8, 7, 0)
        directmode_support ^= sm.ack()
        return directmode_support
        
        
    sm = SM(H_BRIDGE, DIR_PIN, BRAKE_PIN, PWM_PIN, POWER_PIN, ACK_PIN)
    sm.begin()

    t = time.ticks_ms() + 5 * 6e4  ## 5 Minuten Timeout

    try:
        while t > time.ticks_ms():
            I = sm.get_current()
            if I < -1:
                log_print(f"Keine Lok erkannt ({I:>4} mA)", end="\r")
            else:
                break
     
        log_print(30 * " ")

        timeout = t <= time.ticks_ms()

        if timeout:
            sm.statemachine.end()
            return(None, 0, 0)

        for i in range(100):
            sm.loop()
        
        directmode = test_directmode_support()
        
        cv29 = read(29)
        use_long_address = cv29 & 0x20
        if not cv29 & 0x02:
            speedsteps = 14
        else:
            if cv29 & 0b10:
                if cv29 & 0b1000:
                    speedsteps = 28 # indiv. Tabelle
                else:
                    speedsteps = 128
            else:
                speedsteps = 14
        
        if use_long_address:
            cv17 = read(17)
            cv18 = read(18)
            loco = (cv17 - 192) * 256 + cv18
        else:
            loco = read(1)
        
    except KeyboardInterrupt:
        sm.end()
        raise(KeyboardInterrupt("Benutzer hat abgebrochen"))

    sm.end()
    return (loco, use_long_address, speedsteps)


########################

def drive(direction=1, speed=0):
    op.drive(direction, speed)
 
def halt(direction=1):
    op.drive(direction, 0)

# Abfrage, ob das eingegebne Zeichen eine Ziffer ist
# def is_number(number):
#     return number in ['0', '1', '2', '3', '4', '5', '6', '7', '8', '9']

def pom_input():
    i = 1
    j = 0
    addr = 0
    cv = 0
    val = 0
    while i < len(input_buffer):
        c = input_buffer[i]
        if dccp.is_number(c):
            val = val * 10 + int(c)
        else:
            if c == '*' or c == ',':
                if j == 0:
                    addr = val
                    j += 1
                    val = 0
                elif j == 1:
                    cv = val
                    j += 1
                    val = 0

        i += 1
    return (addr, cv, val)
        
def pom_acc():
    (addr, cv, value) = pom_input()
    log_print(f"PoM Accessory Addr {addr}, CV {cv} = {value}")
    op.pom_accessory(addr, cv, value)
#     pass

def pom_loco():
    (addr, cv, value) = pom_input()
    log_print(f"PoM Loco Addr {addr}, CV {cv} = {value}")
    op.pom_multi(addr, cv, value)
#     pass

def get_fn_as_int(functions):
    f_state = 0
    for f_nr in range(0, 13):
        function_group = op.get_function_group_index(f_nr)
        if(functions[function_group] & (1 << op.get_function_shift(f_nr)) != 0):
            f_state |= (1 << f_nr)
    return f_state

def show_fn():
    for f in range(69):
#         if(uart != None and f < 13):
#             uart.write('1' if op.get_function(f) else '0')
        log_print(f"{'F' if op.get_function(f) else 'f'}{f:<4}", end="")
        if((f % 12) == 0):
            log_print()
    log_print()
    
def show_accessories():
    if (len(op.accessories) == 0):
        log_print("Kein Zubehör")
        return
    for i in range(len(op.accessories)):
        acc = op.accessories[i]
        log_print(f"{acc.address:<5}", end=", ")
        if(acc.signal):
            log_print(f"{int2bin(acc.aspect):^9}", end=", ")
        else:
            log_print(f"D={acc.D} R={acc.R}", end=", ")
        log_print(acc.name)

def show_locos():
    if (len(op.locos) == 0):
        log_print("Keine Triebfahrzeuge")
        return
    for i in range(len(op.locos)):
        loco = op.locos[i]
        log_print(f"Addr: {loco.address:>5} | ", end="")
        log_print(f"{loco.speedsteps:3} Fahrstufen | ", end="")
        log_print(f"FS {loco.current_speed["FS"]:>3} {'vorw.' if loco.current_speed["Dir"]==1 else 'rückw.':<6} | ", end="")
        log_print(loco.name)
        show_fn()
        log_print()
        
def get_loco():
    global loco, use_long_address, speedsteps
    clear_input_buffer()
    op.end()
    loco, use_long_address, speedsteps = get_loco_profile()
    set_loco_data(loco)
    log_print(f"Lok {loco}, {'Lange' if use_long_address else 'Kurze'} Adresse, {speedsteps} Fahrstufen")
    op.begin()
    op.ctrl_loco(loco, use_long_address, speedsteps)
    log_print(f"Lok {loco} bereit")
    return (loco, use_long_address, speedsteps)
    
def get_loco_data(address):
    log_print(address)
    index = op.search(address)
    log_print(index)
    loco = None
    if index != None:
        loco = {
            "address" : op.locos[index].address,
            "dir" : op.locos[index].current_speed["Dir"],
            "speed" : op.locos[index].current_speed["FS"],
            "speedsteps" : op.locos[index].speedsteps,
            "functions" : get_fn_as_int(op.locos[index].functions),
            "name" : op.locos[index].name
        }
    return loco


def usage():
    log_print("""
Benutzung:
- Per Tastatur: Instruktion eingeben und mit Enter abschicken
- Groß- und Kleinschreibung ist irrelevant
- Kommandos für die gleiche Adresse dürfen durch Komma verkettet werden,
- Kommando für jede Adresse wird mit # abgeschlossen

Instruktion        | Führt aus
-------------------+---------------------------------------------------------------------
?                  | Diese Hilfe
E[MERG]            | Nothalt alle Fahrzeuge
                   |
L?                 | Lok suchen (via Servicemode, es darf nur ein Decoder ansprechbar sein)
L{nnn, la, fs, n}  | Lok bedienen: aaa = Dekoder-Adresse (sh.unten)
                   |               la  = lange Adresse (0 / 1)
                   |               fs  = Fahrstufen (28 / 128)
                   |               n   = Name
                   |
V{fff}             | Lok vorwärts Fahrstufe {fff}
R{fff}             | Lok rückwärts Fahrstufe {fff}
                   |
G{aaa}             | Daten der Lok {aaa} ausgeben (Fahrstufe, Richtung, Funktionen)
D                  | Liste der Loks ausgeben
D{aaa}             | Lok aus der Liste entfernen (keine weiteren Pakete generieren), {aaa} = Adresse
                   |
F{nn}              | Funktion {nn = 0..12} ein/ausschalten
F                  | Welche Funktionen sind eingeschaltet?
                   |
W{a},{d}           | Weiche {a} Richtung {d} mit d = 0: geradeaus, d != 0 abzweigend
                   |
S{a},{d}           | Signal {a} Bild (Aspect) {d}
                   |
QUIT               | Beenden, alles ausschalten
RESET              | Layout in Grundstellung versetzen
-------------------+---------------------------------------------------------------------
                   |
PoM:               |
P{aaa, cv, value}  | für Multifunktionsdekoder, aaa = Adresse, cv = CV, value = Wert
A{aaa, cv, value}  | für Accessory-Decoder, aaa = Adresse, cv = CV, value = Wert
                   |
-------------------+--------------------------------------------------------------------
"""
)

def set_loco_data(addr=3, use_long_address=0, speedsteps=28):
    op.ctrl_loco(addr, use_long_address, speedsteps)

def send_loco_data(addr=3, use_long_address=0, speedsteps=28):
    time.sleep(0.5)
    log_print(f">>>{addr},{use_long_address},{speedsteps}<<<")
    if uart != None:
        uart.write(f">>>{addr},{use_long_address},{speedsteps}<<<")
        
##---------------------------------

wait_state = 0

def wait(millis=None):
    global wait_state
    val = 0
    i = 0
    if(millis != None):
        while i < len(millis) and val != None:
            c = millis[i]
            if dccp.is_number(c):
                val = val * 10 + int(c)
            else:
                val = None
            i += 1
        if val != None:
            wait_state = time.ticks_ms() + val
#     log_print(val, wait_state, time.ticks_ms())
    op.loop()
    if wait_state <= time.ticks_ms():
        return False
    else:
        return True

#############################PARSER###########################
def process_input_buffer(buffer):
    cmd = None
    value = 0
    if len(buffer) > 0:
        b = buffer.lower()
        if(b == 'run'):
            return True   # Kein Kommando
        
        elif (b == "reset"):
            finish()
            reset() # wie der Name sagt...
            return True
        
        elif(b == "emerg" or b == 'e' or b == "stop"): # Nothalt alles
            log_print(f"Nothalt")
            op.emergency_stop()
            return True

        elif b == 'quit' or b == 'q': # Ende
                return False
        
        elif b == '?':
            usage()
            return True
            

        log_print(buffer)
        cmd = b[:1]
        if  cmd == 'p':
            pom_loco()
        elif cmd == 'a':
            pom_acc()
        elif not (cmd == 'l' or cmd == 'w' or cmd == 'g' or cmd == 's') and op.active_loco == None:
            log_print("no loco")
            return True
                
        log_print(f"Cmd={cmd}")
        
        if cmd in ['l', 'd', 'v', 'r', 'f',  'w', 'h', 's']:
            (addr, use_long_address, fs, text) = dccp.parse(buffer)
            if cmd == 'l':
                if len(buffer) == 2 and buffer[1] == '?':
                    log_print(f"Scan angefordert: {buffer}")
                    addr, use_long_address, speedsteps = get_loco()
                    if(addr):
                        send_loco_data(addr, use_long_address, speedsteps)
                else:
                    cnt = len(op.locos)
                    if(use_long_address == None):
                        op.ctrl_loco(addr)
                    elif (fs == None):
                        op.ctrl_loco(addr, use_long_address)
                    elif (text == None):
                        op.ctrl_loco(addr, use_long_address, fs)
                    else:                        
                        op.ctrl_loco(addr, use_long_address, fs, text)
                        
                    if cnt != len(op.locos):  # wurde eine hinzugefügt
                        log_print(f"aktive Lok: '{text}' [{addr}], { 2 if use_long_address else 1}-Byte-Adresse, {fs} Fahrstufen")
                    else:
                        log_print(f"Lok {op.active_loco.name} [{op.active_loco.address}] (re-)aktiviert")

            elif cmd == 'd':
                if len(buffer) == 1:
                    show_locos()
                else:
                    if op.active_loco.address == addr:
                        log_print(f"Aktive Lok kann nicht aus Liste entfernt werden")
                    else:
                        for l in op.locos:
                            if l.address == addr:
                                op.locos.remove(l)
                                log_print(f"Keine Pakete mehr an Lok {value} senden")

            elif cmd == 'f':
                if len(buffer) == 1:
                    show_fn()
                else:
                    fn = op.get_function(addr)
                    log_print(f"Funktion {addr} {'aus' if fn else 'ein'}")
                    if fn:
                        op.function_off(addr)
                    else:
                        op.function_on(addr)
                    
            elif cmd == 'w':
                if len(buffer) == 1:
                    show_accessories()
                else:
                    r = use_long_address  # Weiche schalten
                    if(r == None):
                        i = op.search_accessory(addr)
                        log_print(i)
                        if i != None:
                            r = 0 if op.accessories[i].R == 1 else 1
                        else:
                            r = 0
                    log_print(f"Weiche {addr} {'gerade' if r == 0 else 'abzweigend'}")
                    op.ctrl_accessory_basic(addr, 1, r)

            elif cmd == 's':
                if len(buffer) == 1:
                    show_accessories()
                else:
                    aspect = use_long_address  # Weiche schalten
                    if(aspect == None):
                        i = op.search_accessory(addr)
                        log_print(i)
                        if i != None:
                            aspect = op.accessories[i].aspect
                        else:
                            aspect = 0
                    log_print(f"Signal {addr} Aspect {aspect}")
                    op.ctrl_accessory_extended(addr, aspect)

            elif cmd == 'h':
                halt()
                log_print(f"Lok {op.active_loco.address} <<- HALT")
                
            elif cmd == 'v' or cmd == 'r':  # Richtung vor/rueckwaerts, Fahrstufe
                if(addr == None):
                    speed = 0
                else:
                    speed = addr
                    
                if cmd == 'v':
                    cmd = 1
                else:
                    cmd = 0
                if(op.direction() != cmd):
                    halt()
                    log_print("HALT")
                    time.sleep(.5)
                log_print(f"Lok {op.active_loco.address} <<- Fahrstufe {speed if speed <= max_speed else max_speed}")
                op.drive(cmd, speed if speed <= max_speed else max_speed)
                
        else:
            cmd = None
        return True
            
def clear_input_buffer():
    global input_buffer
    input_buffer = ""

def uart_begin(ch, baud, tx, rx):
    uart = UART(ch, baudrate=baud, tx=Pin(tx), rx=Pin(rx))
    return uart

# def uart_read():
#     cnt = uart.any()
#     if( __DEBUG__ > 2 ):
#         log_print(f"{cnt} Zeichen in UART", end="\r")
#     if cnt > 0:
#         s = uart.read(cnt)
#         log_print(f"uart.read({cnt}) = {s.decode('utf-8')}")
#         return s.decode('utf-8')
    
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

# Funktion: Eingabe lesen
def poll_cmd():
    if uart != None:
        return uart_read()
    else:
        return "<<<" + kbd.prompt("?") + ">>>"
    return None


def eventloop():
    global input_buffer, auto_sleep_timer
    answer = True
    input_buffer = poll_cmd()
    if input_buffer != None:
        auto_sleep_timer = time.ticks_ms() +  AUTO_SLEEP_TIME
        if(input_buffer[:3] == "<<<" and input_buffer[-3:len(input_buffer)] == ">>>"):
            log_print(input_buffer)
            for buffer in input_buffer[3:-3].split('#'):
                if(buffer > ""):
                    answer = process_input_buffer(buffer)
                    if (answer != True):
                        return answer
        clear_input_buffer()
    return answer


def show_current(t):
    i = op.get_current()
    duty_high_current = m.map(i, 0, HIGH_CURRENT, 0, 4098)
    duty_low_current = 4098 - duty_high_current
    pwm_low_current.duty_u16(duty_low_current)
    pwm_high_current.duty_u16(duty_high_current)
    if(__DEBUG__ > 1):
        log_print(f"DCC-Strom: {i:>4} mA, duty_high={pwm_high_current.duty_u16()}, duty_low={pwm_low_current.duty_u16()}", end="\r")
        
def wait_t(milliseconds=None):
    if(milliseconds != None):
        start = time.ticks_ms()
        stop = start + milliseconds
        while(time.ticks_ms() <= stop):
            op.loop()
            
    
def startup_sequence():
    time.sleep(2)
#     process_input_buffer("W103,1")
#     for i in range(3):
#         process_input_buffer("W102,1")
#         wait_t(300)
#     process_input_buffer("W101,1")
#     wait_t(300)
#     process_input_buffer("W101,0")
#     wait_t(300)
#     process_input_buffer("W103,0")
#     process_input_buffer("W2,1")
#     wait_t(300)
#     process_input_buffer("W2,0")
#     wait_t(300)
#     process_input_buffer("W1,0")
    
##############################################################

if EMERG_PIN != None:
    emerg_pin = Pin(EMERG_PIN, Pin.IN, Pin.PULL_UP)

if RESET_PIN != None:
    reset_pin = Pin(RESET_PIN, Pin.IN, Pin.PULL_UP)
    
if LED_LOW_CURRENT != None:
    pwm_low_current = PWM(Pin(LED_LOW_CURRENT), freq=50, duty_u16=0)
if LED_HIGH_CURRENT != None:
    pwm_high_current = PWM(Pin(LED_HIGH_CURRENT), freq=50, duty_u16=0)

m = MAP

dccp = DCCP()
# Weiche
turnout = 1
# Lok
loco = None
max_speed = 126

# Tastatur-Eingabe
# kbd = prompt.PROMPT()
# ESP-Now-Eingabe
uart = None
if USE_UART:
    uart = uart_begin(UART_CH, 9600, TX_PIN, RX_PIN)
else:
    kbd = PROMPT()
    
input_buffer = ""
    
time.sleep(.3)
usage()
auto_sleep_timer = time.ticks_ms() + AUTO_SLEEP_TIME
emergency = False
try:
    t = time.ticks_ms() + 20000
    timer_current = Timer()

#     1 Minute auf eine Lok warten, wenn "auto_detection" aktiv
    if(AUTO_DETECTION):
        while t > time.ticks_ms():
            I = op.get_current()
            if I < -1:
                log_print(f"Keine Lok erkannt ({I:>4} mA)", end="\r")
            else:
                log_print("\nBeginne")
                break
        addr, use_long_address, speedsteps = get_loco()
        if(addr):
            send_loco_data(addr, use_long_address, speedsteps)
            
    startup_sequence()
    log_print(10 * "-")
    timer_current.init(mode=Timer.PERIODIC, freq=1, period=333, callback=show_current)
    op = OP(H_BRIDGE, DIR_PIN, BRAKE_PIN, PWM_PIN, POWER_PIN, ACK_PIN)
    op.begin()
    current_A = op.get_current()

    while True:
        
        if time.ticks_ms() >= auto_sleep_timer:
            finish()
            machine.deepsleep()
            log_print("Erwache...")
            reset()
            
        if RESET_PIN != None:
            if(reset_pin.value() == LOW):
                start = time.ticks_ms() + 2000
                while (reset_pin.value() == LOW):
                    if(time.ticks_ms() >= start):
                        finish()
                        reset()

        if EMERG_PIN != None:
            if __DEBUG__ > 2:
                log_print(f"{time.ticks_ms() / 1000.0:12} Nothalt-Pin ist {'HIGH' if emerg_pin.value() else 'LOW'}", end="\r")
            if (emerg_pin.value() == LOW):
                if(not emergency):
                    op.emergency_stop()
                    log_print("Red alert")
                    emergency = True
                    timer_current.deinit()
                    pwm_low_current.duty_u16(0)
                    pwm_high_current.duty_u16(4096)
                else:
                    emergency = False
                    timer_current.init(mode=Timer.PERIODIC, freq=1, period=333, callback=show_current)
                    op.begin()
                    pwm_low_current.duty_u16(4096)
                    pwm_high_current.duty_u16(4096)
                while (emerg_pin.value() == LOW):
                    pass

            
        if rp2.bootsel_button():
            finish()
            raise(RuntimeError("BootSel - Abbruch"))
        if(not emergency):
            op.loop()
        if eventloop() == False:
            finish()
        idle()

        
except KeyboardInterrupt:
    finish()
    raise(KeyboardInterrupt("Benutzer hat abgebrochen"))

finish()

