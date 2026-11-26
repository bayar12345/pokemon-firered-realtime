"""Production-ROM cover, stream, overlap and cleanup checks. Uses attack_native args."""
from attack_native import *
from collections import Counter

def u16(c,a):return int.from_bytes(c.read(a,2),'little')
def pos(c,f,x,y):c.write(f+4,struct.pack('<ii',x*256,y*256))
def cast(c,f,target,move):
 c.write(f+38,bytes(2));call(c,'RtUseMove',[f,target,mid[move]])
def resources(c):return c.read(s['gSpriteTileAllocBitmap'],128),c.u32(s['gOamMatrixAllocBitmap'])
def metadata(c,anim):
 b=c.read(s['sRtPtdAnims']+24*anim,24)
 return int.from_bytes(b[:4],'little'),int.from_bytes(b[4:8],'little'),int.from_bytes(b[8:10],'little'),b[10]
def verify_stream(c,f):
 a=f+CAST
 if not c.read(a+65,1)[0]:return None
 sid=c.read(a+55,1)[0]
 # A frontend may yield in the middle of the native allocation function.
 if sid>=64 or not c.read(a+67,1)[0]:return None
 anim=u16(c,a+68);pixels,palette,stride,count=metadata(c,anim)
 tile=u16(c,s['gSprites']+sid*68+4)&1023
 actual=c.read(0x6010000+tile*32,stride)
 matches=[i for i in range(count) if actual==c.read(pixels+i*stride,stride)]
 assert matches,('VRAM differs from imported ROM frames',anim)
 pal=c.read(a+63,1)[0]
 assert c.read(s['gPlttBufferUnfaded']+512+pal*32,32)==c.read(palette,32)
 return anim,matches[0]
def run(c,p,e,frames=190,save=None):
 seen=[];steps=Counter()
 for i in range(frames):
  c.write(p+e+60,bytes(1));c.run(1)
  steps[u16(c,p+layout[6]-2)]+=1
  if i%4==0:
   v=verify_stream(c,p)
   if v:seen.append(v)
  if save and i==36:c.screenshot(OUT/save)
 return seen,dict(steps)

base();results=[]
# A solid central rock stops both the rendered jet and damage.
c,p,e=start('CHARIZARD','FLAMETHROWER','PewterCity_Gym');pos(c,p,160,160);pos(c,p+e,280,160);c.run(5)
initial=resources(c);old=u16(c,p+e+22);cast(c,p,p+e,'FLAMETHROWER');seen,steps=run(c,p,e,save='flamethrower-rock-cover.png')
assert len(set(seen))>2 and u16(c,p+e+22)==old
assert resources(c)==initial
results.append({'case':'Brock rock clips flame and blocks damage','streamed_frames_verified':len(seen),'distinct_frames':len(set(seen)),'damage_blocked':True,'resources_restored':True,'frame_steps':steps,'passed':True});c.close()
# Destructible cover still takes damage, with blocked first and open second cast.
c,p,e=start('CHARIZARD','FLAMETHROWER','CeladonCity_Gym');pos(c,p,160,160);pos(c,p+e,280,160);c.run(5)
obj=p+layout[14]+2*8;assert c.read(obj+5,1)[0]==3
old=u16(c,p+e+22);cast(c,p,p+e,'FLAMETHROWER');run(c,p,e,save='flamethrower-erika-damaged-tree.png')
assert c.read(obj+5,1)[0]==1 and u16(c,p+e+22)==old
cast(c,p,p+e,'FLAMETHROWER');run(c,p,e,save='flamethrower-erika-destroyed-tree.png')
assert c.read(obj+5,1)[0]==0 and u16(c,p+e+22)<old
results.append({'case':'Erika tree 3 to 1 to 0; damage blocked until destroyed','passed':True});c.close()
# Two casters rotate their jets independently; final OAM retains private matrices.
c,p,e=start('CHARIZARD','FLAMETHROWER',True);position(c,p,e,'east',True);initial=resources(c)
cast(c,p,p+e,'FLAMETHROWER');call(c,'RtLaunchCast',[p+e,p,mid['ICE_BEAM']]);c.write(p+e+CAST+20,struct.pack('<ii',0,-256))
c.run(30)
assert c.read(p+CAST+65,1)[0] and c.read(p+e+CAST+65,1)[0]
m1=c.read(p+CAST+66,1)[0];m2=c.read(p+e+CAST+66,1)[0];assert m1!=m2
assert c.read(s['gOamMatrices']+m1*8,8)!=c.read(s['gOamMatrices']+m2*8,8)
verify_stream(c,p);verify_stream(c,p+e);c.screenshot(OUT/'simultaneous-private-beams.png')
run(c,p,e,240);assert resources(c)==initial
results.append({'case':'Simultaneous fire and ice with independent exact-angle matrices and VRAM','resources_restored':True,'passed':True});c.close()
# Repeated mixed effects cannot leak tile or matrix slots.
c,p,e=start('BULBASAUR','ICE_BEAM',True);position(c,p,e,'east',True);initial=resources(c);distinct=set();steps=Counter()
for i,move in enumerate(['ICE_BEAM','HYPER_BEAM','SLUDGE_BOMB','SHADOW_BALL','HEAT_WAVE','RAZOR_LEAF']*4):
 c.write(p+22,struct.pack('<HHH',1000,1000,1000));c.write(p+e+22,struct.pack('<HHH',1000,1000,1000));c.write(p+e+104,b'\xff\xff');c.write(p+38,bytes(2))
 # Finite PP/cooldowns are reset only in this disposable stress fixture.
 call(c,'RtLaunchCast',[p,p+e,mid[move]]);seen,counts=run(c,p,e,210);distinct.update(seen);steps.update(counts)
 call(c,'RtSetStatus',[p+e,0]);c.run(50)
 assert resources(c)==initial,(i,move,'resource leak')
results.append({'case':'24 mixed repeated casts with no tile or matrix leak','distinct_beam_frames_verified':len(distinct),'frame_steps':dict(steps),'passed':True});c.close()
# Sun halves the charging interval; the visible jet grows before its first hit.
c,p,e=start('BULBASAUR','SOLAR_BEAM',True);position(c,p,e,'east',True);c.write(p+layout[26],bytes([2]));cast(c,p,p+e,'SOLAR_BEAM')
first_visible=first_damage=None;old=u16(c,p+e+22)
for i in range(130):
 c.write(p+e+60,bytes(1));c.run(1)
 if c.read(p+CAST+65,1)[0] and first_visible is None:first_visible=i
 if u16(c,p+e+22)<old and first_damage is None:first_damage=i
assert first_visible is not None and first_visible<first_damage<60,(first_visible,first_damage)
results.append({'case':'Sunny Solar Beam visible before damage','first_visible_frame':first_visible,'first_damage_frame':first_damage,'passed':True});c.close()
(OUT/'attack-integration-results.json').write_text(json.dumps({'cases':results,'passed':all(r['passed'] for r in results)},indent=2));print(json.dumps(results,indent=2))
