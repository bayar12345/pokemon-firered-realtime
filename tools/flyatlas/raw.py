import sys;sys.path.insert(0,'tools/flyatlas')
from render import render_map
from place import components,maps,size
from PIL import Image
comp=max(components(),key=len)
xs=[x for x,y in comp.values()];ys=[y for x,y in comp.values()]
x0,y0=min(xs),min(ys);x1=max(x+size(m)[0] for m,(x,y) in comp.items());y1=max(y+size(m)[1] for m,(x,y) in comp.items())
S=4  # pixels per metatile in the preview
W=Image.new('RGB',((x1-x0)*S,(y1-y0)*S),(0,0,0))
for m,(x,y) in comp.items():
    im,w,h=render_map(maps[m]['layout'])
    W.paste(im.resize((w*S,h*S),Image.LANCZOS),((x-x0)*S,(y-y0)*S))
W.save('/tmp/kanto_raw.png');print(W.size,x1-x0,y1-y0)
