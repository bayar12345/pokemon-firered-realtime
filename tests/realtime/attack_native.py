from pathlib import Path
from PIL import Image,ImageDraw
import sys,struct,json,re,subprocess,math,argparse
parser=argparse.ArgumentParser(description='Capture and verify imported attack effects using a disposable production-ROM fixture.')
parser.add_argument('--repo',type=Path,default=Path(__file__).resolve().parents[2])
for name in ['symbols','battery','core','output-dir','arm-tools','layout']:parser.add_argument('--'+name,type=Path,required=True)
parser.add_argument('--smoke',action='store_true');opts=parser.parse_args();P=opts.repo;OUT=opts.output_dir;OUT.mkdir(parents=True,exist_ok=True)
helper=P/'tests/realtime/psychic_native.py'
sys.argv=[str(helper),'--repo',str(P),'--symbols',str(opts.symbols),'--battery',str(opts.battery),'--core',str(opts.core),'--output-dir',str(OUT),'--arm-tools',str(opts.arm_tools),'--layout',str(opts.layout)]
q={'__file__':str(helper),'__name__':'fixture'};exec(compile(helper.read_text(encoding='utf-8').split('\nresults=[]')[0],str(helper),'exec'),q)
ns=q['ns'];s=q['s'];Core=q['Core'];call=q['call'];layout=json.loads(opts.layout.read_text());CAST=layout[2]
mid={n:int(i) for n,i in re.findall(r'#define MOVE_(\w+)\s+(\d+)',(P/'include/constants/moves.h').read_text())}
profiles={m:(k,int(r),int(st)) for m,k,r,st in re.findall(r'\[MOVE_(\w+)\] = \{(\w+),(\d+),\d+,\w+,[^,]+,(\d+),', (P/'src/data/realtime_move_profiles.h').read_text())}
species={n:int(i) for n,i in re.findall(r'#define SPECIES_(\w+)\s+(\d+)',(P/'include/constants/species.h').read_text())}
groups=json.loads((P/'data/maps/map_groups.json').read_text());lookup={n:(gi,ni) for gi,g in enumerate(groups['group_order']) for ni,n in enumerate(groups[g])}

def base():
 c=ns['boot']();c.save_state(OUT/'field-base.state');c.close()
