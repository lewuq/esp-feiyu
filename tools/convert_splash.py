"""Convert the supplied brush-lettering reference into a black-background splash.
Each pixel stores an ink intensity and an approximate stroke-order timestamp.
Requires Pillow; coordinates and paths refer to the original 386x671 reference.
"""
from pathlib import Path
from PIL import Image
import math
ROOT=Path(__file__).resolve().parents[1]
SOURCE=ROOT/'references'/'brush_reference.png'
OUT=ROOT/'main'/'assets'/'splash.bin'
W,H=135,240
# Each glyph has one or more approximate pen paths. Coordinates are in source pixels.
GLYPHS=[
 [ [(72,88),(130,78),(178,92),(201,119),(195,147),(165,185),(121,220),(57,242)], [(120,101),(113,145),(96,200),(79,225)] ],
 [ [(320,79),(288,76),(253,92),(233,117),(235,137),(264,152),(302,170),(310,189),(288,210),(249,228),(211,247)] ],
 [ [(36,301),(69,286),(101,290),(114,309),(109,333),(86,367),(51,395),(17,419)],[(73,304),(67,343),(55,381),(41,400)] ],
 [ [(132,398),(159,349),(181,303)],[(180,303),(184,347),(197,399)],[(145,371),(188,368)] ],
 [ [(204,397),(225,314)],[(224,315),(267,304)],[(217,350),(251,346)] ],
 [ [(270,317),(307,308)],[(286,319),(273,386)],[(280,353),(308,344)],[(273,389),(312,378)] ],
 [ [(326,314),(359,306)],[(347,310),(328,391)],[(315,391),(355,384)] ],
 [ [(92,447),(120,471),(139,509)],[(195,443),(169,477),(138,513),(117,600),(105,641)] ],
 [ [(234,466),(228,500),(218,548),(229,579),(260,587),(293,575),(311,544),(321,484),(325,454)] ]
]

def timing(glyph,x,y):
    segments=[]; total=0.0
    for path in glyph:
        for (ax,ay),(bx,by) in zip(path,path[1:]):
            length=math.hypot(bx-ax,by-ay)
            segments.append((ax,ay,bx,by,total,length)); total+=length
    best=(float('inf'),0)
    for ax,ay,bx,by,start,length in segments:
        t=max(0,min(1,((x-ax)*(bx-ax)+(y-ay)*(by-ay))/(length*length)))
        dist=(x-(ax+(bx-ax)*t))**2+(y-(ay+(by-ay)*t))**2
        if dist<best[0]: best=(dist,(start+t*length)/total)
    return best[1]

def glyph_index(x,y):
    if y<275: return 0 if x<205 else 1
    if y<433:
        return 2 if x<124 else 3 if x<201 else 4 if x<267 else 5 if x<319 else 6
    return 7 if x<210 else 8

im=Image.open(SOURCE).convert('RGB')
if im.size!=(386,671): raise ValueError('Reference must be the supplied 386 x 671 image')
# Preserve blue pigment and light ink marks; suppress the warm paper background.
alpha=Image.new('L',im.size)
for y in range(im.height):
    for x in range(im.width):
        r,g,b=im.getpixel((x,y)); blue=b-r
        value=0 if blue<5 else min(255,max(0,int((blue-4)*2.2 + max(0,230-(r+g+b)/3)*0.65)))
        alpha.putpixel((x,y),value)
alpha=alpha.resize((W,235),Image.Resampling.LANCZOS)
raw=bytearray(); preview=Image.new('RGB',(W,H),(0,0,0))
for y in range(H):
    for x in range(W):
        a=alpha.getpixel((x,y-2)) if 2<=y<237 else 0
        sx=(x+0.5)*386/W; sy=(y-2+0.5)*671/235
        g=glyph_index(sx,sy)
        t=(g+timing(GLYPHS[g],sx,sy))/len(GLYPHS)
        order=min(254,max(0,round(t*254))) if a else 255
        raw.extend((a,order))
        preview.putpixel((x,y),tuple(round(c*a/255) for c in (77,107,254)))
OUT.write_bytes(raw)
preview.resize((W*4,H*4),Image.Resampling.NEAREST).save(ROOT/'splash_preview.png')
print('wrote',OUT,len(raw),'bytes')
