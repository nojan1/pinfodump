import os

class OutputFile(object):
    def __init__(self, path):
        self.f = open(path, 'w')
        self.c = 0

    def beginFile(self):
        self.f.write("""<html><head><title>PinfoDump LOG</title>
        <script type="text/javascript">
          function toggle(name){
        obj = document.getElementById(name);
        if(obj.style.display == "block"){
        obj.style.display = "none";
}else{
        obj.style.display = "block";
}
           }
        </script>
        <style type="text/css">
         td { text-align:center;}
         .vmarow{ background-color:gray; display:none; }
        </style>
        </head>
        <body>
        <table>
        <tr>
         <td> </td>
         <th>PID</th>
         <th>Command</th>
         <th>UID</th>
         <th>Parent</th>
         <th>State</th>
        </tr>""")

    def beginProcess(self, row):
        self.c += 1
        self.f.write("""<tr>
        <td><a href="javascript:toggle('vma%i')"><b>+</b></a></td>
        <td>%s</td>
        <td>%s</td>
        <td>%s</td>
        <td>%s</td>
        <td>%s</td>
        </tr>
        <tr><td colspan="6"> <div id="vma%i" class="vmarow"><table>
        <tr>
         <th>Name</th>
         <th>Address</th>
         <th>Length</th>
         <th>Flags</th>
         <th>In file</th>
        </tr>""" % (self.c,row[0],row[1],row[2],row[3],row[4],self.c))

    def addVMA(self, name, address, length, flags, basepath):
        filepath = os.path.join(basepath, name+str(address)+".dump")

        self.f.write("""<tr>
        <td>%s</td>
        <td>%s</td>
        <td>%s</td>
        <td>%s</td>
        <td>%s</td>
        </tr>""" % (name, hex(address), length, flags, filepath))

    def endProcess(self):
        self.f.write("</table></div></td></tr>")
    
    def endFile(self):
        self.f.write("</body></html>")
        self.f.close()
    
