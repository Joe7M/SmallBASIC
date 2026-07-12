const CORNER_UL = chr(108)
const CORNER_UR = chr(107)
const CORNER_BL = chr(109)
const CORNER_BR = chr(106)
const LINE_H = chr(113)
const LINE_V = chr(120)


print chr(27) + "[?25l"  ' Hide cursor
color 15,0
cls

PrintCharacters()
DrawBox(XMAX*3/5 , YMAX*4/7, 20, 10)

pen on

ii = 0
while(inkey != "q")
  
  DrawRects()
  
  if(pen(0))
    if(pen(12))
      mousex = pen(10)
      mousey = pen(11)
      'cls
      at 1,1
      PrintCharacters()
      DrawBox(mousex - 10, mousey - 5, 20, 10)
    endif
  endif
  
  showpage()
  delay(10)
  
wend

pen off

print chr(27) + "[?25h"  ' show cursor

at  1, YMAX: print "";

'############################
sub LineDrawingMode() 
  print chr(27) + "(0"
end

sub ASCIIDrawingMode()
  print chr(27) + "(B"
end

func rndlohi(lo, hi)
  return (RND * (hi - lo + 1))\1 + lo
end

sub DrawRects()
  rectx = rndlohi(1,XMAX/3)
  recty = rndlohi(12, YMAX-9)
  rectw = rndlohi(5,15)
  recth = rndlohi(2,7)
  c = rndlohi(1,15)
  rect rectx, recty step rectw, recth, c  filled
  color 15,0
end

sub PrintCharacters()
  color 0,15
  print "Line drawing character set: "
  color 15,0
  LineDrawingMode()
  i = 0
  for ii = 33 to 127
    i++
    if(i == 15)
      i = 0
      print
    endif
    color 6
    print format("###", ii);": ";
    color 15
    print chr(ii); " ";
  next
  ASCIIDrawingMode()
  at 1, YMAX-1: print "Press q to exit";
end

sub DrawBox(x,y,w,h)
  rect XMAX/3+16, 11, XMAX-1, YMAX-1, 0 filled
  color 15,0
  LineDrawingMode()
  at x,y
  print CORNER_UL;
  for ii = 1 to w
    print LINE_H;
  next
  print CORNER_UR;
  for ii = 1 to h
    at x, y + ii: print LINE_V;
    at x + w + 1, y + ii: print LINE_V;
  next
  at x, y + h + 1
  print CORNER_BL;
  for ii = 1 to w
    print LINE_H;
  next
  print CORNER_BR;
  
  ASCIIDrawingMode()
  color 0,15 
  at x+w/2-4, y+h-4: print "SmallBASIC"
  color 15,0
end