def start(mon,move,arena=False):
 c=Core(P/'pokefirered.gba',opts.core);c.load_state(OUT/'field-base.state')
 c.write(0x203FF00,struct.pack('<I',species[mon]));call(c,'SetMonData',[s['gPlayerParty'],11,0x203FF00]);call(c,'CalculateMonStats',[s['gPlayerParty']]);call(c,'SetMonMoveSlot',[s['gPlayerParty'],mid[move],0])
 if arena:
  name=arena if isinstance(arena,str) else 'SaffronCity_Gym';leader={'SaffronCity_Gym':'Sabrina','PewterCity_Gym':'Brock','CeladonCity_Gym':'Erika'}[name];obj=next(o for o in json.loads((P/'data/maps'/name/'map.json').read_text())['object_events'] if o['script'].endswith('_'+leader))
  sb=c.u32(s['gSaveBlock1Ptr']);c.write(sb+0xee0+0x500//8,bytes([255])*96)
  g,n=lookup[name];c.write(s['sWarpDestination'],struct.pack('<bbbxhh',g,n,-1,obj['x'],obj['y']+1));ns['callback'](c,'WarpIntoMap')
  c.write(s['gMain']+0x438,bytes(1));c.write(s['gMain']+4,struct.pack('<I',s['CB2_LoadMap']|1));c.run(600)
  ns['press'](c,4,n=10);ns['press'](c,8,wait=130);ns['press'](c,8,wait=80);ns['press'](c,8);p=ns['wait_state'](c,4)
 else:p=ns['begin'](c);assert p;ns['wait_state'](c,4)
 e=c.read(p,1400).find(struct.pack('<I',s['gEnemyParty']));assert e>0
 for f in [p,p+e]:c.write(f+22,struct.pack('<HHH',1000,1000,1000))
 c.write(p+e+38,b'\xff\xff');c.write(p+e+104,b'\xff\xff')
 return c,p,e
def position(c,p,e,direction,arena):
 if arena:
  coords={'east':(128,160,248,160),'west':(352,160,232,160),'north':(128,208,128,96),'south':(128,96,128,208),'diagonal':(128,112,240,176)}[direction]
 else:coords={'east':(64,112,144,112),'west':(144,112,64,112),'north':(96,112,96,48),'south':(96,48,96,112),'diagonal':(64,64,144,112)}[direction]
 c.write(p+4,struct.pack('<ii',coords[0]*256,coords[1]*256));c.write(p+e+4,struct.pack('<ii',coords[2]*256,coords[3]*256))
 facing=3 if direction=='west' else 2 if direction=='north' else 0 if direction=='south' else 3
 # RtAim's player cone accepts the chosen target; a native attack then owns aim.
 dx,dy=coords[2]-coords[0],coords[3]-coords[1]
 c.write(p+35,bytes([2 if abs(dx)>=abs(dy) and dx<0 else 3 if abs(dx)>=abs(dy) else 1 if dy<0 else 0]))
 c.write(p+e+35,bytes([3 if abs(dx)>=abs(dy) and dx<0 else 2 if abs(dx)>=abs(dy) else 0 if dy<0 else 1]));c.run(6)
 return coords
def capture(c,path):c.screenshot(path);return Image.open(path).convert('RGB')

if __name__=='__main__':
 base();cases=[('CHARIZARD','FLAMETHROWER','east',False),('CHARIZARD','FLAMETHROWER','east',True),('BLASTOISE','HYDRO_PUMP','east',True),('BULBASAUR','ICE_BEAM','east',True)]
 if not opts.smoke:
  cases += [('CHARIZARD','FLAMETHROWER',d,a) for a in [False,True] for d in ['west','north','south','diagonal']]
  cases += [('PIKACHU','THUNDERBOLT','east',a) for a in [False,True]]
  cases += [('BULBASAUR',m,'east',True) for m in ['HYPER_BEAM','SOLAR_BEAM','PSYBEAM','DARK_PULSE','VINE_WHIP','RAZOR_LEAF','SLUDGE_BOMB','SHADOW_BALL','FIRE_PUNCH','PSYCHIC','EARTH_POWER','EARTHQUAKE','BLIZZARD','HEAT_WAVE','QUICK_ATTACK']]
 report=[]
 for mon,move,direction,arena in cases:
  c,p,e=start(mon,move,arena);coords=position(c,p,e,direction,arena);name=f'{move.lower()}-{mon.lower()}-{direction}-'+('arena' if arena else 'field')
  kind,reach,startup=profiles[move]
  if kind in ('RT_CLOSE','RT_DASH') or move=='PSYCHIC':
   gap=28 if kind in ('RT_CLOSE','RT_DASH') else 64
   c.write(p+e+4,struct.pack('<ii',(coords[0]+gap)*256,coords[1]*256));c.run(3)
  frames=[];snapshots=[];ptd_seen=False;beam_seen=False;start_frame=None;hp_before=int.from_bytes(c.read(p+e+22,2),'little')
  for frame in range(210):
   c.write(p+e+60,b'\0');c.run(1,[8] if frame<12 else [])
   active=c.read(p+CAST+40,1)[0];t=int.from_bytes(c.read(p+CAST+2,2),'little')
   if active and start_frame is None:start_frame=frame
   if not start_frame is None and frame%4==0 and len(frames)<35:
    path=OUT/(name+f'-{len(frames):02}.png');frames.append(capture(c,path))
    if active and startup+4<=t<=startup+36:snapshots.append((t,path))
   if c.read(p+CAST+65,1)[0]:beam_seen=True
   # Runtime FX are after both fighters and six 44-byte projectile records.
   fx=p+layout[4]
   if any(c.read(fx+i*layout[5],1)[0] and int.from_bytes(c.read(fx+i*layout[5]+8,2),'little') for i in range(24)):ptd_seen=True
   if start_frame is not None and frame-start_frame>140:break
  assert start_frame is not None,name
  if move in ['FLAMETHROWER','ICE_BEAM','HYDRO_PUMP','HYPER_BEAM','SOLAR_BEAM','PSYBEAM','DARK_PULSE','VINE_WHIP']:assert beam_seen,(name,'no streamed beam')
  else:assert ptd_seen,(name,'no PTD FX')
  if frames:frames[0].save(OUT/(name+'.png'),save_all=True,append_images=frames[1:],duration=67,loop=0)
  if snapshots:
   chosen=min(snapshots,key=lambda v:abs(v[0]-(startup+14)));(OUT/(name+'-peak.png')).write_bytes(chosen[1].read_bytes())
  report.append({'case':name,'beam_seen':beam_seen,'ptd_effect_seen':ptd_seen,'coordinates':coords,'hp_before':hp_before,'hp_after':int.from_bytes(c.read(p+e+22,2),'little'),'passed':True})
  c.close();print(name,'PASS',flush=True)
 (OUT/'visual-results.json').write_text(json.dumps(report,indent=2))
