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

Version 1.2 (2019-07-XX)

- Add option to prevent the pinch zoom.
- Prevent the white bottom bar when both options "Hide the status/navigations bar" and "Hide the menu button" are set (Github Fix: #9).
- Prevent the BACK/ESCAPE key to end the application only from a hardware keyboard (Github Fix: #10).


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


TODO

- Try to include the KML files without the ROMs.
- To have the LCD part stick to the screen when swiping the 2 calc parts (Vincent Weber).
    * Need to refresh the layout after a change from manual to auto (and may be after a screen rotation).
- Sometimes, the calculator seems to lag and finally freeze.
- Anyway that the layout settings (zoom mode, fill screen...) be part of the saved state, rather than being global to the app (Vincent Weber).


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
