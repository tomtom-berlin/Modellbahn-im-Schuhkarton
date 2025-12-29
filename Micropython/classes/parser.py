class DCCPARSER:
    
    @classmethod
    def is_number(cls, char):
      return char in ['1', '2', '3', '4', '5', '6', '7', '8', '9', '0']
      
    @classmethod
    def parse(cls, string):
      s = string.lower()
      a = [None, None, None]
      pos = 0
      i = 0
      for c in s:
#         print(c)
        i += 1    
        if(cls.is_number(c)):
          a[pos] = int(c) if a[pos] == None else a[pos] * 10 + int(c)
        else:
          if(c == ','):
            pos += 1
            if(pos > 2):
              break
              return (a[0], a[1], a[2], string[i:len(string)])
      
      return (a[0], a[1], a[2], string[i:len(string)])

if __name__ == "__main__":
    k = 'L3,1,128,Neue Lok#S4,3#W1,1#w2,0#V40#S4,128'
    dccp = DCCPARSER
    for s in k.split('#'):
      (l, u, fs, n) = dccp.parse(s)
      if(fs != None):
        print(f"op.ctrl_loco({l}, {u}, {fs}, '{n}')")
      elif(u != None):
        typ = s[0].lower()
        print(f"Kommando für {'Weiche' if typ=='w' else 'Signal'}", end=": ")
        if(typ == 'w'):
            print(f"op.ctrl_accessory_basic({l}, {u})")
        elif (typ == 's'):
            print(f"op.ctrl_accessory_extended({l}, {u})")

      else:
        print(f"op.drive({l})")

