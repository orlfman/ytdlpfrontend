hi friends.

needs yt-dlp installed on your system. along with qt libraries and qmake6. never tested outside a kde enviroment so your mileage may vary.

make sure you have all the "optional" dependencies for yt-dlp installed.
>aria2
>atomicparsley
>ffmpeg
>python-brotli
>python-mutagen
>python-pycryptodome
>python-pyxattr
>python-secretstorage
>python-websockets
>rtmpdump

uses qmake6 and make to build.

use the included build.sh file:
chmod +x build.sh
./build.sh

or do it manually:

run in the root directory of YTDLPFrontend:
mkdir build
cd build
qmake6 ../YTDLPFrontend.pro
make

and that's it. in the build directory of YTDLPFrontend folder should have the YTDLPFrontend binary

run it via ./YTDLPFrontend or add it to your path. either by placing it in /usr/local/bin or whatever tickles your fancy.

there is also a .desktop file included, ytdlpfrontend.desktop, you can place in /usr/share/applications
by default, the .desktop file looks for the binary in /usr/local/bin
if you want to use the .desktop file, you can move the included .png file, YTDLPFrontend.png to /usr/share/icons so it can have an icon
if you want the program to have a fancy icon in the titlebar, you need to place YTDLPFrontend.png to /usr/share/icons

honestly just use the build.sh. it so much easier.
