"""Fly atlas v3: v2 layout plus natural transitions (beaches + foam at coasts, cliff edges where
forest meets mountains, rounded canopy edges), drawn at 8 px per metatile then reduced."""
import sys,struct,collections
from pathlib import Path
sys.path.insert(0,str(Path(__file__).parent))
from render import render_map,MapRenderer,_layouts,P
from place import components,maps,size,world_layout
from atlas2 import region_classes,fit,material_tiles,noise,MARGIN
import numpy as np
from PIL import Image,ImageFilter
from scipy import ndimage as nd
NEAR=3;S=8
def border_class(tiles):
    a=np.asarray(tiles.convert('RGB')).reshape(-1,3).mean(0)
    if a[2]>a[1] and a[2]>a[0]:return 0   # sea
    if a[0]>a[1]:return 2                  # mountain/rock
    return 1                               # forest/grass
def build(comp):
    xs=[x for x,y in comp.values()];ys=[y for x,y in comp.values()]
    x0=min(xs)-MARGIN;y0=min(ys)-MARGIN
    x1=max(x+size(m)[0] for m,(x,y) in comp.items())+MARGIN;y1=max(y+size(m)[1] for m,(x,y) in comp.items())+MARGIN
    W,H=x1-x0,y1-y0;names=list(comp)
    owner=-np.ones((H,W),np.int32)
    for i,m in enumerate(names):
        x,y=comp[m];w,h=size(m);owner[y-y0:y-y0+h,x-x0:x-x0+w]=i
    filled=owner>=0
    dist,(iy,ix)=nd.distance_transform_edt(~filled,return_indices=True);own=owner[iy,ix]
    # region scores at tile resolution
    rimg,cls=region_classes();ax,ay=fit(comp)
    gx=np.arange(W)+x0;gy=np.arange(H)+y0
    pxf=(ax[0]*gx[None,:]+ax[1])+(noise(H,W,24,1)*2-1)*5;pyf=(ay[0]*gy[:,None]+ay[1])+(noise(H,W,24,2)*2-1)*5
    outside=(pxf<34)|(pxf>206)|(pyf<34)|(pyf>150)   # beyond the Town Map's land area: open sea
    px=np.clip(pxf,0,239).astype(int);py=np.clip(pyf,0,159).astype(int)
    keys=['sea','forest','mountain'];score=[]
    for k in keys:
        b=np.asarray(Image.fromarray((cls[k]*255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(3))).astype(np.float32)
        v=b[py,px]
        v=np.where(outside,255.0 if k=='sea' else 0.0,v)
        score.append(v)
    score=np.stack(score)
    # near real maps the land type follows that map's own border so edges connect
    mats=material_tiles();binfo=[]
    for i,m in enumerate(names):
        l=_layouts[maps[m]['layout']];r=MapRenderer(l['primary_tileset'],l['secondary_tileset'])
        bd=(P/l['border_filepath']).read_bytes();bw,bh=l['border_width'],l['border_height']
        border=[struct.unpack_from('<H',bd,2*k)[0]&0x3FF for k in range(bw*bh)]
        bimg=Image.new('RGB',(bw*16,bh*16))
        for k,b in enumerate(border):bimg.paste(r.metatile(b),((k%bw)*16,(k//bw)*16))
        binfo.append((r,border,bw,bh,border_class(bimg)))
    near=(~filled)&(dist<=NEAR+6)
    bc=np.array([b[4] for b in binfo])[np.maximum(own,0)]
    for c in range(3):
        boost=np.where(near&(bc==c),400.0,0.0)*np.clip(1-(dist-NEAR)/6,0,1)
        score[c]+=boost
    # upsample class scores to pixels with fine noise -> organic boundaries
    PW,PH=W*S,H*S
    def up(a):return np.asarray(Image.fromarray(a.astype(np.float32),mode='F').resize((PW,PH),Image.BICUBIC))
    fine=up(noise(H,W,3,7)*2-1)*40
    sc=np.stack([up(score[c])+(fine if c==1 else 0) for c in range(3)])
    cpx=sc.argmax(0).astype(np.uint8)
    del sc
    tilefree=np.asarray(Image.fromarray(((~filled)&(dist>NEAR+8)).astype(np.uint8)*255).resize((PW,PH),Image.NEAREST))>0
    # decorative islands in open sea (fly-over only)
    rng=np.random.default_rng(11);seaT=np.asarray(Image.fromarray((cpx==0).astype(np.uint8)*255).resize((W,H),Image.NEAREST))>0
    farsea=nd.distance_transform_edt(seaT)
    islands=[]
    for _ in range(400):
        tx,ty=int(rng.integers(0,W)),int(rng.integers(0,H))
        if farsea[ty,tx]>26 and all((tx-a)**2+(ty-b)**2>60**2 for a,b,_ in islands):
            islands.append((tx,ty,int(rng.integers(7,15))))
        if len(islands)>=5:break
    yy,xx=np.mgrid[0:PH,0:PW]
    wob=np.asarray(Image.fromarray((noise(H,W,6,21)*255).astype(np.uint8)).resize((PW,PH),Image.BICUBIC)).astype(np.float32)/255
    for tx,ty,r in islands:
        d=np.sqrt(((xx-tx*S)/(r*S))**2+((yy-ty*S)/(r*S*0.8))**2)+(wob-0.5)*0.6
        cpx[d<1]=1
        if r>10:cpx[d<0.35]=2
    # remove specks: small patches take the class around them
    for c in (0,1,2):
        lab,n=nd.label(cpx==c);sizes=nd.sum(np.ones_like(lab),lab,range(1,n+1))
        small=np.isin(lab,np.nonzero(sizes<S*S*10)[0]+1)
        if small.any():
            _,(iy2,ix2)=nd.distance_transform_edt(small,return_indices=True);cpx[small]=cpx[iy2[small],ix2[small]]
    # clearings (fly-over only): plain field grass inside big forests, bare rocky ground in the mountains
    rng3=np.random.default_rng(17)
    for cls_id,src,count,rmin,rmax in ((1,3,7,4,8),(2,4,5,5,10)):
        inner2=nd.distance_transform_edt((cpx==src if False else cpx==cls_id)&tilefree)
        cand=np.argwhere(inner2>(rmax+4)*S);rng3.shuffle(cand);placed=[]
        for y_,x_ in cand:
            if all((x_-a)**2+(y_-b)**2>(45*S)**2 for a,b in placed):placed.append((x_,y_))
            if len(placed)>=count:break
        for x_,y_ in placed:
            r=int(rng3.integers(rmin,rmax))*S
            y0b,y1b,x0b,x1b=max(0,y_-2*r),min(PH,y_+2*r),max(0,x_-2*r),min(PW,x_+2*r)
            sy,sx=np.mgrid[y0b:y1b,x0b:x1b]
            d=np.sqrt(((sx-x_)/r)**2+((sy-y_)/(r*0.75))**2)+(wob[y0b:y1b,x0b:x1b]-0.5)*0.7
            blk=cpx[y0b:y1b,x0b:x1b];blk[(d<1)&(blk==cls_id)]=src
    # foothills: trees thin out onto the rock over ~3 metatiles
    dforest=nd.distance_transform_edt(cpx!=1)
    bn=np.asarray(Image.fromarray((noise(H*2,W*2,1,5)*255).astype(np.uint8)).resize((PW,PH),Image.NEAREST)).astype(np.float32)/255
    foot=(cpx==2)&(dforest<3*S)&(bn>dforest/(3*S))&tilefree
    # textures: material 2x2 metatile pattern at S px per metatile
    tex=[]
    for k in keys:
        t=Image.new('RGB',(32,32))
        for q in range(4):t.paste(mats[k][q],((q%2)*16,(q//2)*16))
        tex.append(np.asarray(t.resize((2*S,2*S),Image.LANCZOS)))
    for m_,ids in (('MAP_ROUTE1',(0x8,0x10,0x11,0x9)),('MAP_ROUTE3',(0xd9,0xd9,0xd9,0x6c))):
        l=_layouts[maps[m_]['layout']];r=MapRenderer(l['primary_tileset'],l['secondary_tileset']);t=Image.new('RGB',(32,32))
        for q,mid in enumerate(ids):t.paste(r.metatile(mid),((q%2)*16,(q//2)*16))
        tex.append(np.asarray(t.resize((2*S,2*S),Image.LANCZOS)))
    out=np.zeros((PH,PW,3),np.uint8)
    for c in range(5):
        m=cpx==c;out[m]=tex[c][yy[m]%(2*S),xx[m]%(2*S)]
    # mountains: light from the top-left on a ridge heightmap
    hm=sum(np.asarray(Image.fromarray((noise(H,W,sc_,30+sc_)*255).astype(np.uint8)).resize((PW,PH),Image.BICUBIC)).astype(np.float32)/255*w_ for sc_,w_ in ((20,1.0),(9,0.35)))
    gy_,gx_=np.gradient(hm);shade=np.clip(1.0-(gx_+gy_)*S*3.5,0.84,1.12)
    m=cpx==2;out[m]=np.clip(out[m]*shade[m][:,None],0,255).astype(np.uint8)
    m=foot;out[m]=tex[1][yy[m]%(2*S),xx[m]%(2*S)]
    del yy,xx
    sea=cpx==0;land=~sea;forest=(cpx==1)|foot;mount=(cpx==2)&~foot
    dsea=nd.distance_transform_edt(land);dland=nd.distance_transform_edt(sea)
    sand=np.array([239,222,156]);sand2=np.array([206,181,115]);foam=np.array([235,245,255]);shallow=np.array([90,160,235])
    b=land&(dsea<=3.5);out[b]=sand
    b=land&(dsea>3.5)&(dsea<=4.6);out[b]=sand2
    b=sea&(dland<=1.2);out[b]=foam
    b=sea&(dland>1.2)&(dland<=4);out[b]=(out[b]*0.55+shallow*0.45).astype(np.uint8)
    # cliffs where mountain meets forest: shade the mountain side, darker toward the edge
    dfor=nd.distance_transform_edt(~forest)
    for d,f in ((1.5,0.45),(3,0.6),(5,0.75),(7,0.88)):
        b=mount&(dfor<=d)&(dfor>d-2);out[b]=(out[b]*f).astype(np.uint8)
    # canopy outline: a dark rim where forest ends
    dnf=nd.distance_transform_edt(forest)
    b=forest&(dnf<=1.2);out[b]=(out[b]*0.55).astype(np.uint8)
    img=Image.fromarray(out)
    # decorative villages (fly-over only): real FireRed houses in clearings deep in big forests / on islands
    spots=[];inner=nd.distance_transform_edt((cpx==1)&tilefree)
    cand=np.argwhere(inner>14*S)
    cand=cand[(cand[:,0]>30*S)&(cand[:,0]<PH-30*S)&(cand[:,1]>30*S)&(cand[:,1]<PW-30*S)]
    rng2=np.random.default_rng(5);rng2.shuffle(cand)
    for y_,x_ in cand:
        if all((x_-a)**2+(y_-b)**2>(70*S)**2 for a,b in spots):spots.append((x_,y_))
        if len(spots)>=4:break
    VILLAGES=False  # decorative towns removed (copied town slices looked fake)
    for k,(cx_,cy_) in enumerate(spots if VILLAGES else []):
        town=['LAYOUT_PALLET_TOWN','LAYOUT_LAVENDER_TOWN','LAYOUT_VIRIDIAN_CITY','LAYOUT_FUCHSIA_CITY'][k%4]
        tim,tw_,th_=render_map(town)
        crop={'LAYOUT_PALLET_TOWN':(2,2,22,15),'LAYOUT_LAVENDER_TOWN':(2,4,18,16),'LAYOUT_VIRIDIAN_CITY':(4,6,24,20),'LAYOUT_FUCHSIA_CITY':(6,4,26,16)}[town]
        patch=tim.crop(tuple(v*16 for v in crop));patch=patch.resize((patch.width*S//16,patch.height*S//16),Image.LANCZOS)
        if k%2:patch=patch.transpose(Image.FLIP_LEFT_RIGHT)
        # soft-edged clearing so the village sits inside the forest
        mask=Image.new('L',patch.size,0);from PIL import ImageDraw
        ImageDraw.Draw(mask).ellipse((0,0,patch.width-1,patch.height-1),fill=255);mask=mask.filter(ImageFilter.GaussianBlur(S))
        img.paste(patch,(cx_-patch.width//2,cy_-patch.height//2),mask)
    # real maps (and their immediate borders) drawn on top unchanged
    for i,m in enumerate(names):
        mx,my=comp[m];im,w,h=render_map(maps[m]['layout'])
        img.paste(im.resize((w*S,h*S),Image.LANCZOS),((mx-x0)*S,(my-y0)*S))
        r,border,bw,bh,_=binfo[i];cache={}
        for ty,tx in zip(*np.nonzero((own==i)&~filled&(dist<=NEAR))):
            bb=border[((ty+y0-my)%bh)*bw+((tx+x0-mx)%bw)]
            if bb not in cache:cache[bb]=r.metatile(bb).resize((S,S),Image.LANCZOS)
            img.paste(cache[bb],(tx*S,ty*S))
    return img,(x0,y0,W,H)
if __name__=='__main__':
    comp=world_layout()
    img,box=build(comp);img.save('/tmp/kanto8.png');img.resize((img.width//2,img.height//2),Image.LANCZOS).save('/tmp/kanto_fill3.png');print(img.size)
