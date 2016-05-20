# iffanimplay
Player and decoder library for IFF ANIM and CDXL animation file format, common on classic Amiga OS in the 1990s

Project includes:
 * iffanimplay - video player (uses libSDL 1.2.x for video and audio, OpenGL optionally)
 * libiffanim - library for decoding IFF-ANIM video file format
 * libcdxl - library for decoding CDXL video file format

Notes:  
Libiffanim and Libcdxl are standalone. The Makefile builds static libs.

IFF ANIM is related to IFF ILBM. Pixel data ist stored losslessly in bitplanes.  
The first full frame is typically compressed with a run length encoding while all subsequent frames are stored with one of several delta encoding methods.  
Subsequent frames only store the difference to the previous frame.  
For each frame the time to display is stored with a resolution of 1/60 second.  

I didn't want to make a separate CDXL player so I've added the library to the Iffanimplay project.


Other multiplatform players:  
 ANIMApplet - http://www.randelshofer.ch/animapplet/  
 MultiShow - http://www.randelshofer.ch/multishow/  

Programs that export ANIM:  
 Deluxe Paint  
 Personal Paint  
 ...  

Free animation files are available at http://aminet.net/pix/anim

![shot0.png](/screenshot/shot0.png?raw=true)  
