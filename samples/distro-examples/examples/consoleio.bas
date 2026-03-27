print chr(27) + "[?25l"  ' Hide cursor
color 15,0
cls
delay(1)

' Get terminal size
print "Console size: "; xmax; "x"; ymax

' Set foreground and background color
print "Color test: ";
for i = 0 to 15
  color i, 15-i
  print i;
next
print

' Set cursor position
at(5,5): print("Position 5,5")


' Inkey
' InkeyTest()

Input "Input test: ", A
print A


print chr(27) + "[?25h"  ' show cursor

'########################################

sub InkeyTest()
  const KEY_UP        = chr(27) + chr(9)
  const KEY_DOWN      = chr(27) + chr(10)
  const KEY_LEFT      = chr(27) + chr(4)
  const KEY_RIGHT     = chr(27) + chr(5)
  const KEY_SPACE     = " "
  const KEY_ESC       = chr(27)
  const KEY_RETURN    = chr(13)
  const KEY_PAGE_UP   = chr(27) + Chr(0x01)
  const KEY_PAGE_DOWN = chr(27) + Chr(0x02)
  const KEY_DELETE    = Chr(127)
  const KEY_HOME      = Chr(27) + Chr(0x11)
  const KEY_END       = Chr(27) + Chr(0x12)
  const KEY_BACKSPACE = chr(8)
  const KEY_TAB       = chr(9)
  const KEY_F1        = chr(27) + chr(241)
  const KEY_F2        = chr(27) + chr(242)
  const KEY_F3        = chr(27) + chr(243)
  const KEY_F4        = chr(27) + chr(244)
  const KEY_F5        = chr(27) + chr(245)
  const KEY_F6        = chr(27) + chr(246)
  const KEY_F7        = chr(27) + chr(247)
  const KEY_F8        = chr(27) + chr(248)
  const KEY_F9        = chr(27) + chr(249)
  const KEY_F10       = chr(27) + chr(250)
  const KEY_F11       = chr(27) + chr(251)
  const KEY_F12       = chr(27) + chr(252)
  def CtrlKey(k) = chr(k BAND 0x1F)
  const KEY_CTRL_A    = CtrlKey(asc("a"))

  at(6,1): print "INKEY test. Press q to exit"
  repeat
    k = inkey()
    if len(k)
      select case k
          case KEY_UP:        s = "Up"
          case KEY_DOWN:      s = "Down"
          case KEY_LEFT:      s = "LEFT"
          case KEY_RIGHT:     s = "RIGHT"
          case KEY_SPACE:     s = "Space"
          case KEY_ESC:       s = "Esc"
          case KEY_RETURN:    s = "Return"
          case KEY_HOME:      s = "Home"
          case KEY_PAGE_UP:   s = "Page up"
          case KEY_PAGE_DOWN: s = "Page down"
          case KEY_END:       s = "End"
          case KEY_DELETE:    s = "Delete"
          case KEY_BACKSPACE: s = "Backspace"
          case KEY_TAB:       s = "Tab"
          case KEY_CTRL_A:    s = "CTRL a"
          case KEY_F1:        s = "F1"
          case KEY_F2:        s = "F2"
          case KEY_F3:        s = "F3"
          case KEY_F4:        s = "F4"
          case KEY_F5:        s = "F5"
          case KEY_F6:        s = "F6"
          case KEY_F7:        s = "F7"
          case KEY_F8:        s = "F8"
          case KEY_F9:        s = "F9"
          case KEY_F10:       s = "F10"
          case KEY_F11:       s = "F11"
          case KEY_F12:       s = "F12"
          case "q":           running = 1
          case else:          s = k
      end select
      at(7,1): print s; "                         ";
    endif
    delay(10)
  until(running)
end

