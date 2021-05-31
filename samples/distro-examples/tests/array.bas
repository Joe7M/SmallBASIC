#!/usr/bin/sbasic

? cat(1);"TEST:";cat(0);" Arrays, unound, lbound"

dim m(1 to 3, -1 to 1)

m(1, -1) = 1
m(2,  0) = 2
m(3,  1) = 3
if m(1,-1)<>1 then ? "ERR 1"
if m(2,0)<>2 then ? "ERR 2"
if m(3,1)<>3 then ? "ERR 3"

for i=1 to 3
	for j=-1 to 1
		m(i,j)=(i*10)+j
	next
next
for i=1 to 3
	for j=-1 to 1
		if m(i,j)<>(i*10)+j then
			? "ERROR (";i;",";j;")"
		fi
	next
next

if lbound(m)<>1 then ?"LBOUND() ERROR"
if ubound(m)<>3 then ?"UBOUND() ERROR"
if lbound(m,2)<>-1 then ?"LBOUND() ERROR"
if ubound(m,2)<>1 then ?"UBOUND() ERROR"

if (isarray(m) == false) then
  throw "m is not an array"
end if

m2 = array("[1,2,3,\"other\"]")
if (isarray(m2) == false) then
  throw "m2 is not an array"
end if

if (isnumber(m2(0)) == false) then
  throw "m2(0) is not an number"
end if

if (isstring(m2(3)) == false) then
  throw "m2(3) is not an string"
end if

m3 = array("{\"cat\":{\"name\":\"lots\"},\"other\":\"thing\",\"zz\":\"thing\",\"zz\":\"memleak\"}")
if (ismap(m3) == false) then
  throw "m3 is not an map"
end if

m4 =  m3

if m4.cat.name <> "lots" then
  ? m3
  ? m4
  throw "ref/map error"
end if

print "array: " + m4

dim sim
sim << 100
sim(0) --
sim(0) ++
sim(0) /= 2
if (sim(0) <> 50) then
  throw "dim sim not tasty"
fi

rem ==6866== Source and destination overlap in memcpy(0xfc3f090, 0xfc3f096, 13)
rem ==6866==    at 0x4C32513: memcpy@@GLIBC_2.14 (in /usr/lib/valgrind/vgpreload_memcheck-amd64-linux.so)
rem ==6866==    by 0x48D4A3: memcpy (string3.h:53)
rem ==6866==    by 0x48D4A3: comp_text_line_let (scan.c:1874)

dim dots(1)
dots(0).y = 100
dots(0).dy = 1
dots(0).y += dots(0).dy
if (dots(0).y != 101) then
   throw "not 101 !!!"
endif

arr1 = [ 1 , 2 , 3 ; 4 , 5 , 6 ; 7 , 8 , 9       ]
arr2 = array("   [1,2,3;4,5,6;7,8,9]            ")
arr3 = array("[1,2,3    ;4,5,6;           7,8,9]")
if (arr1 != arr2 || arr2 != arr3) then
  throw "array err"
endif

const wormHoles = [[4,  4],  [4, 20], [20,  20], [20, 4], [12,12] ]
if (str(wormHoles) != "[[4,4],[4,20],[20,20],[20,4],[12,12]]") then
  throw wormHoles
endif
x = 4
y = 4
if not([x,y] in wormHoles) then
  ? "fail"
endif

a=[;;,;]
b=[0,0;0,0;0,0;0,0]
c=array("[0,0;0,0;0,0;0,0]")
if a!=b || a!=c then
  ?a
  ?b
  ?c
  ?"1: array initialization failed"
fi

a=[,,,;]
b=[0,0,0,0;0,0,0,0]
c=array("[0,0,0,0;0,0,0,0]")
if a!=b || a!=c then
  ?a
  ?b
  ?"2: array initialization failed"
fi

a=[;;5,;]
b=[0,0;0,0;5,0;0,0]
c=array("[0,0;0,0;5,0;0,0]")
if a!=b || a!=c then
  ?a
  ?b
  ?c
  ?"3: 3rd row moved to 1st (empty)"
fi

a=[0;;5,;]
b=[0,0;0,0;5,0;0,0]
c=array("[0,0;0,0;5,0;0,0]")
if a!=b || a!=c then
  ?a
  ?b
  ?c
  ?"4: 3rd row moved to 2nd (1st empty)"
fi

a=[,2,5,4;;;]
b=[0,2,5,4;0,0,0,0;0,0,0,0;0,0,0,0]
c=array("[0,2,5,4;0,0,0,0;0,0,0,0;0,0,0,0]")
if a!=b || a!=c then
  ?a
  ?b
  ?c
  ?"5: row ignored when 1st item omitted"
fi

a=[0,,,;0;0,,5;]
b=[0,0,0,0;0,0,0,0;0,0,5,0;0,0,0,0]
c=array("[0,0,0,0;0,0,0,0;0,0,5,0;0,0,0,0]")
if a!=b || a!=c then
  ?a
  ?b
  ?c
  ?"6: row ignored when 1st item omitted"
