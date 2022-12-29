## Building
```
1. Download the pacbrew-pacman from following location and install.
   https://github.com/PacBrew/pacbrew-pacman/releases
2. Run following cmds
   pacbrew-pacman -Sy
   pacbrew-pacman -S ps4-openorbis ps4-openorbis-portlibs
   git checkout ps4
   chmod guo+x /opt/pacbrew/ps4/openorbis/ps4vars.sh
   source /opt/pacbrew/ps4/openorbis/ps4vars.sh
   mkdir build; cd build
   openorbis-cmake -DCMAKE_INSTALL_PREFIX=$OPENORBIS/usr ..
   make
   make install
```