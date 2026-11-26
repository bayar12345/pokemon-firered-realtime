"""Build small code-authored 4bpp arena tiles (16-color palette, no external art)."""
from pathlib import Path
p=Path(__file__).resolve().parents[1]
assets=[];names={}
def add(name,im):
 names[name]=len(assets)
 for ty in range(0,len(im),8):
  for tx in range(0,len(im[0]),8):
   assets.append([sum(im[ty+y][tx+x]<<(x*4) for x in range(8)) for y in range(8)])
def canvas(n=16):return [[0]*n for _ in range(n)]
def rect(im,x,y,w,h,c):
 for yy in range(max(0,y),min(len(im),y+h)):
  for xx in range(max(0,x),min(len(im[0]),x+w)):im[yy][xx]=c
def ellipse(im,x,y,rx,ry,c):
 for yy in range(len(im)):
  for xx in range(len(im[0])):
   if ((xx-x)*ry)**2+((yy-y)*rx)**2<=rx*rx*ry*ry:im[yy][xx]=c
def line(im,x,y,x2,y2,c):
 n=max(abs(x2-x),abs(y2-y),1)
 for i in range(n+1):rect(im,round(x+(x2-x)*i/n),round(y+(y2-y)*i/n),1,1,c)
add('CLEAR',canvas(8))
for name,c1,c2,c3 in [('STONE',3,2,4),('SAND',10,9,4),('WATER',11,12,5),('STEEL',4,3,4),('GARDEN',4,3,5),('GRASS',7,6,8),('POISON',2,6,15),('PSYCHIC',2,15,4),('LAVA',9,13,14),('ICE',4,12,5),('CRYPT',4,3,15),('DRAGON',3,11,10),('NEUTRAL',7,6,5)]:
 im=[[c1]*8 for _ in range(8)]
 for y in range(8):
  for x in range(8):
   if (x*7+y*11)%19==0:im[y][x]=c2
 if name in ['STONE','STEEL','GARDEN','CRYPT']:line(im,0,7,7,7,c2);line(im,7,0,7,7,c2);im[0][0]=c3
 if name in ['WATER','ICE']:line(im,1,2,4,2,c3);line(im,4,5,6,5,c2)
 if name=='PSYCHIC':line(im,0,0,7,7,c2)
 if name=='GRASS':im[3][2]=8;im[2][3]=8;im[6][6]=6
 if name=='LAVA':line(im,1,0,3,3,13);line(im,3,3,2,7,14)
 add('FLOOR_'+name,im)
im=[[2]*8 for _ in range(8)];rect(im,0,0,8,2,4);rect(im,0,6,8,2,1);rect(im,0,2,1,4,5);add('WALL',im)
for stage in range(4):
 im=canvas();ellipse(im,8,13,7,2,2)
 if stage:
  ellipse(im,8,8,7,6,1);ellipse(im,8,7,6,5,3);ellipse(im,7,6,4,3,4);line(im,3,11,11,12,2)
  if stage<=2:line(im,8,3,7,8,1);line(im,7,8,10,11,1)
  if stage<=1:rect(im,8,2,8,5,0);line(im,3,7,8,12,1);rect(im,12,8,3,4,2)
 else:rect(im,2,11,4,2,3);rect(im,9,10,5,3,4)
 add('ROCK_'+str(stage),im)
for stage in range(4):
 im=canvas();ellipse(im,8,14,7,1,2);rect(im,6,8,4,7,9);rect(im,7,9,1,5,10)
 if stage:
  ellipse(im,8,6,7,6,1);ellipse(im,8,5,6,5,6);ellipse(im,7,4,5,4,7);ellipse(im,5,3,2,2,8);ellipse(im,10,6,3,3,7)
  if stage<=2:rect(im,10,3,4,3,0);rect(im,3,8,3,2,0);line(im,8,7,7,13,1)
  if stage<=1:rect(im,1,0,5,9,0);rect(im,8,0,8,6,0);ellipse(im,10,7,3,2,6);line(im,5,13,2,15,7)
 else:rect(im,5,12,6,3,9);line(im,6,11,9,11,10)
 add('TREE_'+str(stage),im)
