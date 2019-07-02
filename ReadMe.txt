DESCRIPTION

This project ports the Windows application Emu42 written in C to Android.
It uses the Android NDK. The former Emu42 source code (written by Christoph Giesselink) remains untouched because of a thin win32 emulation layer above Linux/NDK!
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
4. Pick a calculator.
5. And the calculator should now be opened.


NOTES

- For technical reason, this application need the Android 5.0 (API 21).
- The Help menu displays Emu42's original help HTML page and may not accurately reflect the behavior of this Android version.
- When using a custom KML script by selecting a folder, you must take care of the case sensitivity of its dependency files.
- This Emulator does not include the ROM files or the KML files.
- To speed up printing, set the 'delay' to 0 in the calculator's print options.


NOT WORKING YET

- Disassembler
- Debugger


CHANGES

Version 1.1 (2019-07-XX)

- Add a volume slider in the settings.
- Add a rotation option.
- Add the Ir printer simulator (set delay to 0 to speed up!).
- Add the macro support.
- Refactor the code for easier code sharing between Emu48, Emu42 and Emu71.
- Fix: Bad text characters when copy/paste the stack.
- Fix a crash with waveOutClose().
- Fix an issue with the Pan and zoom which was possible after closing the calc.
- Prevent the ESC key from leaving the application (Github Fix: #6).
- Map the keyboard DELETE key like it should (Github Fix: #6).
- Map the +, -, * and / keys catching the typed character instead of the virtual key (Github Fix: #6).


Version 1.0 (2019-06-05)

- First public version available on the store. It is based on Emu42 version 1.22 from Christoph Gießelink.


LICENSES

Android version by Régis COSNIER.
This program is based on Emu42 for Windows version, copyrighted by Christoph Gießelink & Sébastien Carlier.

This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation; either version 2 of the License, or (at your option) any later version.
This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
You should have received a copy of the GNU General Public License along with this program; if not, write to the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.


TODO

- Improve the swipe gesture.
- To have the LCD part stick to the screen when swiping the 2 calc parts (Vincent Weber).
- Anyway that the layout settings (zoom mode, fill screen...) be part of the saved state, rather than being global to the app (Vincent Weber).


DONE

- Improve the access to the menu by adding an optional icon in the top left of the screen.
- After uninstalling and the installing the application, we lose the permission to read in the KML and ROM folder, so, in this case, we now prompt the user to select again this folder.
    I thinks it is this issue: 1/Sometimes loading a kml would systematically fail, with an error on loading the .kmi file. I first thought it was a case sensitivity problem in the filename, but this didn't help. But exiting the app and relaunching it made it work. Very strange.
- Fix the screen issue with the 14B and 32SII.
- Zoom auto depending of the screen ratio (adjust when rotate).
- Add the possibility to scroll or swipe the screen.
    4/Generally speaking the KMLs and skins would need adaptation for Android. The 28S and 19BII are not usable on a phone, too small. Would need to be broken in 2 parts with toggling, like go28s does. Not sure it is possible with KMLs. Even the other calcs have a lot of wasted space, the keys are small.
- Show the LCD in the OSD when zooming.
    3/The 32SII and 14B scripts, which work fine on PC, do not work properly. The annunciators are sticky, you never know whether the next key is going to be shifted or not..
- Fix the issue with the bad screen color (red/blue or event green)!
- Change the icon.


BUILD

Emu42 for Android is built with Android Studio 3.4 (2019).
And to generate an installable APK file with a real Android device, it MUST be signed.

Either use Android Studio:
* In menu "Build"
* Select "Generate Signed Bundle / APK..."
* Select "APK", then "Next"
* "Create new..." (or use an existing key store file)
* Enter "Key store password", "Key alias" and "Key password", then "Next"
* Select a "Destination folder:"
* Select the "Build Variants:" "release"
* Select the "Signature Versions:" "V1" (V1 only)
* Finish

Or in the command line, build the signed APK:
* In the root folder, create a keystore.jks file with:
** keytool -genkey -keystore ./keystore.jks -keyalg RSA -validity 9125 -alias key0
** (or keytool -genkeypair -v -keystore ./keystore.jks -keyalg RSA -validity 9125 -alias key0)
* create the file ./keystore.properties , with the following properties:
    storeFile=../keystore.jks
    storePassword=myPassword
    keyAlias=key0
    keyPassword=myPassword
* gradlew build
* The APK should be in the folder app/build/outputs/apk/release

Then, you should be able to use this fresh APK file with an Android device.
