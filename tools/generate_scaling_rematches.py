"""Link map NPC interaction scripts to their existing native trainer commands."""
from pathlib import Path
import re,json,argparse
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--project',type=Path,default=Path(__file__).resolve().parents[1])
parser.add_argument('--report',type=Path)
args=parser.parse_args()
project=args.project.resolve()
constants={name for name in re.findall(r'^#define\s+(TRAINER_\w+)\s+\d+', (project/'include/constants/opponents.h').read_text(encoding="utf-8"),re.M)}
paths=sorted(set((project/'data/scripts').rglob('*.inc'))|set((project/'data/maps').rglob('scripts.inc')))
blocks={}; order=[]; rewritten={}; counter=0
label=re.compile(r'^(\w+)::?\s*(?:@.*)?$')
for path in paths:
    original=path.read_text(encoding="utf-8"); original=re.sub(r'^RtScaledBattle_\d+::\n','',original,flags=re.M)
    lines=original.splitlines(); output=[]; current=None
    for line in lines:
        m=label.match(line)
        if m:
            current=m[1]; blocks[current]={'lines':[],'trainers':[],'next':None};
            if order and order[-1][0]==path: blocks[order[-1][1]]['next']=current
            order.append((path,current))
        if current:blocks[current]['lines'].append(line)
        if re.match(r'^\s*trainerbattle(?:_\w+)?\s+',line):
            trainers=[t for t in re.findall(r'\bTRAINER_\w+\b',line) if t in constants and t!='TRAINER_SECRET_BASE']
            if len(trainers)==1 and not re.match(r'^\s*trainerbattle_rematch',line):
                cmd=f'RtScaledBattle_{counter:04d}';counter+=1
                output.append(cmd+'::')
                if current:blocks[current]['trainers'].append((trainers[0],cmd))
        output.append(line)
    rewritten[path]='\n'.join(output)+'\n'
roots=set()
for path in (project/'data/maps').rglob('map.json'):
    for obj in json.loads(path.read_text(encoding="utf-8")).get('object_events',[]):
        if obj.get('script') not in (None,'0','NULL'):roots.add(obj['script'])
def reachable(root):
    seen=set(); todo=[root]; result=set()
    while todo and len(seen)<256:
        name=todo.pop()
        if name in seen or name not in blocks:continue
        seen.add(name);block=blocks[name];result.update(block['trainers']);commands=[]
        for line in block['lines'][1:]:
            clean=line.split('@')[0].strip()
            if not clean or clean.startswith('.') or label.match(clean):continue
            commands.append(clean)
            if re.match(r'^(?:goto|call|case|jump)(?:_\w+)?\b',clean):
                todo.extend(token for token in re.findall(r'\b\w+\b',clean) if token in blocks)
        if not commands:
            if block['next']:todo.append(block['next'])
        elif not re.match(r'^(?:end|return|goto\s|jump\s|endtrainerbattle|gotopostbattlescript|gotobeatenscript|warpsilent|warp)\b',commands[-1]):
            if block['next']:todo.append(block['next'])
    return result
rows=[]
for root in sorted(roots):
    for trainer,cmd in sorted(reachable(root)):rows.append((root,trainer,cmd))
assert len(rows)>300,len(rows)
externs=sorted({root for root,_,_ in rows}|{cmd for _,_,cmd in rows})
lines=['// Generated from native map interaction scripts; no script bytes are changed by the added labels.']
lines.extend('extern const u8 '+name+'[];' for name in externs)
lines.append('static const struct ScalingRematchEntry sScalingRematches[] = {')
lines.extend(f' {{{root},{cmd},{trainer}}},' for root,trainer,cmd in rows);lines.append('};')
for path,text in rewritten.items():
    if path.read_text(encoding="utf-8")!=text:path.write_text(text,encoding='utf-8',newline='\n')
(project/'src/data/scaling_rematch_trainers.h').write_text('\n'.join(lines)+'\n',encoding='utf-8',newline='\n')
if args.report: args.report.write_text(json.dumps({'rows':len(rows),'npc_scripts':len({r[0] for r in rows}),'trainers':len({r[1] for r in rows}),'mapping':rows},indent=2))
print('Mapping',len(rows),'entries for',len({r[0] for r in rows}),'NPC interaction scripts and',len({r[1] for r in rows}),'trainer IDs')
