"""Chuyen dung GLB Butterfly cua Polyfork sang DirectX text, can Python + NumPy.

Chay: python convert_glb_to_x.py (file nguon nam cung thu muc).
Khong la trinh chuyen glTF tong quat: tu choi cac tinh nang chua ho tro.
"""
import json
import re
import struct
from pathlib import Path
import numpy as np

FPS = 240
ROOT = Path(__file__).resolve().parent
b = (ROOT / 'butterfly-base.glb').read_bytes()
assert b[:4] == b'glTF' and struct.unpack_from('<I', b, 4)[0] == 2
n = struct.unpack_from('<I', b, 12)[0]
d = json.loads(b[20:20+n])
blob = b[28+n:]

def accessor(i):
    a = d['accessors'][i]
    assert 'sparse' not in a and not a.get('normalized', False)
    v = d['bufferViews'][a['bufferView']]
    assert v.get('buffer', 0) == 0
    dtype = np.dtype({5126:'<f4',5125:'<u4',5123:'<u2',5121:'u1'}[a['componentType']])
    width = {'SCALAR':1,'VEC2':2,'VEC3':3,'VEC4':4,'MAT4':16}[a['type']]
    start = v.get('byteOffset', 0) + a.get('byteOffset', 0)
    stride = v.get('byteStride', width*dtype.itemsize)
    return np.ndarray((a['count'],width),dtype,blob,start,(stride,dtype.itemsize)).copy()

def matrix(t, q, s):
    x,y,z,w = np.asarray(q)/np.linalg.norm(q)
    m = np.eye(4)
    m[:3,:3] = np.array([[1-2*(y*y+z*z),2*(x*y-z*w),2*(x*z+y*w)],
        [2*(x*y+z*w),1-2*(x*x+z*z),2*(y*z-x*w)],
        [2*(x*z-y*w),2*(y*z+x*w),1-2*(x*x+y*y)]]) @ np.diag(s)
    m[:3,3] = t
    return m

flip = np.diag([1,1,-1,1])
def numbers(a): return ','.join(format(float(x), '.10g') for x in np.asarray(a).flat)
def xmatrix(m): return numbers((flip @ m @ flip).T)
nodes = d['nodes']
names = [f'N{i}_'+re.sub('[^a-zA-Z0-9_]', '_', a.get('name','node')) for i,a in enumerate(nodes)]
assert len(d['meshes']) == len(d['skins']) == 1
skin = d['skins'][0]
bind = accessor(skin['inverseBindMatrices']).reshape(-1,4,4).transpose(0,2,1)
prim, = d['meshes'][0]['primitives']
assert prim.get('mode',4) == 4
attrs = {k:accessor(v) for k,v in prim['attributes'].items()}
pos, norm, color = (attrs[k] for k in ('POSITION','NORMAL','COLOR_0'))
joints, weights = attrs['JOINTS_0'], attrs['WEIGHTS_0']
assert np.allclose(weights.sum(axis=1),1,atol=1e-5)
faces = accessor(prim['indices']).reshape(-1,3)[:,[0,2,1]]
assert np.allclose(color[faces],color[faces[:,0]][:,None,:])
palette, colorids = np.unique(color,axis=0,return_inverse=True)
# glTF luu mau tuyen tinh; DxLib dung gia tri mau hien thi sRGB.
pal = np.where(palette<=.0031308,12.92*palette,1.055*palette**(1/2.4)-.055)
lines = ['xof 0303txt 0032',f'AnimTicksPerSecond {{ {FPS}; }}']
def emit(s): lines.append(s)
def local(i, overrides=None):
    node = nodes[i]
    assert 'matrix' not in node
    v = {'translation':node.get('translation',[0,0,0]),'rotation':node.get('rotation',[0,0,0,1]),'scale':node.get('scale',[1,1,1])}
    if overrides: v.update(overrides)
    return matrix(v['translation'],v['rotation'],v['scale'])

