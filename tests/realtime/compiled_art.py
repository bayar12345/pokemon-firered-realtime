"""Verify actual production ROM pixels, palettes and move table against source manifest."""
from pathlib import Path
import struct,json,re,hashlib,argparse
parser=argparse.ArgumentParser()
parser.add_argument('--repo',type=Path,default=Path(__file__).resolve().parents[2])
parser.add_argument('--symbols',type=Path,required=True);parser.add_argument('--output',type=Path,required=True);args=parser.parse_args()
p=args.repo;s={a[2]:int(a[0],16) for l in args.symbols.read_text().splitlines() if len(a:=l.split())==3}
rom=(p/'pokefirered.gba').read_bytes();m=json.loads((p/'assets/ptd/imported-attacks.json').read_text());h=(p/'src/data/realtime_ptd_graphics.h').read_text()
def read(a,n):return rom[a-0x08000000:a-0x08000000+n]
for a in m['animations']:
 b=read(s['sRtPtdAnims']+a['id']*24,24);ptr,pal,stride=struct.unpack_from('<IIH',b)
 assert stride==a['stride'];assert b[10:17]==bytes([int(a[k]) for k in ['frames','fps','width','height','columns','hold_start','hold_end']])
 raw=(p/'graphics/ptd_attacks'/a['file']).read_bytes();assert read(ptr,stride*a['frames'])==raw
 assert hashlib.sha256(raw).hexdigest()==a['raw_sha256']
 vals=re.search(r'sPtdPalette_'+str(a['palette'])+r'\[16\] = \{([^}]+)',h)[1]
 assert read(pal,32)==struct.pack('<16H',*[int(v,0) for v in vals.split(',')])
for move in m['moves']:
 # agbcc rounds this five-u16 struct to a 12-byte array element.
 assert struct.unpack('<5H',read(s['sRtPtdMoveArt']+move['move_id']*12,10))==tuple(move[k] for k in ['shot','impact','cast','beam','beam_arena'])
for move in m.get('status_moves',[]):
 b=read(s['sRtPtdStatusArt']+move['move_id']*4,4);anim,recipient,clock=struct.unpack('<HBB',b);assert anim==move['anim'] and recipient==move['recipient'] and bool(clock)==bool(move['clock'])
assert not any(x in s for x in ['gRtTestReport','gRtTestLayout','RealtimeRegression'])
assert len(rom)==33554432 and rom[0xbd]==(-(sum(rom[0xa0:0xbd])+0x19))&255
report={'passed':True,'compiled_animation_variants_checked':len(m['animations']),'status_move_mappings_checked':len(m.get('status_moves',[])),'damaging_move_mappings_checked':len(m['moves']),'all_raw_rom_pixels_match_converted_sheets':True,'all_palettes_match_generated_table':True,'gba_header_valid':True,'production_test_hooks_absent':True,'rom_sha256':hashlib.sha256(rom).hexdigest()}
args.output.write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2))
