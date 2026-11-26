"""Production smoke: observe imported effects for every damaging-move mapping."""
from attack_native import *
base();c,p,e=start('BULBASAUR','TACKLE',True);position(c,p,e,'east',True)
c.save_state(OUT/'all-moves-base.state')
manifest=json.loads((P/'assets/ptd/imported-attacks.json').read_text());results=[]
for m in manifest['moves']:
 c.load_state(OUT/'all-moves-base.state');move=m['move'];kind=m['kind']
 gap=24 if kind in ['RT_CLOSE','RT_DASH','RT_AIR'] else 60 if kind in ['RT_POINT','RT_AREA','RT_CONE','RT_PARTY'] else 100
 c.write(p+e+4,struct.pack('<ii',(128+gap)*256,160*256));c.write(p+38,bytes(2));c.write(p+e+104,b'\xff\xff');c.run(2)
 if move=='DREAM_EATER':call(c,'RtSetStatus',[p+e,2])
 if move=='SNORE':call(c,'RtSetStatus',[p,2])
 if move=='SPIT_UP':call(c,'RtApplyStatusMove',[p,p+e,mid['STOCKPILE']])
 if kind=='RT_SELF':
  # Counter's contact reaction and Bide/Coat's native return projectile.
  if move=='COUNTER':call(c,'RtPtdImpact',[p,p+e,mid[move]])
  else:call(c,'RtFixedShot',[p,p+e,mid[move],5])
 else:call(c,'RtLaunchCast',[p,p+e,mid[move]])
 seen=set()
 for frame in range(200):
  c.write(p+e+60,bytes(1));c.run(1)
  for i in range(24):
   f=p+layout[4]+i*layout[5]
   if c.read(f,1)[0]:
    a=int.from_bytes(c.read(f+layout[21],2),'little')
    if a:seen.add(a)
  a=p+CAST
  if c.read(a+65,1)[0]:seen.add(int.from_bytes(c.read(a+68,2),'little'))
 passed=bool(seen)
 if not passed:c.screenshot(OUT/(move.lower()+'-missing.png'))
 results.append({'move':move,'kind':kind,'imported_animation_ids':sorted(seen),'passed':passed})
 if len(results)%30==0:print(len(results),'moves checked',flush=True)
c.close();report={'cases':results,'passed':all(r['passed'] for r in results)}
(OUT/'all-moves-results.json').write_text(json.dumps(report,indent=2));print('Failures',[r['move'] for r in results if not r['passed']]);assert report['passed']
