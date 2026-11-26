import struct, sys
from PIL import Image
import os
HERE=os.path.dirname(os.path.abspath(__file__))
ROM=os.environ.get('PMD_RED_ROM','')
R=open(ROM,'rb').read()
def u32(o):return struct.unpack_from('<I',R,o)[0]
def s16(o):return struct.unpack_from('<h',R,o)[0]
def u16(o):return struct.unpack_from('<H',R,o)[0]
def P(p):return p&0x1FFFFFF if p>>24 in (8,9) else None
IDX={l.split()[0]:int(l.split()[1],16)&0x1FFFFFF for l in open(''+HERE+'/red_monster_index.txt')}
pal_o=IDX['palet']
def palette(n):
    o=pal_o+n*0x40; return [(R[o+4*i],R[o+4*i+1],R[o+4*i+2],0 if i==0 else 255) for i in range(16)]
SH={(0,0):(8,8),(0,1):(16,16),(0,2):(32,32),(0,3):(64,64),(1,0):(16,8),(1,1):(32,8),(1,2):(32,16),(1,3):(64,32),(2,0):(8,16),(2,1):(8,32),(2,2):(16,32),(2,3):(32,64)}
class Ax:
    def __init__(s,name):
        f=IDX[name]; assert R[f:f+4]==b'SIRO'; m=P(u32(f+4))
        s.poses_p,s.anims_p,s.n,s.spr_p,s.pos_p=P(u32(m)),P(u32(m+4)),u32(m+8),P(u32(m+12)),P(u32(m+16))
    def pose(s,i):
        o=P(u32(s.poses_p+4*i)); parts=[]
        while True:
            sp=s16(o); f1,f2,f3=u16(o+4),u16(o+6),u16(o+8)
            if f1==0xFFFF and f2==0xFFFF: break
            parts.append((sp,f1,f2,f3)); o+=10
        return parts
    def gfx(s,sp):
        o=P(u32(s.spr_p+4*sp)); b=b''
        while True:
            g,c=u32(o),u32(o+4)
            if g==0 and c==0: break
            b+= bytes(c) if g==0 else R[P(g):P(g)+c]; o+=8
        return b
    def anim(s,a,d):
        t=P(u32(s.anims_p+4*a)); 
        if t is None: return []
        o=P(u32(t+4*d))
        if o is None: return []
        fr=[]
        while True:
            n,fl,pid=R[o],R[o+1],s16(o+2)
            if n==0: break
            fr.append(dict(dur=n,flags=fl,pose=pid,off=(s16(o+4),s16(o+6)),shadow=(s16(o+8),s16(o+10)))); o+=12
        return fr
    def render(s,pid,size=64):
        im=Image.new('RGBA',(size,size),(0,0,0,0)); parts=s.pose(pid)
        cur=b''.join(s.gfx(sp) for sp,_,_,_ in parts if sp>=0)
        for sp,f1,f2,f3 in parts:
            y=f1&0xFF; y=y-256 if y>=128 else y
            shape=(f1>>14)&3; x=f2&0x1FF; x=x-512 if x>=256 else x
            # positions are around 256 base
            sz=(f2>>14)&3; hf=(f2>>12)&1; vf=(f2>>13)&1
            tile=f3&0x3FF; pal=palette((f3>>12)&0xF)
            w,h=SH[(shape,sz)]
            part=Image.new('RGBA',(w,h),(0,0,0,0)); px=part.load()
            for ty in range(h//8):
                for tx in range(w//8):
                    t=tile+ty*(w//8)+tx; base=t*32
                    for yy in range(8):
                        for xx in range(4):
                            if base+yy*4+xx>=len(cur): continue
                            b=cur[base+yy*4+xx]
                            for k,c in ((0,b&15),(1,b>>4)):
                                if c: px[tx*8+xx*2+k,ty*8+yy]=pal[c]
            if hf: part=part.transpose(Image.FLIP_LEFT_RIGHT)
            if vf: part=part.transpose(Image.FLIP_TOP_BOTTOM)
            im.alpha_composite(part,(x+size//2 - (256 if x>100 else 0) if False else x%256 - (256 if x%256>=128 else 0)+size//2, y+size//2))
        return im

EIDX={l.split()[0]:int(l.split()[1],16)&0x1FFFFFF for l in open(''+HERE+'/red_effect_index.txt')}
class Efo(Ax):
    def __init__(s,name):
        f=EIDX[name]; assert R[f:f+4]==b'SIRO'; m=P(u32(f+4))
        s.poses_p,s.anims_p,s.n=P(u32(m)),P(u32(m+4)),u32(m+8)
        s.spr_p=P(u32(m+12)); s.p256=P(u32(m+16)); s.tiles=P(u32(m+20)); s.pal=P(u32(m+24)); s.tilecount=u32(m+28)
        s.m=m

def efo_palette(e,n=0):
    o=e.pal+n*0x40; return [(R[o+4*i],R[o+4*i+1],R[o+4*i+2],0 if i==0 else 255) for i in range(16)]
def efo_render(e,pid,size=128,paln=None,shared=None):
    im=Image.new('RGBA',(size,size),(0,0,0,0)); cur=R[e.tiles:e.tiles+e.tilecount*32]
    for sp,f1,f2,f3 in e.pose(pid):
        y=f1&0xFF; y=y-256 if y>=96 else y  # hardware wraps: low pieces near the 8-bit edge sit above the target
        shape=(f1>>14)&3; x=f2&0x1FF; x=x-512 if x>=256 else x
        sz=(f2>>14)&3; hf=(f2>>12)&1; vf=(f2>>13)&1
        tile=f3&0x3FF
        # Pieces flagged in OAM attribute 1 (bit 8) use the shared Pokemon palette slot named by palNum.
        pal=palette((f3>>12)&15) if (f1&0x100) else palette(shared) if shared is not None else efo_palette(e,0 if paln is None else paln)
        w,h=SH[(shape,sz)]
        part=Image.new('RGBA',(w,h),(0,0,0,0)); px=part.load()
        for ty in range(h//8):
            for tx in range(w//8):
                base=(tile+ty*(w//8)+tx)*32
                for yy in range(8):
                    for xx in range(4):
                        if base+yy*4+xx>=len(cur): continue
                        b=cur[base+yy*4+xx]
                        for k,c in ((0,b&15),(1,b>>4)):
                            if c: px[tx*8+xx*2+k,ty*8+yy]=pal[c]
        if hf: part=part.transpose(Image.FLIP_LEFT_RIGHT)
        if vf: part=part.transpose(Image.FLIP_TOP_BOTTOM)
        im.alpha_composite(part,(x%256-(256 if x%256>=128 else 0)+size//2, y+size//2))
    return im
