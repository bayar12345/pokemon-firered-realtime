"""Small local libretro frontend for deterministic GBA regression tests."""
import ctypes as C
from pathlib import Path
from PIL import Image

class GameInfo(C.Structure):
    _fields_=[('path',C.c_char_p),('data',C.c_void_p),('size',C.c_size_t),('meta',C.c_char_p)]

class Descriptor(C.Structure):
    _fields_=[('flags',C.c_uint64),('ptr',C.c_void_p),('offset',C.c_size_t),('start',C.c_size_t),('select',C.c_size_t),('disconnect',C.c_size_t),('len',C.c_size_t),('addrspace',C.c_char_p)]

class MemoryMap(C.Structure):
    _fields_=[('descriptors',C.POINTER(Descriptor)),('num_descriptors',C.c_uint)]

class Variable(C.Structure):
    _fields_=[('key',C.c_char_p),('value',C.c_char_p)]

class Core:
    def __init__(self, rom, core_path=None):
        self.lib=C.CDLL(str(Path(core_path) if core_path else Path(__file__).parent/'emulator/mgba_libretro.dll'))
        self.maps=[]; self.buttons=set(); self.frame=None; self.pixel=0
        self.directory=str(Path(__file__).parent/'emulator').encode()
        self.values={b'mgba_skip_bios':b'ON',b'mgba_use_bios':b'OFF',b'mgba_frameskip':b'0'}
        self.callbacks=[]
        def callback(name,ctype,func):
            obj=ctype(func); self.callbacks.append(obj)
            getattr(self.lib,name).argtypes=[ctype]; getattr(self.lib,name)(obj)
        def env(cmd,data):
            cmd &= 0xFFFF
            if cmd==10: self.pixel=C.cast(data,C.POINTER(C.c_int))[0]; return True
            if cmd in (9,30,31): C.cast(data,C.POINTER(C.c_char_p))[0]=self.directory; return True
            if cmd==15:
                var=C.cast(data,C.POINTER(Variable)).contents
                var.value=self.values.get(var.key)
                return var.value is not None
            if cmd==16:
                variables=C.cast(data,C.POINTER(Variable)); i=0
                while variables[i].key:
                    key=variables[i].key; val=variables[i].value
                    if key not in self.values and val and b'; ' in val: self.values[key]=val.split(b'; ',1)[1].split(b'|')[0]
                    i+=1
                return True
            if cmd==17: C.cast(data,C.POINTER(C.c_bool))[0]=False; return True
            if cmd==36:
                mm=C.cast(data,C.POINTER(MemoryMap)).contents
                self.maps=[Descriptor.from_buffer_copy(mm.descriptors[i]) for i in range(mm.num_descriptors)]
                return True
            if cmd in (18,39): C.cast(data,C.POINTER(C.c_bool))[0]=False; return True
            if cmd==47: C.cast(data,C.POINTER(C.c_int))[0]=6; return True
            if cmd==52: C.cast(data,C.POINTER(C.c_uint))[0]=0; return True
            return cmd in (1,11,35,37,44,51)
        def video(data,w,h,pitch):
            if data: self.frame=(C.string_at(data,h*pitch),w,h,pitch,self.pixel)
        callback('retro_set_environment',C.CFUNCTYPE(C.c_bool,C.c_uint,C.c_void_p),env)
        callback('retro_set_video_refresh',C.CFUNCTYPE(None,C.c_void_p,C.c_uint,C.c_uint,C.c_size_t),video)
        callback('retro_set_audio_sample',C.CFUNCTYPE(None,C.c_int16,C.c_int16),lambda a,b:None)
        callback('retro_set_audio_sample_batch',C.CFUNCTYPE(C.c_size_t,C.c_void_p,C.c_size_t),lambda data,n:n)
        callback('retro_set_input_poll',C.CFUNCTYPE(None),lambda:None)
        callback('retro_set_input_state',C.CFUNCTYPE(C.c_int16,C.c_uint,C.c_uint,C.c_uint,C.c_uint),lambda port,dev,index,key:sum(1<<b for b in self.buttons) if key==256 else int(key in self.buttons))
        self.lib.retro_init()
        self.lib.retro_load_game.argtypes=[C.POINTER(GameInfo)]; self.lib.retro_load_game.restype=C.c_bool
        self.rom=C.create_string_buffer(Path(rom).read_bytes())
        self.info=GameInfo(str(Path(rom).resolve()).encode(),C.cast(self.rom,C.c_void_p),len(self.rom)-1,None)
        assert self.lib.retro_load_game(C.byref(self.info)), 'ROM load failed'
        self.lib.retro_get_memory_data.restype=C.c_void_p
        self.lib.retro_get_memory_size.restype=C.c_size_t
        self.lib.retro_serialize_size.restype=C.c_size_t
        self.lib.retro_serialize.argtypes=[C.c_void_p,C.c_size_t]; self.lib.retro_serialize.restype=C.c_bool
        self.lib.retro_unserialize.argtypes=[C.c_void_p,C.c_size_t]; self.lib.retro_unserialize.restype=C.c_bool
    def run(self,n=1,buttons=()):
        self.buttons=set(buttons)
        for _ in range(n): self.lib.retro_run()
    def ptr(self,addr):
        for d in self.maps:
            if not d.ptr or not d.len: continue
            if d.select and (addr&d.select)!=(d.start&d.select): continue
            offset=(addr-d.start)&~d.disconnect
            if 0<=offset<d.len: return d.ptr+d.offset+offset
        if 0x2000000<=addr<0x2040000: return self.lib.retro_get_memory_data(2)+addr-0x2000000
        raise KeyError(hex(addr))
    def read(self,addr,size): return C.string_at(self.ptr(addr),size)
    def u32(self,addr): return int.from_bytes(self.read(addr,4),'little')
    def write(self,addr,data): C.memmove(self.ptr(addr),data,len(data))
    def save_state(self,path):
        size=self.lib.retro_serialize_size(); buf=C.create_string_buffer(size)
        assert self.lib.retro_serialize(buf,size); Path(path).write_bytes(buf.raw)
    def load_state(self,path):
        data=Path(path).read_bytes(); buf=C.create_string_buffer(data)
        assert self.lib.retro_unserialize(buf,len(data))
    def screenshot(self,path):
        data,w,h,pitch,pixel=self.frame
        if pixel==1: im=Image.frombytes('RGB',(w,h),data,'raw','BGRX',pitch)
        else:
            bits=(5,6,5) if pixel==2 else (5,5,5)
            out=bytearray()
            for y in range(h):
                for x in range(w):
                    value=int.from_bytes(data[y*pitch+x*2:y*pitch+x*2+2],'little')
                    out.extend(((value>>(bits[1]+5)&31)*255//31,(value>>5&((1<<bits[1])-1))*255//((1<<bits[1])-1),(value&31)*255//31))
            im=Image.frombytes('RGB',(w,h),bytes(out))
        im.save(path)
    def close(self): self.lib.retro_unload_game(); self.lib.retro_deinit()

if __name__=='__main__':
    import sys
    core=Core(sys.argv[1]); core.run(300); core.screenshot(sys.argv[2]); print('Booted',len(core.maps),'memory mappings'); core.close()
