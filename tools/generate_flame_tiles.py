"""Generate a continuous indexed flame ribbon and tapered tip in eight directions."""
from pathlib import Path
import math
p=Path(__file__).resolve().parents[1];words=[]
for angle in range(8):
 co=math.cos(angle*math.pi/4);si=math.sin(angle*math.pi/4)
 for tip in range(2):
  for frame in range(4):
   im=[[0]*32 for _ in range(32)]
   for y in range(32):
    for x in range(32):
     xx=(x-15.5)*co+(y-15.5)*si+15.5;yy=-(x-15.5)*si+(y-15.5)*co
     if not 0<=xx<32:continue
     wave=math.sin((xx+frame*6)*.45)+math.sin((xx-frame*4)*.8)*.6
     width=(5.8+wave)*(max(0,(32-xx)/28) if tip else 1)
     d=abs(yy)
     if d>width:continue
     color=1 if d>width-1 else 2 if d>width*.62 else 3 if d>width*.13 else 4
     if d<.65 and ((int(xx)+frame*7)%23)<3:color=5
     im[y][x]=color
   for ty in range(0,32,8):
    for tx in range(0,32,8):
     for y in range(8):words.append(sum(im[ty+y][tx+x]<<(4*x) for x in range(8)))
lines=['// Code-authored mouth jet: four flowing frames, ribbon/tip, eight directions.','static const u32 sRtFlameTiles[] = {']
lines+=[' '+','.join('0x%08X'%n for n in words[i:i+8])+',' for i in range(0,len(words),8)];lines+=['};','static const u16 sRtFlamePalette[16] = {0,RGB(22,3,1),RGB(31,11,1),RGB(31,24,2),RGB(31,30,15),RGB(31,31,28)};']
(p/'src/data/realtime_flame_graphics.h').write_text('\n'.join(lines)+'\n',encoding='utf-8',newline='\n')
