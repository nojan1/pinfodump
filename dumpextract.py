#/usr/bin/python

import sys, os
from struct import *
from outputfile import *

def read(f, l):
    tmp = f.read(l)
    if not tmp:
        sys.exit(0)

    return tmp

def decodeflags(raw):
    ret = ""
    if raw & 1:
        ret += "r "
    else:
        ret +="- "

    if raw & 2:
        ret += "w "
    else:
        ret += "- "

    if raw & 3:
        ret += "x "
    else:
        ret += "- "

    return ret

def sync(f, msg = None):
     tmp = read(f, 1)
     if tmp != b'\xff':
         offset = f.tell() - 1
         print("ERROR: Data out of sync, expected 0xff got %s at offset %s (%i)" % (hex(tmp[0]), hex(offset), offset))
         if msg:
           print("NOTE:", msg)
  
         sys.exit(1)

if len(sys.argv) < 3:
    print("Usage: dumpextract.py <DUMP PATH> <OUTPUT FILE>")
    sys.exit(1)

dump = open(os.path.join(sys.argv[1], "dump.info"), "rb")
output = OutputFile(sys.argv[2])
output.beginFile()

data = b""
while 1:
    data += read(dump, 1)
    if len(data) > 6 and data[-1] == 255:
        data = data[0:-1]
        #for i,d in enumerate(data):
         #   if d > 127:
          #      data = data[0:i] + b"\x3F" + data[i+1:]

        print(data)
        row = data.decode("ascii").replace("\t", " ")
        output.beginProcess(row.split(" "))

        pid = row.split(" ")[0].strip().replace(u"\x00","")
        
        while(1):
            if len(sys.argv) >= 5:
                format = sys.argv[3]
                intsize = int(sys.argv[4])
            else:
                format = "<Q"
                intsize = 8

            data = read(dump, intsize)
            length = unpack(format, data)[0]
            #print(length, data)

            sync(dump, "After length")

            if length == 0:
                print("PID:%s ignoring mapping, no data" % (pid))
            else:
                data = read(dump, intsize)
                address = unpack(format, data)[0]
                sync(dump, "After address")

                data = read(dump, intsize)
                flags = unpack(format, data)[0]
                sync(dump, "After flags")

                data = read(dump, 50)
                name = unpack("<50s", data)[0]
                name = name.decode("ascii")

                sync(dump, "After name")

                output.addVMA(name, address, length, decodeflags(flags), sys.argv[1])

            #print(hex(dump.tell()))
            data = read(dump, 5)
            
            if data[0:] == b"\x6e\x6f\x6a\x61\x6e":
                data = b""
                output.endProcess()
                break
            else:
                dump.seek(-5, 1)

output.endFile()
