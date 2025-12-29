# Servicemode-Test
# Version 0.6ß 2025-10-13
#
from classes.servicemode import SERVICEMODE as SM
from classes.manufacturers import MANUFACTURER as MAN
from classes.prompt import PROMPT
from classes.parser import DCCPARSER as DCCP
import time
from micropython import const

# mcu = "XIAO-RP2040"
# mcu = "XIAO-RP2350"
mcu = "RPi RP2040"
# mcu = "RPi RP2350"

USE_UART = False
RECEIVER_I2C_ADDRESS = const(0x79)

if(mcu == "RPi RP2040"):
#-------------------------------------------
    # hier Verbindungen einstellen
    # am RP pico / pico2 / Waveshare 1.28
    DIR_PIN = 19
    BRAKE_PIN = 20
    PWM_PIN = 21
    POWER_PIN = 22
    ACK_PIN = 27
    H_BRIDGE = "LMD18200T"

elif(mcu == "XIAO-RP2040"):
    # am XIAO-RP2040
    DIR_PIN = 27
    BRAKE_PIN = 28
    PWM_PIN = 29
    POWER_PIN = 3
    ACK_PIN = 26
    H_BRIDGE = "DRV8871"
    
#-------------------------------------------


__DEBUG__ = const(0)
# __DEBUG__ = const(1)

directmode_support = False

set_address = False
factory_reset = False

acc_address = None
loc_address = None

def command(cv, bit):
      sm.verify_bit(cv, bit, 1)
        

def evaluate(bit, val):
    if sm.ack():
        val |= 1 << bit
    return val

def loco_on_rail():
    I = sm.get_current()
    if I < -4:
        print(f"Keine Lok erkannt ({I:>3} mA)", end="\r")
        return False
    print(30 * " ")
    return True


def read_cv(cv, required=False):
    cv_val = 0
    for cv_val in range(256):
        print(".", end="")
        repetitions = sm.REPETITIONS
        while repetitions > 0:
            if (__DEBUG__):
                print(f"Durchlauf {sm.REPETITIONS - repetitions}, CV {cv} == {bin(cv_val):010}?", end=" ")
            sm.verify(cv, cv_val)
            if(__DEBUG__):
                print(sm.get_current(), "mA")
            if sm.ack():
                return cv_val
            else:
                repetitions -= 1
    print()       
    if cv_val == 255 and repetitions == 0:
        if(required == True):
            sm.end()
            raise(ValueError("Fehler beim Lesen, Hinweise in der Anleitung beachten!"))
        else:
            print(f"Lesen von CV {cv} nicht erfolgreich")
            return None
    return cv_val

def read(cv, required=False, nur_schreiben=False):
    if nur_schreiben:
        return 0
    if directmode_support == False:
        return read_cv(cv, required)
    cv_val = 0
    chk_val = -1
    repetitions = sm.REPETITIONS
    while chk_val != cv_val and repetitions >= 0:
        if (__DEBUG__):
            print(f"Durchlauf {sm.REPETITIONS - repetitions}, CV {cv} == {bin(cv_val):010}?", end=" ")
        for bit in range(8):
            sm.verify_bit(cv, bit, 1)
            if sm.ack():
                if(__DEBUG__):
                    print(sm.get_current(), "mA")
                cv_val |= 1 << bit
            if(__DEBUG__):
                print(cv_val)
        sm.verify(cv, cv_val)
        if sm.ack():
            chk_val = cv_val
        else:
            repetitions -= 1
    if repetitions < 0:
        if(required == True):
            sm.end()
            raise(ValueError("Fehler beim Lesen, Hinweise in der Anleitung beachten!"))
        else:
            print(f"Lesen von CV {cv} nicht erfolgreich")
            return None
    return cv_val


def write(cv, val, nur_schreiben=False):
    if(cv == 8):
        sm.write(cv, val)
        sm.loop()
        return True
    repetitions = 0
    if nur_schreiben:
        while repetitions >= 0:
            sm.write(cv, val)
            sm.loop
            return True
        
    while read(cv) != val and repetitions >= 0:
        sm.write(cv, val)
        sm.loop()
        if sm.ack():
            continue
        else:
            repetitions -= 1
    if repetitions < 0:
#         sm.end()
        print(f"Schreiben von CV {cv} nicht erfolgreich")
        return False
        
    print(f"CV {cv} geschrieben: {val}")
    return True

