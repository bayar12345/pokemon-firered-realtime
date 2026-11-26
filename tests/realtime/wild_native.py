from pathlib import Path
import ctypes as C, struct, json, collections, argparse
from libretro import Core
parser=argparse.ArgumentParser(description='Disposable production-ROM wild encounter regression; never writes the input battery save.')
parser.add_argument('--layout',type=Path,help='Compiler-exported JSON structure offsets')
parser.add_argument('--repo',type=Path,default=Path(__file__).resolve().parents[2])
parser.add_argument('--symbols',type=Path,required=True)
parser.add_argument('--battery',type=Path,required=True)
parser.add_argument('--core',type=Path,required=True)
parser.add_argument('--output-dir',type=Path,required=True)
args=parser.parse_args();repo=args.repo;out=args.output_dir;out.mkdir(parents=True,exist_ok=True)
s={p[2]:int(p[0],16) for l in args.symbols.read_text().splitlines() if len(p:=l.split())==3}
STATE=2766; ORIGIN=3572; WINDOWS=2623; VISIBLE=3358; COMPACT=3359
if args.layout:
 layout=json.loads(args.layout.read_text());STATE,ORIGIN,WINDOWS,VISIBLE,COMPACT=layout[6:11]
def press(c,key,n=30,wait=30): c.run(n,[key]); c.run(wait)
def callback(c,name,args=()):
 # Wait for the fixture callback to finish. A busy field frame can span more
 # than one emulated video frame; restoring callback2 after one frame drops it.
 scratch=0x203FE00;marker=scratch+124
 values=list(args)+[0]*4
 code=struct.pack('<14H6I',0xB510,0x4806,0x4906,0x4A07,0x4B07,0x4C08,0xF000,0xF804,0x4C07,0x2001,0x6020,0xBD10,0x4720,0x46C0,*values[:4],s[name]|1,marker)
 c.write(scratch,code);c.write(marker,bytes(4));old=c.read(s['gMain']+4,4)
 c.write(s['gMain']+4,struct.pack('<I',scratch|1))
 for _ in range(300):
  c.run(1)
  if c.u32(marker):break
 else:raise AssertionError(('fixture callback did not finish',name))
 if c.u32(s['gMain']+4)==scratch|1:c.write(s['gMain']+4,old)
def boot():
 c=Core(repo/'pokefirered.gba',args.core); b=(args.battery).read_bytes(); C.memmove(c.lib.retro_get_memory_data(0),b,len(b))
 c.run(1500);press(c,3,wait=200);press(c,8,wait=500);press(c,8,wait=600);press(c,0,wait=600)
 groups=json.loads((repo/'data/maps/map_groups.json').read_text()); lookup={n:(gi,ni) for gi,g in enumerate(groups['group_order']) for ni,n in enumerate(groups[g])}
 g,n=lookup['Route1'];c.write(s['sWarpDestination'],struct.pack('<bbbxhh',g,n,-1,6,5));c.write(s['gMain'],bytes(4));callback(c,'WarpIntoMap')
 c.write(s['gMain']+0x438,bytes(1)); c.write(s['gMain']+4,struct.pack('<I',s['CB2_LoadMap']|1));c.run(600)
 w,h,buf,layout,tile,behavior=grid(c)
 candidates=[]
 mw,mh=struct.unpack('<ii',c.read(layout,8))
 for yy in range(10,mh+4):
  for xx in range(10,mw+4):
   if behavior(xx,yy)==2 and not tile(xx,yy)&0xC00:
    near=sum(behavior(xx+dx,yy+dy)==2 and not tile(xx+dx,yy+dy)&0xC00 for dx in range(-2,3) for dy in range(-1,2))
    candidates.append((near,xx,yy))
 _,xx,yy=max(candidates)
 c.write(s['sWarpDestination'],struct.pack('<bbbxhh',g,n,-1,xx-7,yy-7));callback(c,'WarpIntoMap')
 c.write(s['gMain']+0x438,bytes(1));c.write(s['gMain']+4,struct.pack('<I',s['CB2_LoadMap']|1));c.run(600)
 return c
def grid(c):
 w,h,buf=struct.unpack('<iiI',c.read(s['VMap'],12));layout=c.u32(s['gMapHeader']);ts=c.u32(layout+16); ss=c.u32(layout+20)
 attrs=[c.u32(ts+20),c.u32(ss+20)]
 def tile(x,y):return struct.unpack('<H',c.read(buf+2*(y*w+x),2))[0]
 def behavior(x,y):
  t=tile(x,y)&1023; return c.u32(attrs[t>=640]+(t-640 if t>=640 else t)*4)&511
 return w,h,buf,layout,tile,behavior
def actor(c):return s['gObjectEvents']+36*c.read(s['gPlayerAvatar']+5,1)[0]
def begin(c):
 c.write(s['gEnemyParty'],c.read(s['gPlayerParty'],100));c.write(s['gEnemyPartyCount'],b'\1');c.write(s['gBattleTypeFlags'],bytes(4))
 callback(c,'TryStartRealtimeWildBattle');c.run(12)
 return c.u32(s['sRt'])
