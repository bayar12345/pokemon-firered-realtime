"""Imported PTD status-art coverage and native A-button visual captures."""
from attack_native import *
from PIL import ImageSequence,ImageChops
base();report=[];manifest=json.loads((P/'assets/ptd/imported-attacks.json').read_text());layout=json.loads(opts.layout.read_text())
c,p,e=start('BULBASAUR','SMOKESCREEN',True);position(c,p,e,'east',True);c.write(p+e+4,struct.pack('<ii',156*256,160*256));c.run(5);c.save_state(OUT/'status-base.state')
def ptd_instances(c,p):
 out=[]
 for i in range(24):
  f=p+layout[4]+i*layout[5]
  if c.read(f,1)[0] and int.from_bytes(c.read(f+layout[21],2),'little'):out.append((i,c.read(f+1,1)[0],int.from_bytes(c.read(f+8,2),'little'),c.read(f+14,1)[0]))
 return out
# Every status row can stream its included ROM frames without an original SWF.
for row in manifest['status_moves']:
 c.load_state(OUT/'status-base.state');call(c,'RtPtdStatusFx',[p,p+e,mid[row['move']],False]);seen=False
 for _ in range(8):
  c.run(1)
  if any(a==row['anim'] for _,_,a,_ in ptd_instances(c,p)):seen=True
 assert seen,('status art missing',row['move'])
 report.append({'move':row['move'],'source':row['source_symbol'],'imported_art_observed':True})
c.close();(OUT/'status-art-coverage.json').write_text(json.dumps({'cases':report,'passed':True},indent=2));print('All',len(report),'status move art rows observed',flush=True)
# Real controller casts check the integration path, recipient and gym readability.
cases=['SMOKESCREEN','REFLECT','LIGHT_SCREEN','BARRIER','PROTECT','DETECT','SAFEGUARD','MIST','ACID_ARMOR','WITHDRAW','DOUBLE_TEAM','SWORDS_DANCE','CALM_MIND','GROWL','TAIL_WHIP','THUNDER_WAVE','TOXIC','SLEEP_POWDER','HYPNOSIS','CONFUSE_RAY','LEECH_SEED','AROMATHERAPY','RECOVER','RAIN_DANCE','HAZE','ENCORE','SUBSTITUTE','MEAN_LOOK']
rows={r['move']:r for r in manifest['status_moves']};results=[]
for move in cases:
 c=Core(P/'pokefirered.gba',opts.core);c.load_state(OUT/'status-base.state');call(c,'SetMonMoveSlot',[s['gPlayerParty'],mid[move],0]);c.write(p+22,struct.pack('<HHH',500,1000,500));c.write(p+e+38,b'\xff\xff');c.write(p+e+104,b'\xff\xff');c.write(p+82,b'\x06');c.write(p+e+83,b'\xfa');c.run(5)
 kind,reach,startup=profiles[move];gap=120 if rows[move]['recipient']==0 else max(24,min(72,reach-8))
 c.write(p+e+4,struct.pack('<ii',(128+gap)*256,160*256));c.run(3)
 c.screenshot(OUT/'before-cast.png');before=Image.open(OUT/'before-cast.png').convert('RGB')
 center=(64,73) if rows[move]['recipient']==0 else ((128+gap)//2,73)
 region=(center[0]-28,center[1]-28,center[0]+28,center[1]+28)
 source=rows[move]['anim'];frames=[];seen=[];kind_seen=set();first=None;peaks=[]
 anim=next(a for a in manifest['animations'] if a['id']==source);raw=(P/'graphics/ptd_attacks'/anim['file']).read_bytes();energy=[sum(bool(v&15)+bool(v>>4) for v in raw[i*anim['stride']:(i+1)*anim['stride']]) for i in range(anim['frames'])]
 for f in range(170):
  c.write(p+e+60,bytes(1));c.run(1,[8] if f<12 else [])
  active=c.read(p+CAST+40,1)[0]
  if active and first is None:first=f
  effects=ptd_instances(c,p);seen.extend((f,side) for _,_,a,side in effects if a==source);kind_seen.update(k for _,k,_,_ in effects)
  if first is not None and f%4==0 and len(frames)<35:
   path=OUT/(move.lower()+f'-{len(frames):02}.png');c.screenshot(path);frames.append(Image.open(path).convert('RGB'))
   for slot,_,a,_ in effects:
    if a==source:
     delta=ImageChops.difference(Image.open(path).convert('RGB').crop(region),before.crop(region));score=sum(1 for v in delta.getdata() if max(v)>24);peaks.append((score,path))
 assert seen,(move,'real A cast did not show PTD source')
 if frames:frames[0].save(OUT/(move.lower()+'.png'),save_all=True,append_images=frames[1:],duration=67,loop=0)
 if peaks:(OUT/(move.lower()+'-peak.png')).write_bytes(max(peaks,key=lambda x:x[0])[1].read_bytes())
 expected=2 if rows[move]['recipient']==1 else 1
 assert rows[move]['recipient']==2 or any(side==expected for _,side in seen),(move,'wrong recipient',seen)
 results.append({'move':move,'source_anim':source,'recipient_sides':sorted(set(side for _,side in seen)),'effect_kinds':sorted(kind_seen),'passed':True});c.close();print(move,'PASS',flush=True)
(OUT/'status-visual-results.json').write_text(json.dumps({'cases':results,'passed':True},indent=2))

