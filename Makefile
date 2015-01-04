# This file is part of SoundPad.

# SoundPad is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.

# SoundPad is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with SoundPad; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

CC = cl
CFLAGS = /DNDEBUG /D_WINDOWS /DWIN32
LIBS = winmm.lib kernel32.lib user32.lib
OBJS = soundpad.obj soundpad.res

all: soundpad.exe

soundpad.exe: $(OBJS)
	$(CC) /o $@ $(OBJS) $(LIBS)

soundpad.obj: resource.h
soundpad.res: resource.h

clean:
	del *.obj *.res