def describe_cv29(cv29_val):
    bit_meaning = [
        ["Fahrtrichtung", ["normal","umgekehrt"]],  # Bit 0
        ["Fahrstufen", ["14","28/128"]],  # Bit 1
        ["Stromart",["Digital","Stromart wandeln lt. CV #12"]],  # Bit 2
        ["Bidirectionale Kommunikation",["Aus","Ein"]],  # Bit 3
        ["Geschwindigkeitstabelle",["Dreipunkt (CV #2/ #5/ #6)",""]],  # Bit 4
        ["Adressmodus",["1 Byte","2 Byte"]],  # Bit 5
        ["Reserviert",["-","-"]],  # Bit 6
        ["Dekoderart",["Multifunktionsdekoder","Zubehördekoder (Adressierung lt. CV #541)"]]  # Bit 7
    ]
    print(f"CV29 = {bin(cv29_val)}")
    
    for bit in range(8):
        i = (cv29_val >> bit) & 0b1
        print(f"{bit_meaning[bit][0]:<31}: {bit_meaning[bit][1][i]}")
    print()
    
# Bit 4 Speed Table "0" = speed table set by configuration variables #2,#5,
# and #6, "1" = Speed Table set by configuration variables
# #66-#95
# Bit 5 DCC Addressing "0" = one byte addressing, "1" = two byte addressing
# (also known as extended addressing), See S 9.2.1 for
# more information.
# Bit 6 Reserved for future
# use
# -
# Bit 7 Accessory Decoder "0" = Multifunction Decoder, "1" = Accessory Decoder
# (see CV #541 for a description of assignments for bits 0-
# 6)
# Note: If the decoder does not support a featu

def test_directmode_support():
    sm.verify_bit(8, 7, 1)
    sm.loop()
    directmode_support = sm.ack()
    sm.verify_bit(8, 7, 0)
    sm.loop()
    directmode_support ^= sm.ack()
    return directmode_support


# Liest eine Gruppe von CVs
def get_cvs(cvs = []):
    cv_array = []
    for cv in cvs:
        if 0 < cv <= 1024:
            cv_array.append((cv, read(cv)))
            if(__DEBUG__):
                print(f"CV {cv_array[len(cv_array) - 1][0]:>3} = {cv_array[len(cv_array) - 1][1]:<3}")
    return cv_array

def auto_list(addr):
    start = time.ticks_ms()
    if not accessory:
    # 66, 95: Zimo-Geschwindigkeitsanpassung vorwärts/rückwärts
        if addr != 3:
            cv_array = get_cvs([1, 2, 3, 4, 5, 6, 8, 9, 17, 18, 19, 27, 29, 33, 34, 35, 36, 37,
                                38, 39, 40, 41, 42, 43, 44, 48, 49, 50, 51, 52, 53, 66, 95,
                                97, 116, 117, 118, 119, 120, 121, 122, 123, 124, 141, 186, 188, 189, 190])
        else:
            cv_array = get_cvs([1, 7, 8, 17, 18, 29])
#TODO: Liste variabel setzen, abhängig von Dekoder
                            #, 26, 27, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46])#,
#                                 64, 66,
#                                 112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 125, 125])
    else:
        cv_array = get_cvs([1, 9, 34, 33, 36, 35, 38, 37, 39, 40, 50, 51, 52, 60, 61, 62, 70, 71, 72])
        print(f"Freq: {cv_array[2][1] + cv_array[3][1] * 256} Hz, Min: {cv_array[4][1] + cv_array[5][1] * 256} µs, Max: {cv_array[6][1] + cv_array[7][1] * 256} µs, Schritt: {cv_array[8][1]} µs, Warten: {cv_array[9][1]} ms\n")
    
    if cv_array != None:
        stop = time.ticks_ms()
        print(f"{len(cv_array)} CVs, {stop - start} ms, {(stop-start)/len(cv_array)} ms/CV")
        for i in cv_array:
            print(f"CV{i[0]:<3}", end="  ")
        print()
        for i in cv_array:
            print(f"{i[1]:<7}", end="")
        print()
        
## TODO: schreiben der Daten aktivieren?
        save = (kbd.prompt("Dekoderdaten sichern? (J/N)").upper() == "J")
        if (save):
            dn = "/data/"
            file = open(f"/data/Lok#{addr}_{hersteller.replace(' ', '-')}.json", "wt")
            try:
                trenn = "\t"
                file.write("cvs:[\n")
                for i in cv_array:
                    file.write(f"{trenn}({i[0]:>4}, {i[1]:>3})")
                    trenn = ",\n\t"
                file.write("\n]\n")
            finally:
                file.close()    
    

