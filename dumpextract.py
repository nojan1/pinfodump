#/usr/bin/python

import sys, os
from struct import *

def read(f, l):
    tmp = f.read(l)
    if not tmp:
        sys.exit(0)

    return tmp

if len(sys.argv) < 3:
    print("Usage: dumpextract.py <DUMP FILE> <OUTPUT FOLDER>")
    sys.exit(1)

if not os.path.exists(sys.argv[2]):
    os.mkdir(sys.argv[2])

segments = ["code", "data", "heap"]

dump = open(sys.argv[1], "rb")
pinfo = open(os.path.join(sys.argv[2], "pinfo.txt"), "w")
pinfo.write("PID\tCOMMAND\tUID\tPARENT\tSTATE\n")

data = b""
while 1:
    data += read(dump, 1)
    if len(data) > 6 and data[-1] == 255:
        data = data[0:-1]
        #for i,d in enumerate(data):
         #   if d > 127:
          #      data = data[0:i] + b"\x3F" + data[i+1:]

        #print(data)
        row = data.decode("ascii")
        pinfo.write(row.replace(" ","\t") + "\n")

        pid = row.split(" ")[0].strip().replace(u"\x00","")
        
        for i in range(3):
            if len(sys.argv) >= 5:
                format = sys.argv[3]
                data = read(dump, int(sys.argv[4]))
            else:
                format = "<Q"
                data = read(dump, 8)

            length = unpack(format, data)[0]
            #print(length, data)

            tmp = read(dump, 1)
            if tmp != b'\xff':
                print("ERROR: Data out of sync, expected 0xff got %s" % hex(tmp[0]))
                sys.exit(1)
            
            if length == 0:
                print("PID:%s ignoring segment %s, no data" % (pid, segments[i]))
            else:
                out = open(os.path.join(sys.argv[2], segments[i]+ "-pid:" +pid+".dump"), "wb")
                out.write(read(dump,length-1))
                out.close()

            data = b""
        
