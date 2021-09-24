DESCRIPTION

This project ports the Windows application Emu42 written in C to Android.
It uses the Android NDK. The former Emu42 source code (written by Christoph Giesselink) remains untouched because of a thin win32 emulation layer above Linux/NDK!
This win32 layer will allow to easily update from the original Emu42 source code.
It can open or save the exact same state files (state.e??) than the original Windows application!

This application does NOT come with the ROM files!
You will need KML scripts and ROM files already copied into your Android filesystem.
You can download the KML scripts here: http://regis.cosnier.free.fr/soft/androidEmu42/KML-original-124-without-ROMs.zip
Or you can download the KML scripts from the original Emu42 Windows application archive (https://hp.giesselink.com/emu42.htm)
and you can extract the ROM file from a real calculator (or be lucky on internet).
Be careful about the case sensitivity of the filename in the KML script (Linux is case sensitive, not Windows).

The application does not request any permission (because it opens the files or the KML folders using the content:// scheme).

The application is distributed with the same license under GPL and you can find the source code here:
https://github.com/dgis/emu42android


QUICK START

1. From the left side, slide your finger to open the menu.
2. Touch the "New..." menu item.
3. "Select a Custom KML script folder..." where you have copied the KML scripts and ROM files (Android 11 cannot use the folder Download).
4. Pick a calculator.
5. And the calculator should now be opened.


NOTES

- For technical reason, this application need the Android 5.0 (API 21).
- The Help menu displays Emu42's original help HTML page and may not accurately reflect the behavior of this Android version.
- When using a custom KML script by selecting a folder (Not the folder Download for Android 11), you must take care of the case sensitivity of its dependency files.
- This Emulator does not include the ROM files or the KML files.
- To speed up printing, set the 'delay' to 0 in the calculator's print options.


NOT WORKING YET

- Disassembler
- Debugger


LINKS

- Original Emu42 for Windows from Christoph Giesselink and Sébastien Carlier: https://hp.giesselink.com/emu42.htm
- The Museum of HP Calculators Forum: https://www.hpmuseum.org/forum/thread-12540.html


CHANGES

Version 1.6 (2021-09-xx)

- Show KML log on request.
- Update from the original source code Emu42 version 1.26 from Christoph Gießelink.
- Update the embedded help file "Emu42.html" to the latest version.


Version 1.5 (2021-04-07)

- Replaces the haptic feedback switch with a slider to adjust the vibration duration.
- Fix transparency issue (RGB -> BGR).
- Fix a printer issue from Christoph Gießelink's HP82240B Printer Simulator version 1.12.
- Fix the KML button Type 3 with a Background offset which was not display at the right location. But Type 3 does not work very well with Emu42.
- Fix a timer issue.
- Fix an issue which prevents to save all the settings (Save in onPause instead of onStop).
- The KML folder is now well saved when changing the KML script for a custom one via the menu "Change KML Script...".
- Fix an issue when the permission to read the KML folder has been lost.
- Allows pressing a calculator button with the right button of the mouse but prevents its release to allow the On+A+F key combination (with Android version >= 5.0).
- Update the embedded help file "Emu42.html" to the latest version.
- Open an external web browser when you click an external links in the Help.


Version 1.4 (2020-09-07)

- Update from the original source code Emu42 version 1.25 from Christoph Gießelink (which can read state file with KML url longer than 256 byte).
- If the KML folder does not exist (like the first time), prompt the user to choose a new KML folder.
- Move the KML folder in the JSON settings embedded in the state file because Windows cannot open the state file with KML url longer than 256 byte.
- Prevent to auto save before launching the "Open...", "Save As...", "Load Object...", "Save Object...", etc...
- Prevent app not responding (ANR) in NativeLib.buttonUp().
- In the menu header, switch the pixel format RGB to BGR when an icon of type BMP is defined in the KML script.


Version 1.3 (2020-05-24)

- Intercept the ESC keyboard key to allow the use of the BACK soft key.
- Add LCD pixel borders (not for Sacajawea and Bert).
- Add support for the dark theme.
- Remove the non loadable file from the MRU file list.
- Fix: Overlapping window source position when Background/Offset is not (0,0).
- Wrap the table of content in the former help documentation.
- Save the settings at the end of the state file.
- Fix: Copy screen not working with Sacajawea.
- Transform all child activities with dialog fragments (to prevent unwanted state save).
- Fix an issue with the numpad keys which send the arrow keys and the numbers at the same time.
- Fix a major issue which prevented to open a state file (with a custom KML script) with Android 10.
- Optimize the speed with -Ofast option.


Version 1.2 (2019-12-12)

- Update from the original source code Emu42 version 1.24 from Christoph Gießelink which supports HP10B, HP20S, HP21S too.
- Add option to prevent the pinch zoom.
- Prevent the white bottom bar when both options "Hide the status/navigation bar" and "Hide the menu button" are set (Github Fix: #9).
- Prevent the BACK/ESCAPE key to end the application only from a hardware keyboard (Github Fix: #10).
- Add the KML Icon if present in the navigation menu header (only support PNG or 32bits BMP in the ICO file).
- Add an optional overlapping LCD part stuck to the screen when swiping the 2 calc parts.
- Improve loading speed by caching the KML folder.
- Make the setting Layout/Auto Zoom by default.
- Support the transparency in the KML Global Color.
- Improve the New and Save menus.
- Sound volume can be adjusted by number by touching the number.


Version 1.1 (2019-07-11)

- Add the Ir printer simulator based on the Christoph Giesselink's HP82240B Printer Simulator for Windows.
- Add the macro support.
- Add a volume slider in the settings.
- Add a rotation option.
- Refactor the code for easier code sharing between Emu48, Emu42 and Emu71.
- Fix: Bad text characters when copy/paste the stack.
- Fix a crash with waveOutClose().
- Fix an issue with the Pan and zoom which was possible after closing the calc.
- Prevent the ESC key from leaving the application (Github Fix: #6).
- Map the keyboard DELETE key like it should (Github Fix: #6).
- Map the +, -, * and / keys catching the typed character instead of the virtual key (Github Fix: #6).
- Improve the swipe gesture.


Version 1.0 (2019-06-05)

- First public version available on the store. It is based on Emu42 version 1.22 from Christoph Gießelink.


LICENSES

Android version by Régis COSNIER.
This program is based on Emu42 for Windows version, copyrighted by Christoph Gießelink & Sébastien Carlier.

This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation; either version 2 of the License, or (at your option) any later version.
This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
You should have received a copy of the GNU General Public License along with this program; if not, write to the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
