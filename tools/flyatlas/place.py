import json,glob,os
from pathlib import Path
P=Path(__file__).resolve().parents[2]
L={l['id']:l for l in json.loads((P/'data/layouts/layouts.json').read_text())['layouts'] if 'id' in l}
maps={}
for f in glob.glob(str(P/'data/maps/*/map.json')):
    m=json.load(open(f))
    if os.path.basename(os.path.dirname(f)).startswith('World_'):continue
    if m['id'].startswith('MAP_PROTOTYPE'):continue
    maps[m['id']]=m
def size(mid):l=L[maps[mid]['layout']];return l['width'],l['height']
def components():
    seen={};comps=[]
    for start in maps:
        if start in seen:continue
        if not maps[start].get('connections') and maps[start].get('map_type') not in ('MAP_TYPE_TOWN','MAP_TYPE_CITY'):continue
        pos={start:(0,0)};q=[start]
        while q:
            a=q.pop();ax,ay=pos[a];aw,ah=size(a)
            for c in maps[a].get('connections') or []:
                b=c['map']
                if b not in maps or b in pos:continue
                bw,bh=size(b);o=c['offset'];d=c['direction']
                pos[b]={'up':(ax+o,ay-bh),'down':(ax+o,ay+ah),'left':(ax-bw,ay+o),'right':(ax+aw,ay+o)}[d];q.append(b)
        for k in pos:seen[k]=1
        comps.append(pos)
    return comps
if __name__=='__main__':
    for c in sorted(components(),key=len,reverse=True):
        xs=[x for x,y in c.values()];ys=[y for x,y in c.values()]
        print(len(c),min(xs),min(ys),max(xs),max(ys),sorted(c)[:4])

def world_layout(gap=34):
    """Kanto plus the Sevii Islands laid out in the sea south of Kanto (fly picture only)."""
    comps=[c for c in components() if c]
    kanto=max(comps,key=len);out=dict(kanto)
    def box(c):
        xs=[x for x,y in c.values()];ys=[y for x,y in c.values()]
        return min(xs),min(ys),max(x+size(m)[0] for m,(x,y) in c.items()),max(y+size(m)[1] for m,(x,y) in c.items())
    kx0,ky0,kx1,ky1=box(kanto)
    order=['MAP_ONE_ISLAND','MAP_TWO_ISLAND','MAP_THREE_ISLAND','MAP_FOUR_ISLAND','MAP_FIVE_ISLAND','MAP_SIX_ISLAND','MAP_SEVEN_ISLAND']
    groups=[]
    for key in order:
        c=next((c for c in comps if key in c and c is not kanto),None)
        if c and c not in groups:groups.append(c)
    rows=[groups[:4],groups[4:]];y=ky1+gap
    for row in rows:
        x=kx0+20;h=0
        for c in row:
            bx0,by0,bx1,by1=box(c)
            for m,(mx,my) in c.items():out[m]=(mx-bx0+x,my-by0+y)
            x+=bx1-bx0+gap;h=max(h,by1-by0)
        y+=h+gap
    return out
