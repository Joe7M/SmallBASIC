color 15,0
cls

pen on

while(INKEY != "q")
  at 1,1
  color 0,15
  print "Mouse test - Press q to exit "
  color 15,0
  print "-----------------------------"
  print "| New event   | PEN(0)  | "; format("###", Pen(0))
  print "| Last down x | PEN(1)  | "; format("###", Pen(1))
  print "| Last down y | PEN(2)  | "; format("###", Pen(2))
  print "| Left down   | PEN(3)  | "; format("###", Pen(3))
  print "| Left down x | PEN(4)  | "; format("###", Pen(4))
  print "| Left down y | PEN(5)  | "; format("###", Pen(5))
  print "| x           | PEN(10) | "; format("###", Pen(10))
  print "| y           | PEN(11) | "; format("###", Pen(11))
  print "| Left        | PEN(12) | "; format("###", Pen(12))
  print "| Right       | PEN(13) | "; format("###", Pen(13))
  print "| Middle      | PEN(14) | "; format("###", Pen(14))
  
  print
  
  delay(10)
  showpage
wend

pen off