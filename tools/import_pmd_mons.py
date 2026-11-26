"""Convert PMD Red Rescue Team Pokemon sprites into compressed animation banks for
the field follower and the real-time battle (replaces the HGSS follower art).

Usage: PMD_RED_ROM=path/to/pmd_red.gba python3 tools/import_pmd_mons.py
Private use only: the art comes from the user's own PMD Red ROM.

Bank layout (little endian, one INCBIN per sprite key):
  u16 frameCount, u8 animCount(13), u8 size(32/64)
  u32 seqOffset[13][8]        0 = animation missing for that direction
  u32 frameOffset[frameCount] each frame is LZ77 (VRAM safe), size*size/2 bytes unpacked
  sequences: {u16 frame (bit15 = hflip), u8 duration, u8 flags(2=hit,1=return), s8 dx, s8 dy} ... ending with duration 0
Directions follow PMD: 0 down, 1 down-right, 2 right, 3 up-right, 4 up, 5 up-left, 6 left, 7 down-left.
Animations: 0 Walk 1 Attack 2 Strike 3 Shoot 4 Dance 5 Sleep 6 Hurt 7 Idle 8 Swing 9 Double 10 Hop 11 Charge 12 Rotate.
"""
from pathlib import Path
import re,json,struct,sys,hashlib
P=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(P/'tools/pmd'))
import redax
from redax import Ax,IDX,palette,SH,R,u16
from gbalz import compress
OUT=P/'graphics/pmd_mons';OUT.mkdir(parents=True,exist_ok=True)
for f in OUT.glob('*.bin'):f.unlink()
ANIMS=['Walk','Attack','Strike','Shoot','Dance','Sleep','Hurt','Idle','Swing','Double','Hop','Charge','Rotate']
NAME_TO_ANIM={n:i for i,n in enumerate(ANIMS)};NAME_TO_ANIM['Shake']=4
red_ids={m:int(i) for m,i in re.findall(r'#define MONSTER_(\w+) (\d+)',(Path(redax.HERE)/'monster.h').read_text())}
species={m:int(i) for m,i in re.findall(r'^#define SPECIES_(\w+) (\d+)',(P/'include/constants/species.h').read_text(),re.M)}
NUM_SPECIES=species['EGG']
def render_idx(a,pid,size=256):
    buf=bytearray(size*size);parts=a.pose(pid)
    cur=b''.join(a.gfx(sp) for sp,_,_,_ in parts if sp>=0)
    pal=None
    for sp,f1,f2,f3 in parts:
        if pal is None:pal=(f3>>12)&15
        y=f1&0xFF;y=y-256 if y>=128 else y
        shape=(f1>>14)&3;x=f2&0x1FF;x=x%256-(256 if x%256>=128 else 0)
        sz=(f2>>14)&3;hf=(f2>>12)&1;vf=(f2>>13)&1;tile=f3&0x3FF;w,h=SH[(shape,sz)]
        for py in range(h):
            for px in range(w):
                t=tile+(py//8)*(w//8)+px//8;o=t*32+(py%8)*4+(px%8)//2
                if o>=len(cur):continue
                c=cur[o]>>4 if px&1 else cur[o]&15
                if not c:continue
                X=(w-1-px if hf else px)+x+size//2;Y=(h-1-py if vf else py)+y+size//2
                if 0<=X<size and 0<=Y<size:buf[Y*size+X]=c
    return buf,pal
def bbox(buf,size=256):
    xs=[i%size for i,v in enumerate(buf) if v];ys=[i//size for i,v in enumerate(buf) if v]
    return (min(xs),min(ys),max(xs)+1,max(ys)+1) if xs else None
def to4bpp(cell,S):
    out=bytearray()
    for ty in range(S//8):
        for tx in range(S//8):
            for y in range(8):
                for x in range(0,8,2):
                    i=(ty*8+y)*S+tx*8+x;out.append(cell[i]|cell[i+1]<<4)
    return bytes(out)
def flip(cell,S):return bytes(b for y in range(S) for b in reversed(cell[y*S:(y+1)*S]))
def rgb555(c):return (c[0]>>3)|((c[1]>>3)<<5)|((c[2]>>3)<<10)
def read_jasc(p):
    l=p.read_text().split()[3:];return [tuple(int(v) for v in l[i:i+3]) for i in range(0,len(l),3)]
def shiny_of(pal,folder):
    d=P/'graphics/pokemon'/folder
    if not (d/'normal.pal').exists() or not (d/'shiny.pal').exists():return pal
    n=read_jasc(d/'normal.pal')[1:16];s=read_jasc(d/'shiny.pal')[1:16];out=[pal[0]]
    for c in pal[1:]:
        k=min(range(len(n)),key=lambda i:sum((c[j]-n[i][j])**2 for j in range(3)))
        out.append(tuple(max(0,min(255,s[k][j]+(c[j]-n[k][j])//2)) for j in range(3)))
    return out
banks={}  # red id -> info
SHAPE_TILES={(0,0):(1,1),(0,1):(2,2),(0,2):(4,4),(0,3):(8,8),(1,0):(2,1),(1,1):(4,1),(1,2):(4,2),(1,3):(8,4),(2,0):(1,2),(2,1):(1,4),(2,2):(2,4),(2,3):(4,8)}
def build(rid):
    if rid in banks:return banks[rid]
    a=Ax(f'ax{rid:03d}')
    order=[]
    for k in [7,0,1,6,3,11,10,2,8,9,12,4,5]:
        if k>=a.n:continue
        for d in range(8):
            for f in a.anim(k,d):
                if f['pose']>=0 and f['pose'] not in order:order.append(f['pose'])
    # body extent from Walk + Idle decides the visual centre (anchor)
    core={f['pose'] for k in (0,7) if k<a.n for d in range(8) for f in a.anim(k,d) if f['pose']>=0}
    y0=10**9;y1=-10**9;palno=None;poses={}
    for pid in order:
        parts=a.pose(pid);poses[pid]=parts
        for sp,f1,f2,f3 in parts:
            if palno is None:palno=(f3>>12)&15
            if pid in core:
                y=f1&0xFF;y=y-256 if y>=128 else y;w,h=SH[((f1>>14)&3,(f2>>14)&3)]
                y0=min(y0,y);y1=max(y1,y+h)
    c=(y0+y1)//2 if y1>y0 else -12
    anchor=max(-100,min(100,8-c))
    # sprite gfx pool, packed into <=4 KB blocks in first-use order
    sprite_ids=[];seen=set()
    for pid in order:
        for sp,_,_,_ in poses[pid]:
            if sp>=0 and sp not in seen:seen.add(sp);sprite_ids.append(sp)
    sidx={sp:i for i,sp in enumerate(sprite_ids)};blocks=[];cur=bytearray();stab=[]
    for sp in sprite_ids:
        g=a.gfx(sp)
        if len(cur)+len(g)>4096:blocks.append(bytes(cur));cur=bytearray()
        stab.append((len(blocks),len(cur),len(g)));cur+=g
    if cur or not blocks:blocks.append(bytes(cur))
    lz=[compress(b) for b in blocks]
    # pose recipes (deduplicated): gather sources in order, then copy each part's tile block (flipped if needed)
    maxtiles=0;blob_index={};pose_blobs=[];pmap={}
    for pid in order:
        parts=poses[pid];src=[sidx[sp] for sp,_,_,_ in parts if sp>=0]
        dst=0;recs=[]
        for sp,f1,f2,f3 in parts:
            shape=(f1>>14)&3;size=(f2>>14)&3;tw,th=SHAPE_TILES[(shape,size)]
            y=f1&0xFF;y=y-256 if y>=128 else y;x=f2&0x1FF;x=x%256-(256 if x%256>=128 else 0)
            flip=(f2>>12)&3;tile=f3&0x3FF;sx=x;sy=y-c
            if not(-128<=sx<=127 and -128<=sy<=127) or dst+tw*th>64:continue
            recs.append(struct.pack('<bbBBBB',sx,sy,shape<<2|size,tile,dst,flip));dst+=tw*th
        maxtiles=max(maxtiles,dst)
        blob=struct.pack('<BBBB',len(src),len(recs),dst,0)+b''.join(struct.pack('<H',v) for v in src)+b''.join(recs)
        if len(blob)%2:blob+=b'\0'
        if blob not in blob_index:blob_index[blob]=len(pose_blobs);pose_blobs.append(blob)
        pmap[pid]=blob_index[blob]
    S=32 if maxtiles<=16 else 64
    wide=len(pose_blobs)>255
    anim_recs=[None]*13
    for k in range(min(a.n,13)):
        dirs=[a.anim(k,d) for d in range(8)]
        if not any(dirs):continue
        base=next(x for x in dirs if x);n=len(base)
        has_off=any(f['off']!=(0,0) for x in dirs for f in x)
        mask=sum(1<<d for d in range(8) if dirs[d])
        rec=bytearray(struct.pack('<BBBB',n,1 if has_off else 0,mask,0))
        for f in base:rec+=struct.pack('<BB',max(1,min(255,f['dur'])),f['flags']&3)
        for d in range(8):
            if not dirs[d]:continue
            fr=(dirs[d]+[dirs[d][-1]]*n)[:n]
            for f in fr:
                v=pmap.get(f['pose'],0)
                rec+=struct.pack('<H',v) if wide else struct.pack('<B',v)
            if has_off:
                for f in fr:rec+=struct.pack('<bb',max(-128,min(127,f['off'][0])),max(-128,min(127,f['off'][1])))
        while len(rec)%4:rec.append(0)
        anim_recs[k]=bytes(rec)
    nb=len(blocks);ns=len(stab);npz=len(pose_blobs)
    head=12+13*4+4*nb+6*ns+2*npz
    while head%4:head+=1
    body=bytearray();anim_off=[0]*13
    for k in range(13):
        if anim_recs[k]:anim_off[k]=head+len(body);body+=anim_recs[k]
    pose_region=head+len(body);pose_off=[]
    for b in pose_blobs:pose_off.append(len(body)+head-pose_region);body+=b
    assert max(pose_off+[0])<65536
    while len(body)%4:body.append(0)
    block_off=[]
    for z in lz:block_off.append(head+len(body));body+=z
    out=struct.pack('<BbBBHHBBH',S,anchor,13,nb,npz,ns,2 if wide else 1,0,0)
    out+=b''.join(struct.pack('<I',o) for o in anim_off)
    out+=b''.join(struct.pack('<I',o) for o in block_off)
    out+=b''.join(struct.pack('<BBHH',bi,0,off,ln) for bi,off,ln in stab)
    out+=b''.join(struct.pack('<H',o) for o in pose_off)
    while len(out)%4:out+=b'\0'
    out=bytearray(out);struct.pack_into('<H',out,10,pose_region-0)  # pose region offset (fits: header+anims small)
    out=bytes(out)+bytes(body)
    assert len(out)==head+len(body),(len(out),head,len(body))
    while len(out)%4:out+=b'\0'
    pal=palette(palno or 0)
    info=dict(rid=rid,size=S,anchor=anchor,center=c,bytes=len(out),frames=npz,wide=wide,pal=[p[:3] for p in pal],data=bytes(out),a=a,maxtiles=maxtiles)
    banks[rid]=info;return info
def red_for(name):
    alias={'NIDORAN_F':'NIDORAN_F','NIDORAN_M':'NIDORAN_M','UNOWN':'UNOWN_A','DEOXYS':'DEOXYS_NORMAL','HO_OH':'HO_OH','MR_MIME':'MR_MIME'}
    n=alias.get(name,name)
    return red_ids.get(n)
keys=[None]  # key -> red id
key_of={}
def key_for(rid):
    if rid not in key_of:key_of[rid]=len(keys);keys.append(rid)
    return key_of[rid]
species_keys=[0]*NUM_SPECIES;missing=[]
for name,sid in sorted(species.items(),key=lambda x:x[1]):
    if sid<=0 or sid>=NUM_SPECIES or name.startswith('OLD_UNOWN'):continue
    rid=red_for(name)
    if rid is None or f'ax{rid:03d}' not in IDX:missing.append(name);continue
    species_keys[sid]=key_for(rid)
letters=[chr(65+i) for i in range(26)]+['EMARK','QMARK']
unown_keys=[key_for(red_ids['UNOWN_'+l]) for l in letters]
folders={}
for name,sid in species.items():
    if 0<sid<NUM_SPECIES:folders[sid]=name.lower()
lines=['// Generated by tools/import_pmd_mons.py from PMD Red Rescue Team (private use).',
 'struct DsMonGfx { const u32 *bank; const u16 *normal, *shiny; u8 size; s8 anchorY; };']
muzzles=[]
total=0
for key,rid in enumerate(keys):
    if rid is None:continue
    b=build(rid);total+=b['bytes']
    fname=f'mon_{rid:03d}.bin';(OUT/fname).write_bytes(b['data'])
    sid=next((s for s,k in enumerate(species_keys) if k==key),None)
    sh=shiny_of(b['pal'],folders.get(sid,'')) if sid else b['pal']
    lines.append(f'static const u32 sPmdBank_{key}[] = INCBIN_U32("graphics/pmd_mons/{fname}");')
    lines.append(f'static const u16 sPmdPal_{key}_0[16] = {{'+','.join(hex(rgb555(c)) for c in b['pal'])+'};')
    lines.append(f'static const u16 sPmdPal_{key}_1[16] = {{'+','.join(hex(rgb555(c)) for c in sh)+'};')
    # Muzzle points (down, up, left, right) from the idle pose silhouette, in cell coordinates
    S=b['size'];row=[];a=b['a']
    for d in (0,4,6,2):
        fr=a.anim(7,d) if a.n>7 else []
        bb=None
        if fr:
            buf,_=render_idx(a,fr[0]['pose']);bb=bbox(buf)
        if not bb:row.append((S//2,S//2));continue
        cy=S//2+b['center']  # ground y inside the cell
        left,top,right,bottom=bb[0]-128+S//2,bb[1]-128-b['center']+S//2,bb[2]-128+S//2,bb[3]-128-b['center']+S//2
        band_bottom=top+(bottom-top)*45//100;mid=(top+band_bottom)//2;cx=(left+right)//2
        pt={0:(cx,band_bottom),4:(cx,top+2),6:(left+2,mid),2:(right-2,mid)}[d]
        row.append(tuple(max(0,min(S-1,v)) for v in pt))
    muzzles.append((key,rid,row))
lines.append('static const struct DsMonGfx sDsGfx[] = {{0},')
for key,rid in enumerate(keys):
    if rid is None:continue
    b=banks[rid];lines.append(f' {{sPmdBank_{key},sPmdPal_{key}_0,sPmdPal_{key}_1,{b["size"]},{b["anchor"]}}}, // red {rid}')
lines.append('};')
lines.append('static const u16 sDsSpeciesKeys[NUM_SPECIES] = {'+','.join(map(str,species_keys))+'};')
lines.append('static const u16 sDsUnownKeys[28] = {'+','.join(map(str,unown_keys))+'};')
# Body animation per move (Explorers of Sky move data, mapped to the 13 shared PMD animations).
mv=json.loads((P/'tools/pmd/moves-fire-red-emerald.json').read_text())['moves']
move_ids={m:int(i) for m,i in re.findall(r'^#define MOVE_(\w+)\s+(\d+)',(P/'include/constants/moves.h').read_text(),re.M)}
norm=lambda s:re.sub('[^a-z0-9]','',s.lower())
mid_by_norm={norm(k):v for k,v in move_ids.items()}
alias={'faintattack':'feintattack','hijumpkick':'highjumpkick','smellingsalt':'smellingsalts'}
table={};overrides=[]
spec_by_norm={norm(k):v for k,v in species.items()}
for m in mv:
    k=norm(m['name']);mid=mid_by_norm.get(k) or mid_by_norm.get(alias.get(k,''))
    if mid is None:continue
    table[mid]=NAME_TO_ANIM.get(m['sky']['body_animation'],1)
    for o in m['sky']['species_overrides']:
        sid=spec_by_norm.get(norm(o['species'] or ''))
        if sid and 0<sid<NUM_SPECIES and o['body_animation'] in NAME_TO_ANIM:overrides.append((mid,sid,NAME_TO_ANIM[o['body_animation']]))
mx=max(move_ids.values())+1
lines.append('const u8 gPmdMoveBodyAnim[MOVES_COUNT] = {'+','.join(str(table.get(i,1)) for i in range(mx))+'};')
ov=sorted(set(overrides))
lines.append('const u16 gPmdMoveAnimOverrides[][3] = {'+','.join('{%d,%d,%d}'%o for o in ov)+',{0,0,0}};')
(P/'src/data/pmd_mon_graphics.h').write_text('\n'.join(lines)+'\n',encoding='utf-8',newline='\n')
em=['// Generated by tools/import_pmd_mons.py. Coordinates within standing poses (down, up, left, right).','static const u8 sPtdMuzzles[][4][2] = {{{0}} ,']
for key,rid,row in muzzles:em.append(' {'+','.join('{%d,%d}'%p for p in row)+'}, // red %d'%rid)
em.append('};')
(P/'src/data/realtime_ptd_emitters.h').write_text('\n'.join(em)+'\n',encoding='utf-8',newline='\n')
print(json.dumps({'keys':len(keys)-1,'bytes':total,'MB':round(total/2**20,2),'missing_species':missing,'overrides':len(ov)},indent=1))