fi

a=[,,,]
b=[0,0,0,0]
c=array("[0,0,0,0]")
if a!=b || a!=c then
  ?a
  ?b
  ?c
  ?"7: empty items in 1-dim array ignored"
fi

a=[0,,5,]
b=[0,0,5,0]
c=array("[0,0,5,0]")
if a!=b || a!=c then
  ?a
  ?b
  ?c
  ?"8: empty items in 1-dim array ignored"
fi

REM 0.12.9 allow array access via [] characters
dim elx.j[2], elj[2]
elj[0] = 111
elj[1] = 222
elx.j[0] = elj[0]
elx.j[1] = elj[1]
if (111 <> elx.j[0] || elx.j[1] != 222) then
  print elx
  throw "err"
endif

rem ------------------------------------
dim z()
dim x(1),y(10),h(1 to 3,0 to 7)
h[1,0]=1
h[3,7]=11
local l
dim l.arr()
if (len(l.arr) != 0) then
  throw "new array field should be empty"
endif

rem ------------------------------------
rem test for MAP IN regression
m = {}
m["blah"] << "this"
m["blah"] << "that"
m["blah"] << "other"
found = 0
for i in m["blah"]
  if i=="this" then found+=1
  if i=="that" then found+=1
  if i=="other" then found+=1
next i
if found != 3 then
  throw "elements not found"
fi

rem  --- 0.12.10
s.v=[[1,99,3],[4,5,6]]
if (s.v[0][1] != 99) then
  throw "illegal access"
endif

grid.map = [[0,0,0,0],[0,0,0,0],[110,0,0,0],[0,0,0,0]]
grid.size = 3
local maxTile = 0
for i = 0 to grid.size
  for j = 0 to grid.size
     v = grid.map[i][j]
     maxTile = max(maxTile, grid.map[i][j])
  next j
next i
if maxTile != 110 then throw "error :" + maxTile

rem ------------------------------------
rem define an empty array with [] notation
aaa=[]
if (!isarray(aaa) or len(aaa) != 0) then
  throw "invalid empty array"
endif

rem ------------------------------------
rem define an array over multiple lines
bbb=[10,
20,30,40,
50,60,
70,
80
]
if (!isarray(bbb) or len(bbb) != 8) then
  throw "invalid multiline array"
endif

const ccc =&
[[0,0,0,0],&
 [1,1,0,0],&
 [1,1,1,0],
 [1,1,1,0]]

ddd=[]
ddd << [1,2, &
        3,4]

rem ------ '([' OR ',[' make code arrays
func fn(a,b)
 return a + b
end
if ([3] != fn([1],[2])) then
  throw "err"
endif

rem ---- array optimisations for queues and stacks
stack=[1,2,3,4]
delete stack, len(stack)-1, 1
if (stack != [1,2,3]) then throw "stack err"
stack=[1,2,3,4,5,6]
delete stack, len(stack)-4, 4
if (stack != [1,2]) then throw "stack err"
queue=[11,12,13,14,15,16]
delete queue, 0, 3
if (queue != [14,15,16]) then throw "queue err"

rem --- handle comments inside JSON block
camera = {
 x:        512, rem position on the map
 y:        800, rem y position on the map
 height:    78, rem height of the camera
 angle:      0, rem direction of the camera
 horizon:  100, rem horizon position (look up and down)
 distance: 800  rem distance of map
}
if (camera.x != 512) then throw "invalid x"
if (camera.height != 78) then throw "invalid height"
if (camera.distance != 800) then throw "invalid distance"

rem --- 6d arrays
dim a6d(0 to 5, 0 to 5, 0 to 5, 0 to 5, 0 to 5, 4 to 5)
a6d(0,1,2,3,4,5)=99
if (a6d(0,1,2,3,4,5) <> 99) then
  throw "a6d error 1"
endif
dim m(1 to 11, 2 to 12, 3 to 13, 4 to 14, 5 to 15, 6 to 16)
for i = 1 to 6
  if (lbound(m,i) != i) then
    throw "lbound error"
  endif
  if (ubound(m,i) != 10+i) then
    throw "ubound error"
  endif
next i
m[1,2,3,4,5,6]=999
if (999 <> m[1,2,3,4,5,6]) then
  throw "e"
endif

REM test that "[0, 0]" in anims[0].framePoses[0, 0].translation is not scanned as a codearray
dim amims(0 to 1)
dim j(0 to 1, 0 to 1)
j[0, 0].translation = [1,2,3]
k.framePoses = j
anims << k
sub xfunc(argx)
  local xx=[1,2,3]
  if xx!=argx then throw "err"
end
z=anims[0].framePoses[0, 0].translation
xfunc(anims[0].framePoses[0, 0].translation)

dim a()
append a, [1,2],[3,4],[5,6]
a << [7,8,9]
if (a[0] != [1,2]) then throw "err1"
if (a[1] != [3,4]) then throw "err2"
if (a[2] != [5,6]) then throw "err3"
if (a[3] != [7,8,9]) then throw "err4"

