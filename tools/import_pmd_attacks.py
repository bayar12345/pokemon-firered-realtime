"""Convert Pokemon Mystery Dungeon: Red Rescue Team move effects into the
real-time battle's streamed 4bpp art (same output format as import_ptd_attacks.py).

Usage: PMD_RED_ROM=path/to/pmd_red.gba python3 tools/import_pmd_attacks.py
Private use only: the art comes from the user's own PMD Red ROM.
Effect slots per move (from PMD Red's move effect table):
  0 = charge on user, 1 = user effect, 2 = travelling / area effect, 3 = hit on target.
"""
from pathlib import Path
from PIL import Image
import json,re,hashlib,math,collections,sys,os
P=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(P/'tools/pmd'))
from redax import Efo,efo_render
D=P/'graphics/pmd_attacks';D.mkdir(parents=True,exist_ok=True)
for old in D.glob('*.4bpp'):old.unlink()
move_ids={m:int(i) for m,i in re.findall(r'^#define MOVE_(\w+)\s+(\d+)',(P/'include/constants/moves.h').read_text(),re.M)}
profiles={m:(k,int(reach)) for m,k,reach in re.findall(r'\[MOVE_(\w+)\] = \{(\w+),(\d+),',(P/'src/data/realtime_move_profiles.h').read_text())}
battles={m:body for m,body in re.findall(r'\[MOVE_(\w+)\] =\s*\{(.*?)\n    \}',(P/'src/data/battle_moves.h').read_text(),re.S)}
red={re.sub('_','',k):v for k,v in json.loads((P/'tools/pmd/red_moves.json').read_text()).items()}
ALIAS={}
# PMD Red tints some effects at runtime with colour ramps; bake the in-game colours for those files.
RECOLOR={}
# Effects whose unflagged pieces flicker through palettes in PMD; draw them with their main palette.
SHARED={119:3}
BEAMS={'FLAMETHROWER','WATER_GUN','HYDRO_PUMP','ICE_BEAM','PSYBEAM','AURORA_BEAM','HYPER_BEAM','SOLAR_BEAM','AEROBLAST','LUSTER_PURGE','SIGNAL_BEAM','DRAGON_BREATH','VINE_WHIP'}
# Generic PMD hit per type when a move has no dedicated target effect (taken from representative moves).
GENERIC={'NORMAL':'POUND','FIGHTING':'KARATE_CHOP','FLYING':'WING_ATTACK','POISON':'SLUDGE','GROUND':'MUD_SLAP','ROCK':'ROCK_THROW','BUG':'TWINEEDLE','GHOST':'SHADOW_BALL','STEEL':'METAL_CLAW','FIRE':'EMBER','WATER':'WATER_GUN','GRASS':'RAZOR_LEAF','ELECTRIC':'THUNDER_SHOCK','PSYCHIC':'CONFUSION','ICE':'POWDER_SNOW','DRAGON':'DRAGON_RAGE','DARK':'BITE'}
anims=[None];lookup={};palettes=[];pal_lookup={};raw_lookup={};manifest=[];cache={}
def tiles(im):
 data=bytearray();pix=im.load()
 for ty in range(0,im.height,8):
  for tx in range(0,im.width,8):
   for y in range(8):
    for x in range(0,8,2):data.append(pix[tx+x,ty+y]|pix[tx+x+1,ty+y]<<4)
 return bytes(data)
def frames_of(fid,aid,bake):
 key=(fid,aid,bake)
 if key in cache:return cache[key]
 try:e=Efo(f'efob{fid:03d}')
 except Exception:cache[key]=None;return None
 if aid>=e.n:cache[key]=None;return None
 fr=e.anim(aid,0)
 if not fr:cache[key]=None;return None
 ims=[];durs=[]
 remap=RECOLOR.get(fid)
 for f in fr:
  im=efo_render(e,f['pose'],256,shared=SHARED.get(fid)) if f['pose']>=0 else Image.new('RGBA',(256,256))
  if remap:im.putdata([remap.get(px[:3],px[:3])+(px[3],) if px[3] else px for px in im.getdata()])
  if bake and (f['off'][0] or f['off'][1]):
   o=Image.new('RGBA',(256,256));o.paste(im,(f['off'][0],f['off'][1]),im);im=o
  ims.append(im);durs.append(f['dur'])
 cache[key]=(ims,durs);return cache[key]
