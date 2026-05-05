print chr(27) + "[?25l"  ' Hide cursor

color 15,0
cls

DrawLines()
DrawPoints()
DrawRects()


at 1, YMAX-2: print "Done"
print chr(27) + "[?25h"  ' show cursor

'########################################

sub DrawPoints()
  color 15,4
  pset 12, 1 
  pset 14, 1, 3
end

sub DrawLines()
  color 15, 4
  line 1, 1, 10, 6       ' white line 
  line 1, 3, 10, 8, 2    ' green line

  ' Test for fast horizontal and vertical lines
  color 6,0
  line 8,2,10,2
  line 9,1,9,3
end

sub DrawRects()
  color 15,0
  rect 16,1,26,10
  rect 28,1,38,10, 13 filled
  rect 40,1,50,10, rgb(30,48,210)
  rect 52,1,62,10, rgb(30,48,210) filled
end