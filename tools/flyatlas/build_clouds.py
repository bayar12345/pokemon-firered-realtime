"""Cloud layers for the Fly journey (procedural, FireRed-style soft whites). One 256x256 texture,
shown on two background layers at different speeds."""
import struct
from pathlib import Path
import numpy as np
from PIL import Image
P=Path(__file__).resolve().parents[2];OUT=P/'graphics/fly_atlas'
rng=np.random.default_rng(42)
def tile_noise(n,cells):
    g=rng.random((cells,cells))
    g=np.concatenate([g,g[:, :1]],1);g=np.concatenate([g,g[:1]],0)   # wraps seamlessly
    im=Image.fromarray((g*255).astype(np.uint8)).resize((n+n//cells,n+n//cells),Image.BICUBIC)
    return np.asarray(im)[:n,:n].astype(np.float32)/255
N=256
f=tile_noise(N,4)*0.55+tile_noise(N,8)*0.3+tile_noise(N,16)*0.15
f=(f-f.min())/(f.max()-f.min())
# 2x2 pixel blocks keep the tile count low and match FireRed's chunky cloud style
f=f.reshape(N//2,2,N//2,2).mean((1,3)).repeat(2,0).repeat(2,1)
levels=np.digitize(f,[0.68,0.73,0.78,0.84])   # 0 = clear sky
cols=[(0,0,0),(190,200,220),(214,222,236),(234,238,248),(255,255,255)]
tiles={};tl=[];mp=[]
for ty in range(32):
    for tx in range(32):
        t=levels[ty*8:ty*8+8,tx*8:tx*8+8].ravel()
        b=bytes(int(t[i])|(int(t[i+1])<<4) for i in range(0,64,2))
        if b not in tiles:tiles[b]=len(tl);tl.append(b)
        mp.append(tiles[b]|(15<<12))
(OUT/'cloud_tiles.4bpp').write_bytes(b''.join(tl));(OUT/'cloud_map.bin').write_bytes(b''.join(struct.pack('<H',v) for v in mp))
pal=[0]+[(c[0]>>3)|((c[1]>>3)<<5)|((c[2]>>3)<<10) for c in cols[1:]]+[0]*11
(OUT/'cloud_pal.gbapal').write_bytes(struct.pack('<16H',*pal))
Image.fromarray(np.array(cols,np.uint8)[levels]).save('/tmp/clouds.png')
print('cloud tiles',len(tl))
