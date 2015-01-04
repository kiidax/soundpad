SoundPad Manual
===============

What is SoundPad
----------------

SoundPad is a sound recording software. The main use of this software is

1) Record a sound clip and play it.
2) Save a recorded sound clip
3) Echo back sound with delay


Requirement
-----------

Windows 95/98/Me/NT/2000/XP
Soundcard, a mic and speaker

* To echo back with delay you'll need a soundcard which can
  record and play sound at the same time.


How to install
--------------

* Extract soundpad.exe to a directory you like. You need to extract
  soundpad.exe if you don't need to build it.

  
How to build
------------

With Mingw

make -f GNUMakefile

With MS Visual C++

nmake


How to use
----------

* Click SoundPad.exe

To record sound and play it

* Clear the Loop check box.
* Press the Record button to start recording.
* Press the Stop button to stop recording.
* Press the Play button to play recorded sound.

Note: You can stop recording and play it by pressing the Play button.

To save a recorded sound.

* Select from File, Save
* tempfile.wav will be created on the current directory.

To play back with delay

* Select the Loop check box.
* Press the Record button to start.
* Wait for a moment and press the Play buton when you want play
  back sound to start.
* Press the Stop button to stop the play back.


Shout Cuts
----------

Ctrl-Q - Quit
Ctrl-S - Save
2      - Record
3      - Play
4      - Stop
L      - Loop


Known problems
--------------

* You cannot open a sound clip from file


Contacts
--------

Bug reports and comments are always welcome. However as the way this
kind of things are, I cannot guarrantee that I reply to you.

Have fun!
Katsuya Iida
katsuya_iida@hotmail.com