def mesh():
    emit('Mesh Butterfly {')
    p=pos*np.array([1,1,-1]); ns=norm*np.array([1,1,-1])
    emit(str(len(p))+';')
    emit(',\n'.join(';'.join(format(float(x),'.10g') for x in v)+';' for v in p)+';')
    face_text=',\n'.join('3;'+','.join(str(int(x)) for x in f)+';' for f in faces)+';'
    emit(str(len(faces))+';');emit(face_text)
    emit('MeshNormals {');emit(str(len(ns))+';')
    emit(',\n'.join(';'.join(format(float(x),'.10g') for x in v)+';' for v in ns)+';')
    emit(str(len(faces))+';');emit(face_text);emit('}')
    emit('MeshMaterialList {');emit(str(len(pal))+';');emit(str(len(faces))+';')
    emit(','.join(str(int(colorids[f[0]])) for f in faces)+';;')
    for c in pal:
        emit('Material { '+';'.join(format(float(x),'.10g') for x in c)+';1.0;; 0; 0;0;0;; 0;0;0;; }')
    emit('}')
    maxv=int((weights>0).sum(axis=1).max())
    maxf=max(len(set(int(joints[v,k]) for v in f for k in range(4) if weights[v,k]>0)) for f in faces)
    emit(f'XSkinMeshHeader {{ {maxv}; {maxf}; {len(skin["joints"])}; }}')
    for j,node in enumerate(skin['joints']):
        w=np.where(joints==j,weights,0).sum(axis=1);ids=np.flatnonzero(w>0)
        if not len(ids):continue
        emit('SkinWeights {');emit('"'+names[node]+'";');emit(str(len(ids))+';')
        emit(','.join(str(int(x)) for x in ids)+';');emit(numbers(w[ids])+';')
        emit(xmatrix(bind[j])+';;');emit('}')
    emit('}')

def frame(i):
    emit('Frame '+names[i]+' {');emit('FrameTransformMatrix { '+xmatrix(local(i))+';; }')
    if 'mesh' in nodes[i]: mesh()
    for c in nodes[i].get('children',[]):frame(c)
    emit('}')
for i in d['scenes'][d.get('scene',0)]['nodes']:frame(i)

def sample(ts, values, t, rotation):
    hi=int(np.searchsorted(ts,t,side='right'))
    if hi==0:return values[0]
    if hi>=len(ts):return values[-1]
    lo=hi-1;u=(t-ts[lo])/(ts[hi]-ts[lo]);a=values[lo].astype(float);c=values[hi].astype(float)
    if not rotation:return (1-u)*a+u*c
    dot=float(np.dot(a,c))
    if dot<0:c=-c;dot=-dot
    if dot>.9995:
        q=(1-u)*a+u*c;return q/np.linalg.norm(q)
    theta=np.arccos(np.clip(dot,-1,1))
    return (np.sin((1-u)*theta)*a+np.sin(u*theta)*c)/np.sin(theta)

summary=[]
for anim in d['animations']:
    channels={};duration=0
    for c in anim['channels']:
        s=anim['samplers'][c['sampler']];assert s.get('interpolation','LINEAR')=='LINEAR'
        ts=accessor(s['input']).ravel();vs=accessor(s['output'])
        channels.setdefault(c['target']['node'],{})[c['target']['path']]=(ts,vs)
        duration=max(duration,float(ts[-1]))
    count=int(np.ceil(duration*FPS))+1
    emit('AnimationSet '+re.sub('[^a-zA-Z0-9_]','_',anim['name'])+' {')
    for i,paths in channels.items():
        emit('Animation { {'+names[i]+'} AnimationKey { 4; '+str(count)+';')
        keys=[]
        for tick in range(count):
            t=min(tick/FPS,duration)
            overrides={k:sample(ts,vs,t,k=='rotation') for k,(ts,vs) in paths.items()}
            keys.append(f'{tick};16;'+xmatrix(local(i,overrides))+';;')
        emit(',\n'.join(keys)+';');emit('} }')
    emit('}')
    summary.append({'name':anim['name'],'seconds':duration,'ticks':count-1})
(ROOT/'butterfly.x').write_text('\n'.join(lines)+'\n',encoding='ascii')
print(json.dumps({'vertices':len(pos),'triangles':len(faces),'colors':len(palette),'bones':len(skin['joints']),'animations':summary},indent=2))
