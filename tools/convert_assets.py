from pathlib import Path
from PIL import Image
import argparse
p=argparse.ArgumentParser(); p.add_argument('source',type=Path); p.add_argument('--out',type=Path,default=Path(__file__).resolve().parents[1]/'main'/'assets'); args=p.parse_args()
names=['home','beg','caught','defiant','dust','flee','knocked','scared','gatling','broom','sock','rice']
args.out.mkdir(parents=True,exist_ok=True)
for n in names:
 im=Image.open(args.source/('actors' if n in names[:8] else 'weapons')/(n+'.webp')).convert('RGBA')
 size=112 if n in names[:8] else 60
 im=im.resize((size,size),Image.Resampling.LANCZOS)
 data=bytearray()
 for r,g,b,a in im.getdata():
  c=((r>>3)<<11)|((g>>2)<<5)|(b>>3)
  data.extend((c>>8,c&255,a))
 (args.out/(n+'.bin')).write_bytes(data)
 print(n,size,len(data))
