import os
def fileseek(path, name):
    d = os.listdir(path)
    for i in range(len(d)):
        if d[i] == name:
            fileread(path + '/' + name)
                      
def pathseek(path):
    old_dir = os.getcwd()
    os.chdir("/")
    d = os.listdir()
    os.chdir(old_dir)
    for i in range(len(d)):
        if d[i] == path:
            return True
    return False

def fileread(name):
    f = open(name, 'rt')
    f.seek(0)
    r = f.readline()
    while len(r) > 0:
        print(f"{f.tell()} [{len(r)} Zeichen]: '{r[:-1]}'")
        r = f.readline()
    f.close()
    
def filewrite(name, data=None):
    if not data == None:
        f = open(name, 'wt')
        f.seek(0)
        r = f.write(data + "\n");
        f.close()