for stage in range(4):
 im=canvas();rect(im,3,13,10,2,2)
 if stage:
  rect(im,4,1,8,13,1);rect(im,5,2,6,11,3);rect(im,6,2,2,10,4);rect(im,3,1,10,2,10)
  if stage<=2:line(im,9,4,6,7,1);line(im,6,7,9,9,1)
  if stage<=1:rect(im,3,0,10,5,0);rect(im,9,5,5,4,0)
 else:rect(im,4,11,8,3,3);rect(im,5,10,6,1,4)
 add('COLUMN_'+str(stage),im)
for state in range(3):
 im=canvas();ellipse(im,8,14,6,1,2);rect(im,5,9,6,5,1);rect(im,6,10,4,3,3);rect(im,7,2,2,9,4);ellipse(im,8,3,3,3,1);ellipse(im,8,3,2,2,12 if state==0 else 14 if state==1 else 5)
 if state==2:line(im,1,4,4,7,14);line(im,4,7,2,10,5);line(im,14,2,11,6,14);line(im,11,6,14,8,5)
 add('ROD_'+str(state),im)
for name,col in [('SPARK',14),('WATER',12),('GRASS',7),('POISON',15),('PSYCHIC',15),('FIRE',13),('SAND',10),('ICE',12),('SHADOW',15),('WIND',5)]:
 for state in range(3):
  im=canvas();border=col if state==0 else 14 if state==1 else 5
  if name in ['FIRE','POISON']:
   rect(im,2,5,12,8,1);rect(im,3,6,10,6,3)
   for x in range(4,13,3):line(im,x,7,x,11,2)
   if state:ellipse(im,8,6,4,5,col);ellipse(im,7,5,2,3,border)
  elif name=='GRASS':
   for y in range(2,15,4):
    for x in range(2,15,4):line(im,x,y+2,x+1,y,8);line(im,x+1,y,x+2,y+2,6)
   for x,y in [(4,4),(11,8),(5,12)]:
    rect(im,x-1,y,3,1,15);rect(im,x,y-1,1,3,13);im[y][x]=14
  elif name in ['PSYCHIC','SHADOW']:
   for y in range(3,14):
    for x in range(3,14):
     if max(abs(x-8),abs(y-8)) in (4,5):im[y][x]=border
   line(im,5,8,11,8,col);line(im,8,5,8,11,col)
  elif name=='WIND':
   for y in (4,8,12):line(im,2,y,12,y,border);line(im,10,y-2,13,y,col)
  else:
   ellipse(im,8,8,7,6,11 if name in ['WATER','ICE'] else 9);ellipse(im,8,7,6,5,col)
   for y in (4,8,11):line(im,4,y,11,y,border)
  add(name+'_'+str(state),im)
assert len(assets)<=256,len(assets)
lines=['// Code-authored arena tiles. Regenerate with tools/generate_arena_tiles.py.','#define RT_ARENA_TILE_BASE 0x300']
lines += ['#define RAT_'+n+' '+str(i) for n,i in names.items()]
lines += ['static const u32 sRtArenaTiles[] = {']
lines += [' '+','.join('0x%08X'%v for v in row)+',' for row in assets];lines+=['};']
colors=[(0,0,0),(3,4,6),(8,10,13),(14,17,18),(22,24,25),(29,30,29),(3,11,5),(7,18,7),(17,25,11),(13,8,4),(25,18,10),(3,9,20),(9,23,28),(29,9,4),(31,27,5),(19,7,24)]
lines+=['static const u16 sRtArenaPalette[16] = {'+','.join(str(r|g<<5|b<<10) for r,g,b in colors)+'};']
(p/'src/data/realtime_arena_graphics.h').write_text('\n'.join(lines)+'\n',newline='\n');print(len(assets),'arena tiles')
