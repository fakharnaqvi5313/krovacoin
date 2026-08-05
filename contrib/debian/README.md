
Debian
====================
This directory contains files used to package krovad/krova-qt
for Debian-based Linux systems. If you compile krovad/krova-qt yourself, there are some useful files here.

## krovacoin: URI support ##


krova-qt.desktop  (Gnome / Open Desktop)
To install:

	sudo desktop-file-install krova-qt.desktop
	sudo update-desktop-database

If you build yourself, you will either need to modify the paths in
the .desktop file or copy or symlink your krova-qt binary to `/usr/bin`
and the `../../share/pixmaps/krovacoin128.png` to `/usr/share/pixmaps`

krova-qt.protocol (KDE)