def wait_state(c,state):
 for _ in range(500):
  p=c.u32(s['sRt'])
  if p and c.read(p+STATE,1)[0]==state:return p
  c.run(1)
 raise AssertionError(('state not reached',state,hex(p),c.read(p+STATE,1) if p else None))
def finish_flee(c):
 c.run(10,[3]);c.run(250)
 print('flee result',c.read(s['gRealtimeBattleActive'],1).hex(),hex(c.u32(s['sRt'])),c.read(c.u32(s['sRt'])+STATE,1).hex() if c.u32(s['sRt']) else '',c.read(s['gBattleOutcome'],1).hex(),hex(c.u32(s['gMain']+4)),flush=True)
 assert not c.read(s['gRealtimeBattleActive'],1)[0] and c.u32(s['sRt'])==0
 assert c.read(s['gBattleOutcome'],1)[0]==4
 before=struct.unpack('<hh',c.read(actor(c)+16,4))
 for key in [5,6,4,7]:
  press(c,key,n=18)
  if struct.unpack('<hh',c.read(actor(c)+16,4))!=before:break
 else:raise AssertionError('field movement remains locked')

c=boot();w,h,buf,layout,tile,behavior=grid(c);a=actor(c);tx,ty=struct.unpack('<hh',c.read(a+16,4))
print('origin',tx,ty,'behavior',behavior(tx,ty),'map size',struct.unpack('<ii',c.read(layout,8)),flush=True)
c.screenshot(out/'route-before.png');c.save_state(out/'wild-base.state')
p=begin(c);print('natural map start',hex(p),flush=True)
assert p
wait_state(c,2);e=c.read(p,1400).find(struct.pack('<I',s['gEnemyParty']));assert e>0
c.screenshot(out/'wild-intro.png');finish_flee(c);print('INTRO escape and field movement PASS',flush=True);c.close()
results=[]
for mode in ['countdown','picker','fight','disconnected','detour','no-grass','boundary']:
 c=Core(repo/'pokefirered.gba',args.core);c.load_state(out/'wild-base.state')
 w,h,buf,layout,tile,behavior=grid(c);a=actor(c);tx,ty=struct.unpack('<hh',c.read(a+16,4))
 original=tile(tx,ty);grass=original&0xF3FF
 if behavior(tx,ty)!=2:
  attrs=c.u32(c.u32(layout+16)+20); grass=next(i for i in range(640) if c.u32(attrs+4*i)&511==2)|(original&0xF000)
 normal=next(i for i in range(640) if c.u32(c.u32(c.u32(layout+16)+20)+4*i)&511==0)|(original&0xF000)
 def put(x,y,value):c.write(buf+2*(y*w+x),struct.pack('<H',value))
 if mode in ['disconnected','detour','no-grass','boundary']:
  # These fixtures isolate terrain connectivity from the roaming Route 1 NPC.
  player=c.read(s['gPlayerAvatar']+5,1)[0]
  for npc in range(16):
   obj=s['gObjectEvents']+npc*36
   if npc!=player and c.u32(obj)&1:callback(c,'RemoveObjectEvent',[obj])
  for y in range(ty-5,ty+6):
   for x in range(tx-7,tx+8):put(x,y,original|0xC00)
  put(tx,ty,grass);put(tx-1,ty,normal);put(tx-2,ty,normal);put(tx-3,ty,grass)
  put(tx+3,ty,grass)
  if mode=='detour':
   put(tx-3,ty,normal)
   for x in range(tx-1,tx+4):put(x,ty-1,normal)
  if mode=='no-grass':put(tx-3,ty,normal)
  if mode=='boundary':
   put(tx-3,ty,normal)
   # Clone the map layout into disposable emulator RAM and shorten its edge.
   scratch=0x203FE00;c.write(scratch,c.read(layout,28));c.write(scratch,struct.pack('<i',tx-7+1));c.write(s['gMapHeader'],struct.pack('<I',scratch))
 p=begin(c)
 if mode in ['no-grass','boundary']:
  print('fallback check',mode,hex(p),flush=True)
  assert p==0 and not c.read(s['gRealtimeBattleActive'],1)[0]
  callback(c,'StartWildBattle');c.run(500)
  assert c.u32(s['gMain']+4)&~1 != s['CB2_Overworld']
  results.append({'case':mode,'classic_fallback':True,'passed':True});c.close();continue
 assert p;wait_state(c,2);e=c.read(p,1400).find(struct.pack('<I',s['gEnemyParty']));assert e>0
 ox,oy,mtx,mty=struct.unpack('<hhhh',c.read(p+ORIGIN,8));ex,ey=struct.unpack('<ii',c.read(p+e+4,8));ex//=256;ey//=256
 et=(mtx+((ex-(ox-8))>>4),mty+((ey+6-(oy-8))>>4))
 if behavior(tx,ty)==2:assert behavior(*et)==2
 if mode=='disconnected':assert et==(tx-3,ty),et
 if mode=='detour':assert et==(tx+3,ty),et
 if mode=='picker':c.run(10,[2]);c.run(1);assert c.read(p+STATE,1)[0]==5
 elif mode=='countdown':wait_state(c,3)
 elif mode in ['fight','detour','disconnected']:
  wait_state(c,4);assert c.read(p+VISIBLE,1)[0]==0
  if mode=='fight':
   c.run(6);c.screenshot(out/'wild-combat-clear.png')
   assert not c.read(p+VISIBLE,1)[0]
   for frame in range(120):
    c.run(1,[0] if frame<12 else [])
    if c.read(p+VISIBLE,1)[0] and c.read(p+COMPACT,1)[0]:break
   else:raise AssertionError('Growl compact notice never appeared')
   c.run(6);c.screenshot(out/'wild-compact-notice.png')
   win=c.read(p+WINDOWS+3,1)[0]
   top=c.read(s['gWindows']+win*12+2,1)[0]
   assert top==2,top
   c.run(35);assert not c.read(p+VISIBLE,1)[0]
  if mode=='detour':
   px,py=struct.unpack('<ii',c.read(p+4,8));start=(mtx+(px//256-ox)//16,mty+(py//256-oy)//16)
   # Independent tile BFS then real D-pad travel, with the opponent held still.
   queue=collections.deque([start]);prev={start:None}
   while queue and et not in prev:
    pos=queue.popleft()
    for dx,dy in [(1,0),(-1,0),(0,1),(0,-1)]:
     n=(pos[0]+dx,pos[1]+dy)
     sx,sy=ox+(n[0]-mtx)*16,oy+(n[1]-mty)*16
     if n in prev or not(10<=sx<=230 and 32<=sy<=120) or tile(*n)&0xC00:continue
     prev[n]=pos;queue.append(n)
   assert et in prev
   path=[];node=et
   while node is not None:path.append(node);node=prev[node]
   for node in list(reversed(path))[1:-1]:
    target=(ox+(node[0]-mtx)*16,oy+(node[1]-mty)*16)
    for _ in range(80):
     px,py=[v/256 for v in struct.unpack('<ii',c.read(p+4,8))]
     if abs(px-target[0])<2 and abs(py-target[1])<2:break
     key=(7 if px<target[0] else 6) if abs(px-target[0])>=2 else (5 if py<target[1] else 4)
     c.write(p+e+60,b'\xfe');c.run(1,[key])
    else:raise AssertionError(('movement blocked',node,px,py,target))
   hp=int.from_bytes(c.read(p+e+22,2),'little')
   for frame in range(240):
    c.write(p+e+60,b'\xfe');c.run(1,[8] if frame%30<6 else [])
    if int.from_bytes(c.read(p+e+22,2),'little')<hp:break
   else:
    c.screenshot(out/'detour-melee-failed.png')
    raise AssertionError(('melee failed after walking around cover',c.read(p+4,8).hex(),c.read(p+e+4,8).hex(),c.read(p+STATE,1).hex()))
   print('Visible detour: actual movement and melee damage PASS',flush=True)
 finish_flee(c)
 results.append({'case':mode,'enemy_map_grid':et,'grass':behavior(*et)==2,'escape_and_field_movement':True,'passed':True});c.close()
 print(mode,'PASS',flush=True)
# Exercise the actual land-encounter caller with ordinary walking as well.
c=Core(repo/'pokefirered.gba',args.core);c.load_state(out/'wild-base.state')
for frame in range(2400):
 c.run(1,[[6,7,4,5][(frame//24)%4]])
 if c.read(s['gRealtimeBattleActive'],1)[0]:break
else:raise AssertionError('No normal walking encounter was generated')
p=wait_state(c,2);e=c.read(p,1400).find(struct.pack('<I',s['gEnemyParty']));assert e>0
w,h,buf,layout,tile,behavior=grid(c)
ox,oy,tx,ty=struct.unpack('<hhhh',c.read(p+ORIGIN,8));ex,ey=struct.unpack('<ii',c.read(p+e+4,8))
assert behavior(tx+((ex//256-(ox-8))>>4),ty+((ey//256+6-(oy-8))>>4))==2
finish_flee(c);c.close()
results.append({'case':'ordinary_walking_encounter','grass_spawn':True,'escape_and_movement':True,'passed':True})
print('Normal walking encounter caller PASS',flush=True)
(out/'wild-native-results.json').write_text(json.dumps({'intro_escape':True,'cases':results,'method':'Disposable emulator RAM map geometry, isolated synthetic maze NPCs and native start callback; normal RT setup, D-pad/melee and START inputs plus ordinary walking encounter. Existing player save untouched.'},indent=2))