def add(slot,role,width=0,height=0):
 fid,aid=slot['effect_file'],slot['anim_in_file']
 key=(fid,aid,role,width,height)
 if key in lookup:return lookup[key]
 got=frames_of(fid,aid,role!='shot')
 if not got:return 0
 ims,durs=got
 bb=None
 for im in ims:
  b=im.getbbox()
  if b:bb=b if not bb else (min(bb[0],b[0]),min(bb[1],b[1]),max(bb[2],b[2]),max(bb[3],b[3]))
 if not bb:return 0
 # Resample the variable PMD timeline to a uniform 30 fps strip (max 32 frames).
 # Most PMD effect frames last 1-2 ticks, so 30 fps keeps nearly every drawn frame.
 total=sum(durs);step=2;seq=[];t=0;acc=0;i=0
 while t<total and len(seq)<32:
  while i<len(durs)-1 and acc+durs[i]<=t:acc+=durs[i];i+=1
  seq.append(i);t+=step
 ims=[ims[k] for k in seq];durs=[step]*len(ims)
 longest0={'shot':32,'hit':64,'status':64,'screen':48,'indicator':24,'condition':24,'cast':64}.get(role,64)
 # Very tall/wide PMD effects (bolts from the sky, screen-wide waves) are cropped around
 # the part that touches the target instead of being shrunk into a few pixels.
 if not role.startswith('beam'):
  x0,y0,x1,y1=bb
  if y1-y0>longest0*1.5:y0=y1-int(longest0*1.25);y1=y0+int(longest0*1.25)
  if x1-x0>longest0*1.5:cx=(x0+x1)//2;x0=cx-int(longest0*0.625);x1=x0+int(longest0*1.25)
  bb=(x0,y0,x1,y1)
 crops=[im.crop(bb) for im in ims];w,h=crops[0].size
 if role in ('beam','beam_arena'):
  canvas=(math.ceil(width/64)*64,32);fit=(width,height)
 else:
  longest={'shot':32,'hit':64,'status':64,'screen':48,'indicator':24,'condition':24}.get(role,64)
  scale=min(1,longest/max(w,h));fit=(max(1,round(w*scale)),max(1,round(h*scale)))
  size=32 if max(fit)<=32 else 64;canvas=(size,size)
 converted=[]
 for im in crops:
  resized=im.resize(fit,Image.Resampling.NEAREST);out=Image.new('RGBA',canvas)
  out.paste(resized,(0,(canvas[1]-fit[1])//2) if role.startswith('beam') else ((canvas[0]-fit[0])//2,(canvas[1]-fit[1])//2))
  converted.append(out)
 # PMD effects already use one 16-colour palette; keep the colours actually drawn.
 colors=collections.Counter(c[:3] for im in converted for c in im.getdata() if c[3]>=96)
 rgb=[c for c,_ in colors.most_common(15)]+[(0,0,0)]*(15-min(15,len(colors)))
 nearest={c:1+min(range(15),key=lambda i:sum((c[k]-rgb[i][k])**2 for k in range(3))) for c in colors}
 raw=bytearray();alpha_widths=[]
 for im in converted:
  ix=Image.new('P',canvas);ix.putdata([nearest[(r,g,b)] if a>=96 else 0 for r,g,b,a in im.getdata()])
  box=im.getchannel('A').point(lambda a:255 if a>=96 else 0).getbbox();alpha_widths.append(box[2]-box[0] if box else 0)
  if role.startswith('beam'):
   for x in range(0,canvas[0],64):raw.extend(tiles(ix.crop((x,0,x+64,32))))
  else:raw.extend(tiles(ix))
 values=[0]+[(c[0]>>3)|((c[1]>>3)<<5)|((c[2]>>3)<<10) for c in rgb]
 palette_key=tuple(values)
 if palette_key not in pal_lookup:pal_lookup[palette_key]=len(palettes);palettes.append(values)
 digest=hashlib.sha256(raw).hexdigest()
 if digest not in raw_lookup:
  file='anim_'+digest[:16]+'.4bpp';(D/file).write_bytes(raw);raw_lookup[digest]=file
 file=raw_lookup[digest]
 start=0;end=len(ims)-1
 if role.startswith('beam'):
  eligible=[i for i,w in enumerate(alpha_widths) if w>=max(alpha_widths)*.92]
  if eligible:start=eligible[0];end=eligible[-1]
  end=min(end,start+5)
 avg=sum(durs)/len(durs);fps=max(4,min(60,round(60/avg)))
 info={'id':len(anims),'file':file,'palette':pal_lookup[palette_key],'frames':len(ims),'fps':fps,'width':canvas[0],'height':canvas[1],'stride':canvas[0]*canvas[1]//2,'columns':canvas[0]//64 if role.startswith('beam') else 1,'hold_start':start,'hold_end':end,'extent':fit[0]}
 anims.append(info);lookup[key]=info['id']
 manifest.append({**info,'source':'PMD Red Rescue Team','effect_file':f'efob{fid:03d}','effect_anim':aid,'role':role,'raw_sha256':digest})
 return info['id']
def slots(move):
 m=red.get(re.sub('_','',move))
 return [None]*4 if not m else m['effects']
def first(s,order):
 for i in order:
  if s[i] and s[i]['kind']!='effect_sprite_alt':return s[i]
 return None
maps=[];statusmaps=[]
for move,mid in sorted(move_ids.items(),key=lambda x:x[1]):
 if move not in battles or move not in profiles:continue
 body=battles[move];power=int(re.search(r'\.power = (\d+)',body)[1]);effect=re.search(r'\.effect = (\w+)',body)[1];typ=re.search(r'\.type = TYPE_(\w+)',body)[1]
 kind,reach=profiles[move];s=slots(move)
 damaging=power or effect in {'EFFECT_DRAGON_RAGE','EFFECT_SONICBOOM','EFFECT_LEVEL_DAMAGE','EFFECT_PSYWAVE','EFFECT_SUPER_FANG','EFFECT_OHKO','EFFECT_COUNTER','EFFECT_MIRROR_COAT','EFFECT_BIDE'}
 if damaging:
  hit=first(s,[3,2,1]) or first(slots(GENERIC.get(typ,'POUND')),[3,2,1])
  impact=add(hit,'hit') if hit else 0
  if not impact:continue  # no PMD effect: keep FireRed's own hit particles for this move
  shot=cast=beam=arena=0
  if kind=='RT_SHOT' or move in ('COUNTER','MIRROR_COAT','BIDE'):
   proj=first(s,[2,1,3]) or first(slots(GENERIC.get(typ,'POUND')),[2,1,3]) or hit
   shot=add(proj,'shot') if proj else 0
   if not shot:shot=impact
  if move in BEAMS:
   src=first(s,[2,1,3]) or hit;thickness=12 if move=='WATER_GUN' else 22
   beam=add(src,'beam',max(16,reach-16),thickness);arena=add(src,'beam_arena',max(16,(reach-24)//2),16 if thickness>16 else 10)
  elif kind in ('RT_CONE','RT_AREA'):
   src=first(s,[2,1,0,3]) or hit;cast=add(src,'cast')
  maps.append({'move':move,'move_id':mid,'kind':kind,'type':typ,'shot':shot,'impact':impact,'cast':cast,'beam':beam,'beam_arena':arena,'dedicated_source':any(s)})
 else:
  src=first(s,[1,2,3,0])
  if not src:continue
  target=re.search(r'\.target = ([^,]+)',body)[1]
  own='MOVE_TARGET_USER' in target or kind=='RT_SELF'
  clock={'REFLECT':'RTC_REFLECT','LIGHT_SCREEN':'RTC_LIGHTSCREEN','SAFEGUARD':'RTC_SAFEGUARD','MIST':'RTC_MIST','PROTECT':'RTC_PROTECT','DETECT':'RTC_PROTECT','INGRAIN':'RTC_INGRAIN','CHARGE':'RTC_CHARGE','MAGIC_COAT':'RTC_MAGICCOAT'}
  recipient=2 if move in ('PERISH_SONG','HAZE') else 0 if own else 1
  a=add(src,'screen' if move in clock else 'status')
  if a:statusmaps.append({'move':move,'move_id':mid,'anim':a,'recipient':recipient,'clock':clock.get(move)})
def fx(move,role):
 s=first(slots(move),[1,2,3,0]);return add(s,role) if s else 0
stage_src=[('HOWL','GROWL'),('HARDEN','TAIL_WHIP'),('AGILITY','STRING_SHOT'),('GROWTH','CHARM'),('AMNESIA','FAKE_TEARS'),('LOCK_ON','SAND_ATTACK'),('DOUBLE_TEAM','SWEET_SCENT')]
stage_maps=[[fx(down,'indicator'),fx(up,'indicator')] for up,down in stage_src]
conditions=[fx(m,'condition') for m in ('WILL_O_WISP','POISON_POWDER','THUNDER_WAVE','HYPNOSIS','ICE_BEAM','CONFUSE_RAY')]
lines=['// Generated by tools/import_pmd_attacks.py from PMD Red Rescue Team effects (private use).']
for idx,(digest,file) in enumerate(raw_lookup.items()):lines.append(f'static const u32 sPtdPixels_{idx}[] = INCBIN_U32("graphics/pmd_attacks/{file}");')
file_index={file:i for i,file in enumerate(raw_lookup.values())}
for i,pal in enumerate(palettes):lines.append(f'static const u16 sPtdPalette_{i}[16] = {{'+','.join(hex(v) for v in pal)+'};')
lines.append('static const struct RtPtdAnim sRtPtdAnims[] = {{0},')
for a in anims[1:]:lines.append(' {sPtdPixels_%d,sPtdPalette_%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d},'%(file_index[a['file']],a['palette'],a['stride'],a['frames'],a['fps'],a['width'],a['height'],a['columns'],a['hold_start'],a['hold_end'],a['palette'],a['extent']))
lines.append('};\nstatic const struct RtPtdMoveArt sRtPtdMoveArt[MOVES_COUNT] = {')
for m in maps:lines.append(' [MOVE_%s] = {%d,%d,%d,%d,%d},'%(m['move'],m['shot'],m['impact'],m['cast'],m['beam'],m['beam_arena']))
lines.append('};\nstatic const struct RtPtdStatusArt sRtPtdStatusArt[MOVES_COUNT] = {')
for m in statusmaps:lines.append(' [MOVE_%s] = {%d,%d,%s},'%(m['move'],m['anim'],m['recipient'],m['clock']+'+1' if m['clock'] else '0'))
lines.append('};\nstatic const u16 sRtPtdStageArt[RT_STAT_COUNT][2] = {'+','.join('{%d,%d}'%tuple(row) for row in stage_maps)+'};')
lines.append('static const u16 sRtPtdConditionArt[6] = {'+','.join(map(str,conditions))+'};')
(P/'src/data/realtime_ptd_graphics.h').write_text('\n'.join(lines)+'\n',encoding='utf-8',newline='\n')
report={'format_version':1,'source':'PMD Red Rescue Team (user ROM, private)','mapped_damaging_moves':len(maps),'mapped_status_moves':len(statusmaps),'converted_animations':len(anims)-1,'unique_pixel_sheets':len(raw_lookup),'palettes':len(palettes),'rom_pixel_bytes':sum((D/f).stat().st_size for f in raw_lookup.values()),'moves':maps,'status_moves':statusmaps,'stat_indicators':stage_maps,'condition_indicators':conditions,'animations':manifest}
(P/'assets/pmd').mkdir(parents=True,exist_ok=True)
(P/'assets/pmd/imported-attacks.json').write_text(json.dumps(report,indent=2),encoding='utf-8',newline='\n')
print(json.dumps({k:report[k] for k in ['mapped_damaging_moves','mapped_status_moves','converted_animations','unique_pixel_sheets','palettes','rom_pixel_bytes']},indent=2))
