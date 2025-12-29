import sys
import uselect as select
spoll = select.poll()
spoll.register(sys.stdin, select.POLLIN)

class PROMPT():

    # Funktion: Eingabe lesen
    @classmethod
    def prompt(cls, prompt_str="?"):
        print(f"{prompt_str}: ", end="")
        s = None
        c = None
        while (c != "\n" and c != "\r") or s==None:
            c = (sys.stdin.read(1) if spoll.poll(0) else None)
            if c != None and c != "\n" and c != "\r":
                if(s == None):
                    s = ""
                s += c
        return s

if __name__ == "__main__":
    cv = ""
    val = ""
    while (cv[:1]).upper() != "Q":
        cv = PROMPT.prompt('CV?')
        if cv[:1].upper() != "Q":
            cv = int(cv)
            if cv >= 1 and cv <= 1024:
                print(f"---{cv}---")
                val = PROMPT.prompt('Wert?')
                print(f"---{int(val)%256}---")
            cv = ""

    
