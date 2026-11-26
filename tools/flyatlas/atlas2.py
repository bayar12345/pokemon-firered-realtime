"""Fly atlas v2: real maps at their connected positions; the land between them follows FireRed's own
Town Map (sea / forest / mountains), drawn with the game's own border tiles."""
import sys,struct,collections,re,json
from pathlib import Path
sys.path.insert(0,str(Path(__file__).parent))
from render import render_map,MapRenderer,_layouts,P
from place import components,maps,size
import numpy as np
from PIL import Image,ImageFilter
MARGIN=40;NEAR=3
def region_classes():
    t=Image.open(P/'graphics/region_map/region_map.png');pal=t.getpalette()
    tm=(P/'graphics/region_map/kanto.bin').read_bytes();tw=t.width//8
    img=Image.new('RGB',(240,160))
    for i in range(600):
        v=struct.unpack_from('<H',tm,2*i)[0];ti=v&0x3FF;pl=v>>12
        tile=t.crop(((ti%tw)*8,(ti//tw)*8,(ti%tw)*8+8,(ti//tw)*8+8))
        d=[(pl*16+(p%16)) if p else 0 for p in tile.getdata()];tile=Image.new('P',(8,8));tile.putdata(d);tile.putpalette(pal);tile=tile.convert('RGB')
        if v&0x400:tile=tile.transpose(Image.FLIP_LEFT_RIGHT)
        if v&0x800:tile=tile.transpose(Image.FLIP_TOP_BOTTOM)
        img.paste(tile,((i%30)*8,(i//30)*8))
    a=np.asarray(img).astype(int);r,g,b=a[...,0],a[...,1],a[...,2]
    sea=(b>g)&(b>r)
    path=(r>180)&(g>140)&(b<120)                      # route lines: treat as land
    light=(g>200)&~sea&~path                            # bright highlands
    land=~sea
    mountain=light
    forest=land&~mountain
    return img,{'sea':sea,'forest':forest,'mountain':mountain}
def mapsec_cells():
    s=(P/'src/data/region_map/region_map_layout_kanto.h').read_text().split('[LAYER_DUNGEON]')[0]
    rows=[re.findall(r'MAPSEC_\w+',r) for r in re.findall(r'\{(MAPSEC[^}]*)\}',s)]
    cells=collections.defaultdict(list)
    for y,r in enumerate(rows):
        for x,v in enumerate(r):cells[v].append((x,y))
    return cells
def fit(comp):
    cells=mapsec_cells();A=[];BX=[];BY=[]
    for m,(x,y) in comp.items():
        sec=maps[m]['region_map_section'];c=cells.get(sec)
        if not c or sec=='MAPSEC_NONE':continue
        w,h=size(m);cx=sum(p[0] for p in c)/len(c);cy=sum(p[1] for p in c)/len(c)
        A.append((x+w/2,y+h/2));BX.append((cx+4)*8+4);BY.append((cy+4)*8+4)
    A=np.array(A);ax=np.polyfit(A[:,0],BX,1);ay=np.polyfit(A[:,1],BY,1)
    return ax,ay
def material_tiles():
    out={}
    for name,m in (('sea','MAP_ROUTE19'),('forest','MAP_ROUTE2'),('mountain','MAP_ROUTE3')):
        l=_layouts[maps[m]['layout']];r=MapRenderer(l['primary_tileset'],l['secondary_tileset'])
        bd=(P/l['border_filepath']).read_bytes()
        out[name]=[r.metatile(struct.unpack_from('<H',bd,2*k)[0]&0x3FF) for k in range(4)]
    return out
def noise(H,W,scale,seed):
    rng=np.random.default_rng(seed);small=rng.random((H//scale+2,W//scale+2)).astype(np.float32)
    im=Image.fromarray((small*255).astype(np.uint8)).resize((W+scale*2,H+scale*2),Image.BICUBIC)
    return np.asarray(im)[:H,:W].astype(np.float32)/255
def build(comp,S=16):
    xs=[x for x,y in comp.values()];ys=[y for x,y in comp.values()]
    x0=min(xs)-MARGIN;y0=min(ys)-MARGIN
    x1=max(x+size(m)[0] for m,(x,y) in comp.items())+MARGIN;y1=max(y+size(m)[1] for m,(x,y) in comp.items())+MARGIN
    W,H=x1-x0,y1-y0;names=list(comp)
    owner=-np.ones((H,W),np.int32)
    for i,m in enumerate(names):
        x,y=comp[m];w,h=size(m);owner[y-y0:y-y0+h,x-x0:x-x0+w]=i
    filled=owner>=0
    # distance to nearest map + nearest owner (BFS)
    own=owner.copy();dist=np.where(filled,0,10**6)
    q=collections.deque((yy,xx) for yy,xx in zip(*np.nonzero(filled)))
    while q:
        yy,xx=q.popleft()
        for dy,dx in ((1,0),(-1,0),(0,1),(0,-1)):
            ny,nx=yy+dy,xx+dx
            if 0<=ny<H and 0<=nx<W and own[ny,nx]<0:own[ny,nx]=own[yy,xx];dist[ny,nx]=dist[yy,xx]+1;q.append((ny,nx))
    # region-map class per atlas tile, smoothed and wobbled so coastlines look natural
    rimg,cls=region_classes();ax,ay=fit(comp)
    gx=np.arange(W)+x0;gy=np.arange(H)+y0
    nx_=noise(H,W,24,1)*2-1;ny_=noise(H,W,24,2)*2-1
    px=(ax[0]*gx[None,:]+ax[1])+nx_*5;py=(ay[0]*gy[:,None]+ay[1])+ny_*5
    px=np.clip(px,0,239).astype(int);py=np.clip(py,0,159).astype(int)
    score={}
    for k,mask in cls.items():
        blurred=np.asarray(Image.fromarray((mask*255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(3))).astype(np.float32)
        score[k]=blurred[py,px]
    keys=list(score);stack=np.stack([score[k] for k in keys]);region=np.array(keys)[stack.argmax(0)]
    mats=material_tiles()
    img=Image.new('RGB',(W*S,H*S))
    rcache={}
    for i,m in enumerate(names):
        l=_layouts[maps[m]['layout']];r=MapRenderer(l['primary_tileset'],l['secondary_tileset'])
        bd=(P/l['border_filepath']).read_bytes();bw,bh=l['border_width'],l['border_height']
        border=[struct.unpack_from('<H',bd,2*k)[0]&0x3FF for k in range(bw*bh)]
        mx,my=comp[m];im,w,h=render_map(maps[m]['layout'])
        img.paste(im if S==16 else im.resize((w*S,h*S),Image.LANCZOS),((mx-x0)*S,(my-y0)*S))
        for yy,xx in zip(*np.nonzero((own==i)&~filled&(dist<=NEAR))):
            b=border[((yy+y0-my)%bh)*bw+((xx+x0-mx)%bw)];key=(i,b)
            if key not in rcache:t=r.metatile(b);rcache[key]=t if S==16 else t.resize((S,S),Image.LANCZOS)
            img.paste(rcache[key],(xx*S,yy*S))
    mcache={}
    for yy,xx in zip(*np.nonzero(~filled&(dist>NEAR))):
        k=region[yy,xx];t=mats[k][((yy+y0)%2)*2+((xx+x0)%2)];key=(k,id(t))
        if key not in mcache:mcache[key]=t if S==16 else t.resize((S,S),Image.LANCZOS)
        img.paste(mcache[key],(xx*S,yy*S))
    return img,(x0,y0,W,H)
if __name__=='__main__':
    comp=max(components(),key=len)
    img,box=build(comp,S=4);img.save('/tmp/kanto_fill2.png');print(img.size,box)
