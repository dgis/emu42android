DESCRIPTION

This project ports the Windows application Emu42 written in C to Android.
It uses the Android NDK. The former Emu42 source code remains untouched because of a thin win32 emulation layer above Linux/NDK!
This win32 layer will allow to easily update from the original Emu42 source code.
It can open or save the exact same state files (state.e??) than the original Windows application!

This application does NOT come with the ROM files!
You will need KML scripts and ROM files already copied into your Android filesystem.
You can download the KML scripts from the original Emu42 Windows application archive (https://hp.giesselink.com/emu42.htm)
and you can extract the ROM file from a real calculator (or be lucky on internet).
Be careful about the case sensitivity of the filename in the KML script (Linux is case sensitive, not Windows).

The application does not request any permission (because it opens the files or the KML folders using the content:// scheme).

The application is distributed with the same license under GPL and you can find the source code here:
https://github.com/dgis/emu42android


QUICK START

1. From the left side, slide your finger to open the menu.
2. Touch the "New..." menu item.
3. "Select a Custom KML script folder..." where you have copied the KML scripts and ROM files.
4. And the calculator should now be opened.


NOTES

- When using a custom KML script by selecting a folder, you must take care of the case sensitivity of its dependency files.
- This Emulator does not include the ROM files or the KML files.


NOT WORKING YET

- Disassembler
- Debugger
- Macro
- Infrared Printer


CHANGES

Version 1.0 (2019-05-xx)

- First public version available on the store. It is based on Emu42 version 1.22 from Christoph Gießelink.


LICENSES

Android version by Régis COSNIER.
This program is based on Emu42 for Windows version, copyrighted by Christoph Gießelink & Sébastien Carlier, with the addition of a win32 layer to run on Android.

This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation; either version 2 of the License, or (at your option) any later version.
This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
You should have received a copy of the GNU General Public License along with this program; if not, write to the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
