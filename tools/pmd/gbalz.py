"""GBA BIOS LZ77 (type 0x10) compressor, VRAM-safe (no displacement < 2)."""
def compress(src):
    src=bytes(src); n=len(src); out=bytearray([0x10,n&255,(n>>8)&255,(n>>16)&255])
    heads={}; i=0
    def find(i):
        best_len=0; best_d=0
        if i+3>n: return 0,0
        key=src[i:i+3]
        for j in reversed(heads.get(key,[])[-64:]):
            d=i-j
            if d>4096: break
            if d<2: continue
            l=3
            while l<18 and i+l<n and src[j+l]==src[i+l]: l+=1
            if l>best_len: best_len,best_d=l,d
            if l==18: break
        return best_len,best_d
    def add(k):
        if k+3<=n: heads.setdefault(src[k:k+3],[]).append(k)
    while i<n:
        flag_pos=len(out); out.append(0); flags=0
        for b in range(8):
            if i>=n: break
            l,d=find(i)
            if l>=3:
                flags|=0x80>>b; v=((l-3)<<12)|(d-1); out+=bytes([v>>8,v&255])
                for k in range(i,i+l): add(k)
                i+=l
            else:
                out.append(src[i]); add(i); i+=1
        out[flag_pos]=flags
    while len(out)%4: out.append(0)
    return bytes(out)
def decompress(data):
    n=data[1]|data[2]<<8|data[3]<<16; out=bytearray(); i=4
    while len(out)<n:
        f=data[i]; i+=1
        for b in range(8):
            if len(out)>=n: break
            if f&(0x80>>b):
                v=data[i]<<8|data[i+1]; i+=2; l=(v>>12)+3; d=(v&0xFFF)+1
                for _ in range(l): out.append(out[-d])
            else: out.append(data[i]); i+=1
    return bytes(out)
if __name__=='__main__':
    import os
    for t in [bytes(2048), os.urandom(512), bytes(range(256))*8]:
        assert decompress(compress(t))==t
    print('ok')
