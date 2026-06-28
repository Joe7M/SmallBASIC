print chr(27) + "[?25l"  ' Hide cursor

color 15,0
cls
showpage()

print "Standard Colors"
for c = 0 to 15
  color 15,c
  print format("####",c);
next
print "\n"

color 15,0
print "Extended Colors"
for c = 16 to 231
  color 15, c
  print format("####",c);
next
print "\n"

color 15,0
print "Grayscale Colors"
for c = 232 to 255
  color 15, c
  print format("####",c);
next
print "\n"

color 15,0
c = 15
print "RGB colors to 8 bit"
for r = 0 to 255 step 255/5
  for g = 0 to 255 step 255/5
    for b = 0 to 255 step 255/5
    color 15, rgb(r,g,b)
    c++
    print format("####",c);
    next
  next
next
print "\n"


color 15,0
c = 231
print "RGB gray to 8 bit"
for g = 1 to 255 step 255/24
  color 15, rgb(g,g,g)
  c++
  print format("####",c);
next  

print "\n\n"


'color 15,0
'color rgb(50,100,200),0
'color rgb(255,255,255),0
'color rgb(1,1,1)

print chr(27) + "[?25h"  ' show cursor

'########################################


