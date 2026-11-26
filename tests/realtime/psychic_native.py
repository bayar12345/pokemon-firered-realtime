from pathlib import Path
import sys,struct,subprocess,json
import argparse
parser=argparse.ArgumentParser(description='Production-ROM Psychic lift/slam checks using disposable emulator RAM.')
parser.add_argument('--layout',type=Path)
parser.add_argument('--repo',type=Path,default=Path(__file__).resolve().parents[2])
for name in ['symbols','battery','core','output-dir','arm-tools']:parser.add_argument('--'+name,type=Path,required=True)
options=parser.parse_args();OUT=options.output_dir;OUT.mkdir(parents=True,exist_ok=True)
helper=options.repo/'tests/realtime/wild_native.py'
sys.argv=[str(helper),'--repo',str(options.repo),'--symbols',str(options.symbols),'--battery',str(options.battery),'--core',str(options.core),'--output-dir',str(OUT)]
if options.layout:sys.argv+=['--layout',str(options.layout)]
ns={'__file__':str(helper),'__name__':'fixture'};exec(compile(helper.read_text(encoding='utf-8').split('\nc=boot();')[0],str(helper),'exec'),ns)
s=ns['s'];Core=ns['Core'];CAST=180;STATE=ns['STATE'];SCRATCH=0x203FE00

def call(c,name,args):
    return ns['callback'](c,name,args)

def start(arena=False):
    c=ns['boot']();call(c,'SetMonMoveSlot',[s['gPlayerParty'],94,0])
    if arena:
        repo=options.repo;groups=json.loads((repo/'data/maps/map_groups.json').read_text());lookup={n:(gi,ni) for gi,g in enumerate(groups['group_order']) for ni,n in enumerate(groups[g])}
        name='SaffronCity_Gym';obj=next(o for o in json.loads((repo/'data/maps'/name/'map.json').read_text())['object_events'] if o['script'].endswith('_Sabrina'))
        sb=c.u32(s['gSaveBlock1Ptr']);c.write(sb+0xee0+0x500//8,bytes([255])*96)
        g,n=lookup[name];c.write(s['sWarpDestination'],struct.pack('<bbbxhh',g,n,-1,obj['x'],obj['y']+1));ns['callback'](c,'WarpIntoMap')
        c.write(s['gMain']+0x438,bytes(1));c.write(s['gMain']+4,struct.pack('<I',s['CB2_LoadMap']|1));c.run(600)
        ns['press'](c,4,n=10);ns['press'](c,8,wait=130);ns['press'](c,8,wait=80);ns['press'](c,8)
        p=ns['wait_state'](c,4)
    else:
        p=ns['begin'](c);assert p;ns['wait_state'](c,4)
    e=c.read(p,1400).find(struct.pack('<I',s['gEnemyParty']));assert e>0
    if arena:
        c.write(p+4,struct.pack('<ii',100*256,160*256));c.write(p+e+4,struct.pack('<ii',148*256,160*256))
    c.write(p+e+22,struct.pack('<HHH',1000,1000,1000));c.write(p+22,struct.pack('<HHH',1000,1000,1000))
    c.write(p+e+38,b'\xff\xff');c.write(p+e+60,b'\xfe');c.run(5)
    return c,p,e
def sprite_y2(c,f):
    return int.from_bytes(c.read(s['gSprites']+c.read(f+33,1)[0]*68+38,2),'little',signed=True)
def hp(c,f):return int.from_bytes(c.read(f+22,2),'little')

results=[]
for mode in ['slam','interrupted','fast_dodge','ordinary_walk','arena_slam']:
    c,p,e=start(arena=mode=='arena_slam');enemy=p+e
    if mode in ['slam','interrupted','arena_slam']:
        for _ in range(16):
            c.write(enemy+60,b'\xfe');c.run(1,[8])
            if c.read(p+CAST+40,1)[0]:break
        else:raise AssertionError('A did not start Psychic')
        caster=p;victim=enemy
    else:
        c.write(p+79,bytes([6 if mode=='fast_dodge' else 0]))
        call(c,'RtUseMove',[enemy,p,94]);caster=enemy;victim=p
    oldhp=hp(c,victim);startxy=struct.unpack('<ii',c.read(victim+4,8));lifted=False;min_y=0;damage_frames=[];interrupted=False
    previous=oldhp
    for frame in range(85):
        if mode in ['slam','interrupted','arena_slam'] and not c.read(caster+CAST+41,1)[0]:c.write(enemy+60,b'\xfe')
        elif mode in ['fast_dodge','ordinary_walk']:c.write(enemy+60,b'\xfe')
        c.run(1,[7] if mode in ['fast_dodge','ordinary_walk'] else [])
        time=int.from_bytes(c.read(caster+CAST+2,2),'little');active=c.read(caster+CAST+40,1)[0]
        y=sprite_y2(c,victim);min_y=min(min_y,y)
        if y<0:
            lifted=True
            assert hp(c,victim)==oldhp,('damage before landing',mode,time,y)
        if active and 26<=time<=28 and lifted and mode in ['slam','interrupted','arena_slam']:
            c.screenshot(OUT/f'psychic-{mode}-lift.png')
            if mode=='interrupted' and not interrupted:
                call(c,'RtCancelCast',[caster]);interrupted=True
        current=hp(c,victim)
        if current<previous:damage_frames.append(frame)
        previous=current
        if mode=='slam' and damage_frames and frame<50:c.screenshot(OUT/'psychic-slam-land.png')
    assert sprite_y2(c,victim)==0,(mode,sprite_y2(c,victim))
    if mode in ['slam','ordinary_walk','arena_slam']:
        assert lifted and min_y<=-16 and len(damage_frames)==1 and hp(c,victim)<oldhp,(mode,min_y,damage_frames)
    elif mode=='interrupted':assert interrupted and lifted and hp(c,victim)==oldhp
    else:assert not lifted and hp(c,victim)==oldhp and struct.unpack('<ii',c.read(victim+4,8))!=startxy
    if mode!='arena_slam':ns['finish_flee'](c)
    c.close()
    results.append({'case':mode,'lifted':lifted,'maximum_lift_pixels':-min_y,'damage_events':len(damage_frames),'released_afterwards':True,'passed':True})
    print(mode,'PASS',flush=True)
# Escape while the opponent is airborne must cancel the hold and restore field controls.
c,p,e=start();enemy=p+e;call(c,'RtUseMove',[p,enemy,94])
for _ in range(45):
    c.write(enemy+60,b'\xfe');c.run(1)
    if sprite_y2(c,enemy)<0:break
else:raise AssertionError('No lift before escape test')
ns['finish_flee'](c);c.close()
results.append({'case':'escape_during_lift','released_afterwards':True,'field_movement_restored':True,'passed':True})
print('escape during lift PASS',flush=True)
(OUT/'psychic-native-results.json').write_text(json.dumps({'cases':results,'method':'Production ROM, native controller A and D-pad plus disposable RAM fixtures for Psychic access, HP and enemy inactivity; a small native-function trampoline tests interruption and enemy casting. Current user save untouched.'},indent=2))