def calc_accessory_address(address):
    lsb = 1
    msb = 0
    if address > 0 and address < 2045:
        lsb = address % 256
        msb = address // 256
    return (lsb, msb)

def calc_multifunction_address(address):
    lsb = 3
    msb = 0
    if address > 0 and address < 10240:
        lsb = address % 256
        msb = (address // 256) + 192
    return (lsb, msb)

def parse_value(buffer):
    val = 0
    i = 0
    while i < len(buffer) and val != None:
        c = buffer[i]
        if dccp.is_number(c):
            val = val * 10 + int(c)
        else:
            val = None
        i += 1
    return val

def do_factory_reset():
    print("Auf Werkseinstellungen zurücksetzen ...")
    write(8,0)
    write(8,8)
    write(8,33)
    sm.power_off()
    time.sleep_ms(1000)
    sm.power_on()
    time.sleep_ms(1000)


man = MAN
sm = SM(H_BRIDGE, DIR_PIN, BRAKE_PIN, PWM_PIN, POWER_PIN, ACK_PIN)
sm.begin()

kbd = PROMPT()
dccp = DCCP()

cv_array = None

t = time.ticks_ms() + 1 * 6e4  ## 1 Minute Timeout

auto = (kbd.prompt("Automatisch lesen? (J/N)").upper() == "J")
nur_schreiben = (kbd.prompt("Nur Schreiben? (J/N)").upper() == "J")
try:
    while not loco_on_rail() and t > time.ticks_ms():
        sm.loop()
    
    timeout = t <= time.ticks_ms()
    
    
    if timeout:
        sm.end()
        raise(RuntimeError("Timeout"))

    for i in range(100):
        sm.loop()
        
    print("Beginne")
    directmode_support = test_directmode_support()
    print(f"{'Direct mode' if directmode_support else 'CV mode'}")

    if factory_reset == True:
        do_factory_reset()

    if not nur_schreiben:
        cv29 = read(29)
        if cv29 == None:
            cv29 = read(29, True)
            
        accessory = cv29 & 0b10000000
        if accessory and cv29 != 0b11100000:
            write(29, 0b11100000)
            
        describe_cv29(cv29)
        use_long_address = cv29 & 0x20
        accessory = cv29 & 0x80
        
        if use_long_address:
            if accessory:
                msb = read(9) & 0b00000111
                lsb = read(1)
                if msb != None and lsb != None:
                    addr = (msb << 8) + (lsb & 0xff)
            else: 
                msb = read(17)
                lsb = read(18)
                if msb != None and lsb != None:
                    addr = (msb - 192) * 256 + lsb
        else:
            addr = read(1)
        
        print(f"gefunden: {'Zubehör' if accessory else 'Lok'} mit Adresse {addr}", end="") 
        cv8 = read(8)
        if(cv8 != None):
            hersteller = man.get_manufacturer_name(cv8)
            print(f", Dekoder-Hersteller: {hersteller}", end="")
        print()

        if accessory:
            if acc_address != addr and set_address:
                cv1, cv9 = calc_accessory_address(acc_address)
                write(9, cv9)
                write(1, cv1)
        else:
            if loc_address != addr and set_address:
                if(loc_address > 127):
                    cv1 = 3
                    cv18, cv17 = calc_multifunction_address(loc_address)
                    if read(1) != cv1:
                        write(1, cv1)
                    write(17, cv17)
                    write(18, cv18)
                    write(29, cv29 | 0x20)
                else:
                    cv1 = loc_address
                    write(1, cv1)
                    write(29, cv29 & ~0x20)
    if(auto and not nur_schreiben):
        auto_list(addr)
    else:
        strg = ''
        while strg[:1].upper != "Q":
            strg = kbd.prompt("CV")
            if strg[:1].upper() == "Q":
                break
            cv = parse_value(strg)
            if cv == None:
                continue
            print(f"CV {cv}", end=" = ")
            if not nur_schreiben:
                wert = read(cv)
                if(wert == None):
                    continue
                else:
                    print(wert, end="")
            if(cv == 8): # Factory Reset?
                strg = kbd.prompt(", Werkseinstellungen (J/N)")
                if(strg[:1].upper() == "J"):
                    factory_reset = True
                continue
            
            strg = kbd.prompt(", Neuer Wert")
            if(dccp.is_number(strg[:1])):
                wert = parse_value(strg)
                if(wert == None):
                    continue
                else:
                    write(cv, wert, nur_schreiben)
            print()
            
except KeyboardInterrupt:
    raise(KeyboardInterrupt("Benutzer hat abgebrochen"))

sm.end()
