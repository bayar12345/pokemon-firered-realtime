"""Render FireRed outdoor maps from the decomp source (layouts + tilesets) at full metatile resolution."""
import json,re,struct,os
from pathlib import Path
from PIL import Image
P=Path(__file__).resolve().parents[2]
_hdr=(P/'src/data/tilesets/headers.h').read_text()
_gfx=(P/'src/graphics.c').read_text()+(P/'src/data/tilesets/graphics.h').read_text() if (P/'src/data/tilesets/graphics.h').exists() else (P/'src/graphics.c').read_text()
_met=(P/'src/data/tilesets/metatiles.h').read_text()
def _block(name):
    m=re.search(r'const struct Tileset '+re.escape(name)+r'\s*=\s*\{(.*?)\};',_hdr,re.S);return m.group(1)
def _field(b,f):return re.search(r'\.'+f+r'\s*=\s*(\w+)',b).group(1)
def _incbin(text,sym):
    m=re.search(re.escape(sym)+r'\[\]\s*=\s*INCBIN_\w+\("([^"]+)"\)',text);return m.group(1) if m else None
def _pals(sym):
    m=re.search(re.escape(sym)+r'\[\]\[16\]\s*=\s*\{(.*?)\};',_gfx,re.S)
    return re.findall(r'INCBIN_U16\("([^"]+)"\)',m.group(1))
def _gbapal(p):
    d=(P/p).read_bytes();return [((v&31)*255//31,((v>>5)&31)*255//31,((v>>10)&31)*255//31) for v in struct.unpack('<16H',d[:32])]
def _tiles(path):
    path=path.replace('.lz','')
    d=(P/path).read_bytes();return [d[i:i+32] for i in range(0,len(d),32)]
class Tileset:
    def __init__(s,name):
        b=_block(name);s.secondary='TRUE' in re.search(r'isSecondary\s*=\s*(\w+)',b).group(1)
        s.tiles=_tiles(_incbin(_gfx,_field(b,'tiles')))
        s.pals=[_gbapal(p) for p in _pals(_field(b,'palettes'))]
        md=(P/_incbin(_met,_field(b,'metatiles'))).read_bytes();s.meta=[struct.unpack_from('<8H',md,i) for i in range(0,len(md)-15,16)]
_ts={}
def tileset(n):
    if n not in _ts:_ts[n]=Tileset(n)
    return _ts[n]
class MapRenderer:
    def __init__(s,prim,sec):
        s.p=tileset(prim);s.s=tileset(sec);s.cache={}
        s.pals=(s.p.pals[:7]+s.s.pals[7:13]) if len(s.s.pals)>=13 else s.p.pals
    def tile(s,idx):
        return s.p.tiles[idx] if idx<640 else (s.s.tiles[idx-640] if idx-640<len(s.s.tiles) else bytes(32))
    def metatile(s,mid):
        if mid in s.cache:return s.cache[mid]
        meta=s.p.meta[mid] if mid<640 else (s.s.meta[mid-640] if mid-640<len(s.s.meta) else (0,)*8)
        im=Image.new('RGB',(16,16));px=im.load()
        for layer in range(2):
            for q in range(4):
                v=meta[layer*4+q];t=s.tile(v&0x3FF);hf=v&0x400;vf=v&0x800;pal=s.pals[(v>>12)%len(s.pals)]
                ox,oy=(q&1)*8,(q>>1)*8
                for y in range(8):
                    for x in range(8):
                        b=t[y*4+x//2];c=(b>>4) if x&1 else b&15
                        if layer and not c:continue
                        X=7-x if hf else x;Y=7-y if vf else y;px[ox+X,oy+Y]=pal[c]
        s.cache[mid]=im;return im
_layouts={l['id']:l for l in json.loads((P/'data/layouts/layouts.json').read_text())['layouts'] if 'id' in l}
def render_map(layout_id):
    l=_layouts[layout_id];r=MapRenderer(l['primary_tileset'],l['secondary_tileset'])
    d=(P/l['blockdata_filepath']).read_bytes();w,h=l['width'],l['height']
    im=Image.new('RGB',(w*16,h*16))
    for i in range(w*h):
        v=struct.unpack_from('<H',d,i*2)[0]&0x3FF;im.paste(r.metatile(v),((i%w)*16,(i//w)*16))
    return im,w,h
if __name__=='__main__':
    im,w,h=render_map('LAYOUT_PALLET_TOWN');im.save('/tmp/pallet.png');print(w,h)
