"""Fly atlas: original outdoor maps placed by their connections; empty space filled with each
nearest map's own border pattern (what the game shows past that map's edge)."""
import sys,struct,collections
from pathlib import Path
sys.path.insert(0,str(Path(__file__).parent))
from render import render_map,MapRenderer,_layouts,P
from place import components,maps,size
import numpy as np
from PIL import Image
MARGIN=24
def build(comp,S=16):
    xs=[x for x,y in comp.values()];ys=[y for x,y in comp.values()]
    x0=min(xs)-MARGIN;y0=min(ys)-MARGIN
    x1=max(x+size(m)[0] for m,(x,y) in comp.items())+MARGIN;y1=max(y+size(m)[1] for m,(x,y) in comp.items())+MARGIN
    W,H=x1-x0,y1-y0
    owner=-np.ones((H,W),np.int32);names=list(comp)
    for i,m in enumerate(names):
        x,y=comp[m];w,h=size(m);owner[y-y0:y-y0+h,x-x0:x-x0+w]=i
    filled=owner>=0
    # multi-source BFS (Voronoi) from every map cell
    q=collections.deque((yy,xx) for yy,xx in zip(*np.nonzero(filled)))
    own=owner.copy()
    while q:
        yy,xx=q.popleft()
        for dy,dx in ((1,0),(-1,0),(0,1),(0,-1)):
            ny,nx=yy+dy,xx+dx
            if 0<=ny<H and 0<=nx<W and own[ny,nx]<0:own[ny,nx]=own[yy,xx];q.append((ny,nx))
    img=Image.new('RGB',(W*S,H*S))
    for i,m in enumerate(names):
        l=_layouts[maps[m]['layout']];r=MapRenderer(l['primary_tileset'],l['secondary_tileset'])
        bd=(P/l['border_filepath']).read_bytes();bw,bh=l['border_width'],l['border_height']
        border=[struct.unpack_from('<H',bd,2*k)[0]&0x3FF for k in range(bw*bh)]
        mx,my=comp[m]
        im,w,h=render_map(maps[m]['layout'])
        if S!=16:im=im.resize((w*S,h*S),Image.LANCZOS)
        img.paste(im,((mx-x0)*S,(my-y0)*S))
        cache={}
        for yy,xx in zip(*np.nonzero((own==i)&~filled)):
            gx,gy=xx+x0-mx,yy+y0-my;b=border[(gy%bh)*bw+(gx%bw)]
            if b not in cache:
                t=r.metatile(b);cache[b]=t if S==16 else t.resize((S,S),Image.LANCZOS)
            img.paste(cache[b],(xx*S,yy*S))
    return img,(x0,y0,W,H),own,filled
if __name__=='__main__':
    comp=max(components(),key=len)
    img,box,own,filled=build(comp,S=4);img.save('/tmp/kanto_fill.png');print(img.size,box)
