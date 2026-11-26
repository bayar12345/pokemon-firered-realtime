import json, struct, argparse
from pathlib import Path
from libretro import Core

root=Path(__file__).parent
parser=argparse.ArgumentParser()
parser.add_argument('--rom',type=Path,default=root/'pokefirered/pokefirered.gba')
parser.add_argument('--symbols',type=Path,default=root/'v6-regression-symbols.txt')
parser.add_argument('--core',type=Path)
parser.add_argument('--output-dir',type=Path,default=root)
args=parser.parse_args()
root=args.output_dir; root.mkdir(parents=True,exist_ok=True)
symbols={parts[2]:int(parts[0],16) for line in args.symbols.read_text().splitlines() if len(parts:=line.split())==3}
core=Core(args.rom,args.core)
addr=symbols['gRtTestReport']
def report(): return struct.unpack('<64I',core.read(addr,256))
for _ in range(1500):
    core.run(10)
    if report()[0]==0x5254444E: break
data=report()
print('C regression:',data[:16],flush=True)
if data[0]!=0x5254444E: raise AssertionError('Regression did not finish')
failures=list(data[16:16+data[2]])
print('Failed C assertions:',failures,flush=True)
core.run(60); core.screenshot(root/'learning-menu.png')
ui=[]
def check(name,condition):
    ui.append({'name':name,'passed':bool(condition)})
    print(name, 'PASS' if condition else 'FAIL',flush=True)
def press(key): core.run(30,[key]); core.run(30)
check('initial four-slot move menu',report()[4]==18 and report()[5]==33)
press(0); check('decline asks for confirmation',report()[9]==4 and report()[10]==1)
press(0); check('cancel decline preserves moves and PP Ups',report()[5]==33 and report()[6]==255 and report()[7]==0)
press(4); check('navigate to HM slot',report()[9]==3)
press(8); check('HM replacement blocked',report()[5]==33 and report()[10]==0)
core.screenshot(root/'learning-hm-protection.png')
press(5); press(5); press(8); check('replacement asks for confirmation',report()[9]==0 and report()[10]==1)
press(0); check('cancel replacement preserves original move',report()[5]==33 and report()[6]==255)
press(8); press(8); check('confirm teaches Ember and resets only its PP Ups',report()[5]==52 and report()[6]==252 and report()[7]==1)
core.run(170); check('next pending move offered',report()[4]==18 and report()[7]==1)
core.screenshot(root/'learning-next-move.png')
press(0); press(8); core.run(170)
check('decline second move resumes progression',report()[4]==12 and report()[5]==52)
result={'c_assertions':data[1],'c_failures':failures,'ui_checks':ui,'passed':not failures and all(c['passed'] for c in ui)}
result['status_probe_bits']=list(data[48:60])
(root/'layout.json').write_text(json.dumps(struct.unpack('<32I',core.read(symbols['gRtTestLayout'],128))))
(root/'regression-results.json').write_text(json.dumps(result,indent=2))
core.close()
if not result['passed']: raise SystemExit(1)
