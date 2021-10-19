// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

package org.emulator.forty.two;

import android.app.Activity;
import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.res.AssetManager;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.ParcelFileDescriptor;
import android.os.Vibrator;
import android.util.Log;
import android.util.SparseArray;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.MenuItem;
import android.view.SubMenu;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.FrameLayout;
import android.widget.ImageButton;
import android.widget.ImageView;
import android.widget.TextView;
import android.widget.Toast;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.appcompat.app.AlertDialog;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.content.FileProvider;
import androidx.core.view.GravityCompat;
import androidx.documentfile.provider.DocumentFile;
import androidx.drawerlayout.widget.DrawerLayout;
import androidx.fragment.app.Fragment;

import com.google.android.material.navigation.NavigationView;

import org.emulator.calculator.EmuApplication;
import org.emulator.calculator.InfoFragment;
import org.emulator.calculator.InfoWebFragment;
import org.emulator.calculator.LCDOverlappingView;
import org.emulator.calculator.MainScreenView;
import org.emulator.calculator.NativeLib;
import org.emulator.calculator.PrinterSimulator;
import org.emulator.calculator.PrinterSimulatorFragment;
import org.emulator.calculator.Settings;
import org.emulator.calculator.Utils;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.nio.ByteBuffer;
import java.nio.charset.StandardCharsets;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.Date;
import java.util.HashMap;
import java.util.HashSet;
import java.util.LinkedHashMap;
import java.util.LinkedList;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Set;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

public class MainActivity extends AppCompatActivity implements NavigationView.OnNavigationItemSelectedListener {

    private static final String TAG = "MainActivity";
	private final boolean debug = false;
	private Settings settings;
    private NavigationView navigationView;
    private DrawerLayout drawer;
    private MainScreenView mainScreenView;
    private LCDOverlappingView lcdOverlappingView;
    private ImageButton imageButtonMenu;
	private Vibrator vibrator;

    public static final int INTENT_GETOPENFILENAME = 1;
    public static final int INTENT_GETSAVEFILENAME = 2;
    public static final int INTENT_OBJECT_LOAD = 3;
    public static final int INTENT_OBJECT_SAVE = 4;
    public static final int INTENT_PORT2LOAD = 6;
    public static final int INTENT_PICK_KML_FOLDER_FOR_NEW_FILE = 7;
    public static final int INTENT_PICK_KML_FOLDER_FOR_CHANGING = 8;
	public static final int INTENT_PICK_KML_FOLDER_FOR_SECURITY = 10;
	public static final int INTENT_PICK_KML_FOLDER_FOR_KML_NOT_FOUND = 11;
    public static final int INTENT_CREATE_RAM_CARD = 12;
    public static final int INTENT_MACRO_LOAD = 13;
    public static final int INTENT_MACRO_SAVE = 14;
	public static final int INTENT_CREATE_FLASH_ROM = 15;
	public static final int INTENT_LOAD_FLASH_ROM = 16;

	public static String intentPickKmlFolderForUrlToOpen;

	private final String kmlMimeType = "application/vnd.google-earth.kml+xml";
    private boolean kmlFolderUseDefault = true;
    private String kmlFolderURL = "";
	private boolean kmlFolderChange = true;

	private boolean saveWhenLaunchingActivity = true;

    private int selectedRAMSize = -1;
    private boolean[] objectsToSaveItemChecked = null;

    // Most Recently Used state files
    private final int MRU_ID_START = 10000;
    private final int MAX_MRU = 5;
    private LinkedHashMap<String, String> mruLinkedHashMap = new LinkedHashMap<String, String>(5, 1.0f, true) {
        @Override
        protected boolean removeEldestEntry(Map.Entry eldest) {
            return size() > MAX_MRU;
        }
    };
	private HashMap<Integer, String> mruByMenuId = new HashMap<>();

    private final PrinterSimulator printerSimulator = new PrinterSimulator();
    private final PrinterSimulatorFragment fragmentPrinterSimulator = new PrinterSimulatorFragment();
    private Bitmap bitmapIcon;


    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        drawer = findViewById(R.id.drawer_layout);

        navigationView = findViewById(R.id.nav_view);
        navigationView.setNavigationItemSelectedListener(this);

	    settings = EmuApplication.getSettings();
	    settings.setIsDefaultSettings(true);

	    vibrator = (Vibrator) getSystemService(Context.VIBRATOR_SERVICE);

        ViewGroup mainScreenContainer = findViewById(R.id.main_screen_container);
        mainScreenView = new MainScreenView(this);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP)
            mainScreenView.setStatusBarColor(getWindow().getStatusBarColor());
        mainScreenContainer.addView(mainScreenView, 0, new ViewGroup.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.MATCH_PARENT));

        lcdOverlappingView = new LCDOverlappingView(this, mainScreenView);
        lcdOverlappingView.setVisibility(View.GONE);
        mainScreenContainer.addView(lcdOverlappingView, 1, new FrameLayout.LayoutParams(0, 0));

        imageButtonMenu = findViewById(R.id.button_menu);
        imageButtonMenu.setOnClickListener(v -> {
            if(drawer != null)
                drawer.openDrawer(GravityCompat.START);
        });
        showCalculatorView(false);

        AssetManager assetManager = getResources().getAssets();
        NativeLib.start(assetManager, this);

        // By default Port1 is set
        setPort1Settings(true, true);

        Set<String> savedMRU = settings.getStringSet("MRU", null);
        if(savedMRU != null) {
            for (String url : savedMRU) {
                if(url != null && !url.isEmpty())
                    mruLinkedHashMap.put(url, null);
            }
        }
        updateMRU();
        updateFromPreferences(null, false);

        updateNavigationDrawerItems();


        fragmentPrinterSimulator.setPrinterSimulator(printerSimulator);
        printerSimulator.setOnPrinterOutOfPaperListener((currentLine, maxLine, currentPixelRow, maxPixelRow) -> runOnUiThread(
                () -> Toast.makeText(MainActivity.this, String.format(Locale.US, getString(R.string.message_printer_out_of_paper), maxLine, maxPixelRow), Toast.LENGTH_LONG).show()
                )
        );


        //android.os.Debug.waitForDebugger();


        String documentToOpenUrl = settings.getString("lastDocument", "");
        Uri documentToOpenUri;
        Intent intent = getIntent();
        if(intent != null) {
            String action = intent.getAction();
            if(action != null) {
                if (action.equals(Intent.ACTION_VIEW)) {
                    documentToOpenUri = intent.getData();
                    if (documentToOpenUri != null) {
                        String scheme = documentToOpenUri.getScheme();
                        if(scheme != null && scheme.compareTo("file") == 0)
                            documentToOpenUrl = null;
                        else
                            documentToOpenUrl = documentToOpenUri.toString();
                    }
                } else if (action.equals(Intent.ACTION_SEND)) {
                    documentToOpenUri = intent.getParcelableExtra(Intent.EXTRA_STREAM);
                    if (documentToOpenUri != null) {
                        documentToOpenUrl = documentToOpenUri.toString();
                    }
                }
            }
        }

        if(documentToOpenUrl != null && documentToOpenUrl.length() > 0)
            try {
            	// FileOpen auto-open.
                onFileOpen(documentToOpenUrl, intent, null);
            } catch (SecurityException e) {
	            showAlert(getString(R.string.message_open_security_open_last_document), true);
	            if(debug && e.getMessage() != null) Log.e(TAG, e.getMessage());
            } catch (Exception e) {
                if(debug && e.getMessage() != null) Log.e(TAG, e.getMessage());
            }
        else if(drawer != null)
            drawer.openDrawer(GravityCompat.START);
    }

    private void updateMRU() {
        Menu menu = navigationView.getMenu();
        MenuItem recentsMenuItem = menu.findItem(R.id.nav_item_recents);
        SubMenu recentsSubMenu = null;
        if(recentsMenuItem != null) {
            recentsSubMenu = recentsMenuItem.getSubMenu();
            if (recentsSubMenu != null)
                recentsSubMenu.clear();
        }
	    mruByMenuId.clear();
        if (recentsSubMenu != null) {
            Set<String> mruLinkedHashMapKeySet = mruLinkedHashMap.keySet();
            String[] mrus = mruLinkedHashMapKeySet.toArray(new String[0]);
            for (int i = mrus.length - 1; i >= 0; i--) {
            	String mostRecentlyUsedFile = mrus[i];
                String displayName = getFilenameFromURL(mostRecentlyUsedFile);
                if(displayName == null || displayName.equals("") || displayName.equals(mostRecentlyUsedFile)) {
                	// We should remove this file because it seems impossible to get the display name of this Most Recently Used state file.
	                // It might be deleted or the permissions does not allow to reach it anymore.
	                mruLinkedHashMap.remove(mostRecentlyUsedFile);
                } else {
                	int menuId = MRU_ID_START + i;
	                mruByMenuId.put(menuId, mostRecentlyUsedFile);
	                recentsSubMenu.add(Menu.NONE, menuId, Menu.NONE, displayName);
                }
            }
        }
    }

    @Override
    protected void onPause() {
    	// Following the Activity lifecycle (https://developer.android.com/reference/android/app/Activity),
	    // the data must be saved in the onPause() event instead of onStop() simply because the onStop
	    // method is killable and may lead to the data half saved!
	    if(saveWhenLaunchingActivity) {
		    settings.putStringSet("MRU", mruLinkedHashMap.keySet());
		    if (lcdOverlappingView != null)
			    lcdOverlappingView.saveViewLayout();

		    if (NativeLib.isDocumentAvailable() && settings.getBoolean("settings_autosave", true)) {
			    String currentFilename = NativeLib.getCurrentFilename();
			    if (currentFilename != null && currentFilename.length() > 0)
				    onFileSave();
		    }

		    settings.saveApplicationSettings();

		    clearFolderCache();
	    }

        super.onPause();
    }

    @Override
    protected void onDestroy() {
        //onDestroy is never called!
        NativeLib.stop();
        super.onDestroy();
    }

    @Override
    public void onBackPressed() {
        if (drawer.isDrawerOpen(GravityCompat.START)) {
            drawer.closeDrawer(GravityCompat.START);
        } else {
            super.onBackPressed();
        }
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        // Inflate the menu; this adds items to the action bar if it is present.
        getMenuInflater().inflate(R.menu.menu_main, menu);
        return true;
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        // Handle action bar item clicks here. The action bar will
        // automatically handle clicks on the Home/Up button, so long
        // as you specify a parent activity in AndroidManifest.xml.
        int id = item.getItemId();

        //noinspection SimplifiableIfStatement
        if (id == R.id.action_settings) {
            OnSettings();
            return true;
        }

        return super.onOptionsItemSelected(item);
    }

    @Override
    public boolean onNavigationItemSelected(@NonNull MenuItem item) {
        // Handle navigation view item clicks here.
        int id = item.getItemId();
        if (id == R.id.nav_new) {
            OnFileNew();
        } else if (id == R.id.nav_open) {
            OnFileOpen();
        } else if (id == R.id.nav_save) {
            OnFileSave();
        } else if (id == R.id.nav_save_as) {
            OnFileSaveAs();
        } else if (id == R.id.nav_close) {
            OnFileClose();
        } else if (id == R.id.nav_settings) {
            OnSettings();
        } else if (id == R.id.nav_load_object) {
            OnObjectLoad();
        } else if (id == R.id.nav_save_object) {
            OnObjectSave();
        } else if (id == R.id.nav_copy_fullscreen) {
            OnViewCopyFullscreen();
        } else if (id == R.id.nav_copy_screen) {
            OnViewCopy();
        } else if (id == R.id.nav_copy_stack) {
            OnStackCopy();
        } else if (id == R.id.nav_paste_stack) {
            OnStackPaste();
        } else if (id == R.id.nav_reset_calculator) {
            OnViewReset();
        } else if (id == R.id.nav_backup_save) {
            OnBackupSave();
        } else if (id == R.id.nav_backup_restore) {
            OnBackupRestore();
        } else if (id == R.id.nav_backup_delete) {
            OnBackupDelete();
        } else if (id == R.id.nav_change_kml_script) {
            OnViewScript();
        } else if (id == R.id.nav_show_kml_script_compilation_result) {
            showKMLLogForce();
        } else if (id == R.id.nav_show_printer) {
            OnViewPrinter();
//        } else if (id == R.id.nav_create_ram_card) {
//            OnCreateRAMCard();
//        } else if (id == R.id.nav_manage_flash_rom) {
//	        OnManageFlashROM();
        } else if (id == R.id.nav_macro_record) {
            OnMacroRecord();
        } else if (id == R.id.nav_macro_play) {
            OnMacroPlay();
        } else if (id == R.id.nav_macro_stop) {
            OnMacroStop();
        } else if (id == R.id.nav_help) {
            OnTopics();
        } else if (id == R.id.nav_about) {
            OnAbout();
        } else if(id >= MRU_ID_START && id < MRU_ID_START + MAX_MRU) {
	        String url = mruByMenuId.get(id);
			if(url != null) {
				// Increase the file usage count
				mruLinkedHashMap.get(url);

				ensureDocumentSaved(() -> {
					// FileOpen from MRU.
					onFileOpen(url, null, null);
				});
			}
        }

        drawer.closeDrawer(GravityCompat.START);
        return true;
    }

    private void removeMRU(String url) {
        // We should remove this file from the MRU list because it might be deleted or the permissions does not allow to reach it anymore.
	    if(mruLinkedHashMap.containsKey(url)) {
		    mruLinkedHashMap.remove(url);
		    navigationView.post(this::updateMRU);
	    }
    }

    private void updateNavigationDrawerItems() {
        Menu menu = navigationView.getMenu();
        boolean isBackup = NativeLib.isBackup();
        int cCurrentRomType = NativeLib.getCurrentModel();
        int nMacroState = NativeLib.getMacroState();

        boolean uRun = NativeLib.isDocumentAvailable();
        boolean bObjectEnable = (cCurrentRomType == 'D' || cCurrentRomType == 'L' || cCurrentRomType == 'N' || cCurrentRomType == 'O');
        boolean bStackCEnable = (cCurrentRomType == 'A' || cCurrentRomType == 'C' || cCurrentRomType == 'D' || cCurrentRomType == 'E' || cCurrentRomType == 'F' || cCurrentRomType == 'I' || cCurrentRomType == 'L' || cCurrentRomType == 'M' || cCurrentRomType == 'N' || cCurrentRomType == 'O' || cCurrentRomType == 'T' || cCurrentRomType == 'U' || cCurrentRomType == 'Y');
        boolean bStackPEnable = (cCurrentRomType == 'D' || cCurrentRomType == 'O');

        menu.findItem(R.id.nav_save).setEnabled(uRun);
        menu.findItem(R.id.nav_save_as).setEnabled(uRun);
        menu.findItem(R.id.nav_close).setEnabled(uRun);
        menu.findItem(R.id.nav_load_object).setEnabled(uRun && bObjectEnable);
        menu.findItem(R.id.nav_save_object).setEnabled(uRun && bObjectEnable);
        menu.findItem(R.id.nav_copy_screen).setEnabled(uRun);
        menu.findItem(R.id.nav_copy_stack).setEnabled(uRun && bStackCEnable);
        menu.findItem(R.id.nav_paste_stack).setEnabled(uRun && bStackPEnable);
        menu.findItem(R.id.nav_reset_calculator).setEnabled(uRun);
        menu.findItem(R.id.nav_backup_save).setEnabled(uRun);
        menu.findItem(R.id.nav_backup_restore).setEnabled(uRun && isBackup);
        menu.findItem(R.id.nav_backup_delete).setEnabled(uRun && isBackup);
        menu.findItem(R.id.nav_change_kml_script).setEnabled(uRun);
        menu.findItem(R.id.nav_show_kml_script_compilation_result).setEnabled(uRun);
        menu.findItem(R.id.nav_macro_record).setEnabled(uRun && nMacroState == 0 /* MACRO_OFF */);
        menu.findItem(R.id.nav_macro_play).setEnabled(uRun && nMacroState == 0 /* MACRO_OFF */);
        menu.findItem(R.id.nav_macro_stop).setEnabled(uRun && nMacroState != 0 /* MACRO_OFF */);
    }

	private void loadKMLFolder() {
		kmlFolderUseDefault = settings.getBoolean("settings_kml_default", true);
		if (!kmlFolderUseDefault) {
			kmlFolderURL = settings.getString("settings_kml_folder", "");
			// https://github.com/googlesamples/android-DirectorySelection
			// https://stackoverflow.com/questions/44185477/intent-action-open-document-tree-doesnt-seem-to-return-a-real-path-to-drive/44185706
			// https://stackoverflow.com/questions/26744842/how-to-use-the-new-sd-card-access-api-presented-for-android-5-0-lollipop
		}
		kmlFolderChange = true;
	}

	private void changeKMLFolder(String url) {
		if(url == null || url.startsWith("assets/")) {
			if(!kmlFolderUseDefault)
				kmlFolderChange = true;
			kmlFolderUseDefault = true;
			settings.putBoolean("settings_kml_default", true);
		} else {
			if(kmlFolderUseDefault || !url.equals(kmlFolderURL))
				kmlFolderChange = true;
			kmlFolderUseDefault = false;
			kmlFolderURL = url;
			settings.putBoolean("settings_kml_default", false);
			settings.putString("settings_kml_folder", kmlFolderURL);
		}
	}

    static class KMLScriptItem {
        public String filename;
	    public String folder;
        public String title;
        public String model;
    }
    ArrayList<KMLScriptItem> kmlScripts;
    private void extractKMLScripts() {
        if(kmlScripts == null || kmlFolderChange) {
            kmlFolderChange = false;

            kmlScripts = new ArrayList<>();

            if(kmlFolderUseDefault) {
            	// We use the default KML scripts and ROMs from the embedded asset folder inside the Android app.
                AssetManager assetManager = getAssets();
                String[] calculatorsAssetFilenames = new String[0];
                try {
                    calculatorsAssetFilenames = assetManager.list("calculators");
                } catch (IOException e) {
                    e.printStackTrace();
                }
                kmlScripts.clear();
                if(calculatorsAssetFilenames != null) {
                    Pattern patternGlobalTitle = Pattern.compile("\\s*Title\\s+\"(.*)\"");
                    Pattern patternGlobalModel = Pattern.compile("\\s*Model\\s+\"(.*)\"");
                    Matcher m;
                    for (String calculatorFilename : calculatorsAssetFilenames) {
                        if (calculatorFilename.toLowerCase().lastIndexOf(".kml") != -1) {
	                        try (BufferedReader reader = new BufferedReader(new InputStreamReader(assetManager.open("calculators/" + calculatorFilename), StandardCharsets.UTF_8))) {
		                        // do reading, usually loop until end of file reading
		                        String mLine;
		                        boolean inGlobal = false;
		                        String title = null;
		                        String model = null;
		                        while ((mLine = reader.readLine()) != null) {
			                        //process line
			                        if (mLine.indexOf("Global") == 0) {
				                        inGlobal = true;
				                        title = null;
				                        model = null;
				                        continue;
			                        }
			                        if (inGlobal) {
				                        if (mLine.indexOf("End") == 0) {
					                        KMLScriptItem newKMLScriptItem = new KMLScriptItem();
					                        newKMLScriptItem.filename = calculatorFilename;
					                        newKMLScriptItem.folder = null;
					                        newKMLScriptItem.title = title;
					                        newKMLScriptItem.model = model;
					                        kmlScripts.add(newKMLScriptItem);
					                        break;
				                        }

				                        m = patternGlobalTitle.matcher(mLine);
				                        if (m.find()) {
					                        title = m.group(1);
				                        }
				                        m = patternGlobalModel.matcher(mLine);
				                        if (m.find()) {
					                        model = m.group(1);
				                        }
			                        }
		                        }
	                        } catch (IOException e) {
		                        //log the exception
		                        e.printStackTrace();
	                        }
                        }
                    }
                }
            } else {
	            // We use the custom KML scripts and ROMs from a chosen folder.
                Uri kmlFolderUri = Uri.parse(kmlFolderURL);
                List<String> calculatorsAssetFilenames = new LinkedList<>();

                DocumentFile kmlFolderDocumentFile = DocumentFile.fromTreeUri(this, kmlFolderUri);
                if(kmlFolderDocumentFile != null) {
	                for (DocumentFile file : kmlFolderDocumentFile.listFiles()) {
	                    String url = file.getUri().toString();
	                    String name = file.getName();
	                    String mime = file.getType();
		                if(debug) Log.d(TAG, "url: " + url + ", name: " + name + ", mime: " + mime);
	                    if(kmlMimeType.equals(mime)) {
	                        calculatorsAssetFilenames.add(url);
	                    }
	                }
                }
                kmlScripts.clear();
                Pattern patternGlobalTitle = Pattern.compile("\\s*Title\\s+\"(.*)\"");
                Pattern patternGlobalModel = Pattern.compile("\\s*Model\\s+\"(.*)\"");
                Matcher m;
                for (String calculatorFilename : calculatorsAssetFilenames) {
                    BufferedReader reader = null;
                    try {
                        Uri calculatorsAssetFilenameUri = Uri.parse(calculatorFilename);
                        DocumentFile documentFile = DocumentFile.fromSingleUri(this, calculatorsAssetFilenameUri);
                        if(documentFile != null) {
                            Uri fileUri = documentFile.getUri();
                            InputStream inputStream = getContentResolver().openInputStream(fileUri);
                            if(inputStream != null) {
                                reader = new BufferedReader(new InputStreamReader(inputStream, StandardCharsets.UTF_8));
                                // do reading, usually loop until end of file reading
                                String mLine;
                                boolean inGlobal = false;
                                String title = null;
                                String model = null;
                                while ((mLine = reader.readLine()) != null) {
                                    //process line
                                    if (mLine.indexOf("Global") == 0) {
                                        inGlobal = true;
                                        title = null;
                                        model = null;
                                        continue;
                                    }
                                    if (inGlobal) {
                                        if (mLine.indexOf("End") == 0) {
                                            KMLScriptItem newKMLScriptItem = new KMLScriptItem();
	                                        newKMLScriptItem.filename = documentFile.getName();
	                                        newKMLScriptItem.folder = kmlFolderURL;
                                            newKMLScriptItem.title = title;
                                            newKMLScriptItem.model = model;
                                            kmlScripts.add(newKMLScriptItem);
                                            break;
                                        }

                                        m = patternGlobalTitle.matcher(mLine);
                                        if (m.find()) {
                                            title = m.group(1);
                                        }
                                        m = patternGlobalModel.matcher(mLine);
                                        if (m.find()) {
                                            model = m.group(1);
                                        }
                                    }
                                }
                            }
                        }
                    } catch (IOException e) {
                        //log the exception
                        e.printStackTrace();
                    } finally {
                        if (reader != null) {
                            try {
                                reader.close();
                            } catch (IOException e) {
                                //log the exception
                            }
                        }
                    }
                }
            }

            Collections.sort(kmlScripts, (lhs, rhs) -> lhs.title.compareTo(rhs.title));
        }
    }

    private Runnable fileSaveAsCallback = null;
    private void ensureDocumentSaved(Runnable continueCallback) {
        ensureDocumentSaved(continueCallback, false, false);
    }
    private void ensureDocumentSaved(Runnable continueCallback, boolean forceRequest) {
        ensureDocumentSaved(continueCallback, forceRequest, false);
    }
    private void ensureDocumentSaved(Runnable continueCallback, boolean forceRequest, boolean simpleSave) {
        if(NativeLib.isDocumentAvailable()) {
            String currentFilename = NativeLib.getCurrentFilename();
            boolean hasFilename = (currentFilename != null && currentFilename.length() > 0);

            DialogInterface.OnClickListener onClickListener = (dialog, which) -> {
                if (which == DialogInterface.BUTTON_POSITIVE) {
                    if (hasFilename) {
	                    onFileSave();
                        if (continueCallback != null)
                            continueCallback.run();
                    } else {
                        fileSaveAsCallback = continueCallback;
                        OnFileSaveAs();
                    }
                } else if (which == DialogInterface.BUTTON_NEGATIVE) {
                    if(continueCallback != null)
                        continueCallback.run();
                }
            };

            if(simpleSave || (!forceRequest && hasFilename && settings.getBoolean("settings_autosave", true))) {
                onClickListener.onClick(null, DialogInterface.BUTTON_POSITIVE);
            } else {
                new AlertDialog.Builder(this)
                        .setMessage(getString(R.string.message_do_you_want_to_save))
                        .setPositiveButton(getString(R.string.message_yes), onClickListener)
                        .setNegativeButton(getString(R.string.message_no), onClickListener)
                        .show();
            }
        } else if(continueCallback != null)
            continueCallback.run();
    }

    private void OnFileNew() {
        // By default Port1 is set
        setPort1Settings(true, true);

        ensureDocumentSaved(() -> showKMLPicker(false) );
    }

    private void newFileFromKML(String kmlScriptFilename, String kmlScriptFolder) {
	    // Eventually, close the previous state file
	    NativeLib.onFileClose();
	    showCalculatorView(false);
	    displayFilename("");

	    // Reset the embedded settings to the common settings
	    settings.setIsDefaultSettings(false);
	    settings.clearEmbeddedStateSettings();

	    // Update the Emu VM with the new settings
	    updateFromPreferences(null, false);

	    // Create a new genuine state file
	    int result = NativeLib.onFileNew(kmlScriptFilename, kmlScriptFolder);
        if(result > 0) {
            showCalculatorView(true);
            displayFilename("");
            showKMLLog();
            if(kmlScriptFolder != null) {
	            // Note: kmlScriptFolder should be equal to kmlFolderURL!
	            // We keep the global settings_kml_folder distinct from embedded settings_kml_folder_embedded.
	            settings.putString("settings_kml_folder_embedded", kmlScriptFolder);
            }
            suggestToSaveNewFile();
        } else
            showKMLLogForce();
        updateNavigationDrawerItems();
    }

    private void suggestToSaveNewFile() {
        new AlertDialog.Builder(this)
                .setMessage(getString(R.string.message_save_new_file))
                .setPositiveButton(android.R.string.yes, (dialog, which) -> OnFileSaveAs())
                .setNegativeButton(android.R.string.no, (dialog, which) -> {})
                .show();
    }

    private void OnFileOpen() {
        ensureDocumentSaved(() -> {
            Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
            intent.addCategory(Intent.CATEGORY_OPENABLE);
            intent.setType("*/*");
            intent.putExtra(Intent.EXTRA_TITLE, getString(R.string.filename) + "-state.e42");
	        saveWhenLaunchingActivity = false;
            startActivityForResult(intent, INTENT_GETOPENFILENAME);
        });
    }
    private void OnFileSave() {
        ensureDocumentSaved(null, false, true);
    }
    private void OnFileSaveAs() {
        Intent intent = new Intent(Intent.ACTION_CREATE_DOCUMENT);
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        intent.setType("*/*");
        int model = NativeLib.getCurrentModel();
        String extension = "e42";
        switch (model) {
            case 'E': // HP10B # Ernst
                extension = "e10";
                break;
            case 'I': // HP14B # Midas
                extension = "e14";
                break;
            case 'T': // HP17B # Trader
            case 'U': // HP17BII # Trader II
                extension = "e17";
                break;
            case 'Y': // HP19BII # Tycoon II
                extension = "e19";
                break;
            case 'F': // HP20S # Erni
                extension = "e20";
                break;
            case 'C': // HP21S # Monte Carlo
                extension = "e21";
                break;
            case 'A': // HP22S # Plato
                extension = "e22";
                break;
            case 'M': // HP27S # Mentor
                extension = "e27";
                break;
            case 'O': // HP28S # Orlando
                extension = "e28";
                break;
            case 'L': // HP32S # Leonardo
            case 'N': // HP32SII # Nardo
                extension = "e32";
                break;
            case 'D': // HP42S # Davinci
                extension = "e42";
                break;
        }
        intent.putExtra(Intent.EXTRA_TITLE, getString(R.string.filename) + "-state." + extension);
	    saveWhenLaunchingActivity = false;
        startActivityForResult(intent, INTENT_GETSAVEFILENAME);
    }
    private void OnFileClose() {
        ensureDocumentSaved(() -> {
            NativeLib.onFileClose();
	        settings.setIsDefaultSettings(true);
	        settings.clearEmbeddedStateSettings();
            showCalculatorView(false);
            saveLastDocument("");
            updateNavigationDrawerItems();
            displayFilename("");
            if(drawer != null) {
                new android.os.Handler().postDelayed(() -> drawer.openDrawer(GravityCompat.START), 300);
            }
        }, true);
    }

    private void OnSettings() {
	    SettingsFragment settingsFragment = new SettingsFragment();
	    settingsFragment.registerOnSettingsKeyChangedListener(settingsKeyChanged -> {
		    HashSet<String> changedKeysCleaned = new HashSet<>();
		    for (String key : settingsKeyChanged) {
			    if(debug) Log.d(TAG, "ChangedKey): " + key);
			    switch (key) {
				    case "settings_port1en":
				    case "settings_port1wr":
					    changedKeysCleaned.add("settings_port1");
					    break;
				    case "settings_port2en":
				    case "settings_port2wr":
				    case "settings_port2load":
					    changedKeysCleaned.add("settings_port2");
					    break;
				    default:
					    changedKeysCleaned.add(key);
					    break;
			    }
		    }
		    for (String key : changedKeysCleaned) {
			    updateFromPreferences(key, true);
		    }
		    settingsFragment.unregisterOnSettingsKeyChangedListener();
	    });
	    settingsFragment.show(getSupportFragmentManager(), "SettingsFragment");
    }

    private void openDocument() {
        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        intent.setType("*/*");
        intent.putExtra(Intent.EXTRA_TITLE, getString(R.string.filename) + "-object.hp");
	    saveWhenLaunchingActivity = false;
        startActivityForResult(intent, INTENT_OBJECT_LOAD);
    }

    private void OnObjectLoad() {
        if(settings.getBoolean("settings_objectloadwarning", false)) {
            DialogInterface.OnClickListener onClickListener = (dialog, which) -> {
                if (which == DialogInterface.BUTTON_POSITIVE) {
                    openDocument();
                }
            };
            new AlertDialog.Builder(this)
                    .setMessage(getString(R.string.message_object_load))
                    .setPositiveButton(android.R.string.yes, onClickListener)
                    .setNegativeButton(android.R.string.no, onClickListener)
                    .show();
        } else
            openDocument();
    }
    private void SaveObject() {
        Intent intent = new Intent(Intent.ACTION_CREATE_DOCUMENT);
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        intent.setType("*/*");
        intent.putExtra(Intent.EXTRA_TITLE, getString(R.string.filename) + "-object.hp");
	    saveWhenLaunchingActivity = false;
        startActivityForResult(intent, INTENT_OBJECT_SAVE);
    }
    private void OnObjectSave() {
        int model = NativeLib.getCurrentModel();
        if(model == 'L'     // HP32S    # Leonardo
		|| model == 'N'     // HP32SII  # Nardo
        || model == 'D') {  // HP42S    # Davinci
            final String[] objectsToSave = NativeLib.getObjectsToSave();
            objectsToSaveItemChecked = new boolean[objectsToSave.length];
            new AlertDialog.Builder(MainActivity.this)
                    .setTitle(getResources().getString(R.string.message_object_save_program))
                    .setMultiChoiceItems(objectsToSave, null, (dialog, which, isChecked) -> objectsToSaveItemChecked[which] = isChecked).setPositiveButton("OK", (dialog, id) -> {
                        //NativeLib.onObjectSave(url);
                        SaveObject();
                    }).setNegativeButton("Cancel", (dialog, id) -> objectsToSaveItemChecked = null).show();
        } else
            SaveObject(); // HP28S
    }
    
    private void OnViewCopyFullscreen() {
        Bitmap bitmapScreen = mainScreenView.getBitmap();
        if(bitmapScreen == null)
            return;
        String imageFilename = getString(R.string.filename) + "-" + new SimpleDateFormat("yyyy-MM-dd_HH-mm-ss", Locale.US).format(new Date());
        try {
            File storagePath = new File(this.getExternalCacheDir(), "");
            File imageFile = File.createTempFile(imageFilename, ".png", storagePath);
            FileOutputStream fileOutputStream = new FileOutputStream(imageFile);
            bitmapScreen.compress(Bitmap.CompressFormat.PNG, 90, fileOutputStream);
            fileOutputStream.close();

	        String subject = getString(R.string.message_screenshot);
	        Intent intent = new Intent(android.content.Intent.ACTION_SEND);
	        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
	        intent.putExtra(Intent.EXTRA_SUBJECT, subject);
	        intent.putExtra(Intent.EXTRA_TITLE, subject);
	        intent.setFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
	        intent.putExtra(Intent.EXTRA_STREAM, FileProvider.getUriForFile(this,this.getPackageName() + ".provider", imageFile));
	        intent.setType("image/png");
//	        intent.setDataAndType(FileProvider.getUriForFile(this,this.getPackageName() + ".provider", imageFile), "image/png");
	        startActivity(Intent.createChooser(intent, getString(R.string.message_share_screenshot)));
        } catch (Exception e) {
            e.printStackTrace();
            showAlert(e.getMessage());
        }
    }
    private void OnViewCopy() {
        int width = NativeLib.getScreenWidth();
        int height = NativeLib.getScreenHeight();
        Bitmap bitmapScreen = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888);
        bitmapScreen.eraseColor(Color.BLACK);

        NativeLib.onViewCopy(bitmapScreen);

        String imageFilename = getString(R.string.filename) + "-" + new SimpleDateFormat("yyyy-MM-dd_HH-mm-ss", Locale.US).format(new Date());
        try {
            File storagePath = new File(this.getExternalCacheDir(), "");
            File imageFile = File.createTempFile(imageFilename, ".png", storagePath);
            FileOutputStream fileOutputStream = new FileOutputStream(imageFile);
            bitmapScreen.compress(Bitmap.CompressFormat.PNG, 90, fileOutputStream);
            fileOutputStream.close();

	        String subject = getString(R.string.message_screenshot);
	        Intent intent = new Intent(android.content.Intent.ACTION_SEND);
	        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
	        intent.putExtra(Intent.EXTRA_SUBJECT, subject);
	        intent.putExtra(Intent.EXTRA_TITLE, subject);
	        intent.setFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
	        intent.putExtra(Intent.EXTRA_STREAM, FileProvider.getUriForFile(this,this.getPackageName() + ".provider", imageFile));
	        intent.setType("image/png");
//	        intent.setDataAndType(FileProvider.getUriForFile(this,this.getPackageName() + ".provider", imageFile), "image/png");
	        startActivity(Intent.createChooser(intent, getString(R.string.message_share_screenshot)));
        } catch (Exception e) {
            e.printStackTrace();
            showAlert(e.getMessage());
        }
    }
    private void OnStackCopy() {
        NativeLib.onStackCopy();
    }
    private void OnStackPaste() {
        NativeLib.onStackPaste();
    }
    private void OnViewReset() {
        DialogInterface.OnClickListener onClickListener = (dialog, which) -> {
            if (which == DialogInterface.BUTTON_POSITIVE) {
                NativeLib.onViewReset();
            }
        };
        new AlertDialog.Builder(this)
                .setMessage(getString(R.string.message_press_reset))
                .setPositiveButton(getString(R.string.message_yes), onClickListener)
                .setNegativeButton(getString(R.string.message_no), onClickListener)
                .show();
    }
    private void OnBackupSave() {
        NativeLib.onBackupSave();
        updateNavigationDrawerItems();
    }
    private void OnBackupRestore() {
        NativeLib.onBackupRestore();
        updateNavigationDrawerItems();
    }
    private void OnBackupDelete() {
        NativeLib.onBackupDelete();
        updateNavigationDrawerItems();
    }
    private void OnViewScript() {
        if (NativeLib.getState() != 0 /*SM_RUN*/) {
            showAlert(getString(R.string.message_change_kml));
            return;
        }

        showKMLPicker(true);
    }

    private void OnViewPrinter() {
        fragmentPrinterSimulator.show(getSupportFragmentManager(), "PrinterSimulatorFragment");
    }

    private void showKMLPicker(boolean changeKML) {
        extractKMLScripts();

        ArrayList<KMLScriptItem> kmlScriptsForCurrentModel;
        if(changeKML) {
            kmlScriptsForCurrentModel = new ArrayList<>();
            char m = (char) NativeLib.getCurrentModel();
            for (int i = 0; i < kmlScripts.size(); i++) {
                KMLScriptItem kmlScriptItem = kmlScripts.get(i);
                if (kmlScriptItem.model.charAt(0) == m)
                    kmlScriptsForCurrentModel.add(kmlScriptItem);
            }
        } else
            kmlScriptsForCurrentModel = kmlScripts;

        boolean showDefaultKMLScriptFolderItem = !kmlFolderUseDefault && (getPackageName().contains("org.emulator.forty.eight") || getPackageName().contains("org.emulator.forty.two"));
        int lastIndex = kmlScriptsForCurrentModel.size();
        String[] kmlScriptTitles = new String[lastIndex + (showDefaultKMLScriptFolderItem ? 2 : 1)];
        for (int i = 0; i < kmlScriptsForCurrentModel.size(); i++)
            kmlScriptTitles[i] = kmlScriptsForCurrentModel.get(i).title;
        kmlScriptTitles[lastIndex] = getResources().getString(R.string.load_custom_kml);
        if(showDefaultKMLScriptFolderItem)
            kmlScriptTitles[lastIndex + 1] = getResources().getString(R.string.load_default_kml);
        new AlertDialog.Builder(MainActivity.this)
                .setTitle(getResources().getString(kmlFolderUseDefault ? R.string.pick_default_calculator : R.string.pick_custom_calculator))
                .setItems(kmlScriptTitles, (dialog, which) -> {
                    if(which == lastIndex) {
                    	// [Select a Custom KML script folder...]
                        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.LOLLIPOP) { // < API 21
                            new AlertDialog.Builder(MainActivity.this)
                                    .setTitle(getString(R.string.message_kml_folder_selection_need_api_lollipop))
                                    .setMessage(getString(R.string.message_kml_folder_selection_need_api_lollipop_description))
                                    .setPositiveButton(android.R.string.ok, (dialog1, which1) -> {
                                    }).show();
                        } else {
                            Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE);
	                        saveWhenLaunchingActivity = false;
                            startActivityForResult(intent, changeKML ? INTENT_PICK_KML_FOLDER_FOR_CHANGING : INTENT_PICK_KML_FOLDER_FOR_NEW_FILE);
                        }
                    } else if(which == lastIndex + 1) {
                        // [Default KML script folder]
                        changeKMLFolder(null);
                        if(changeKML)
                            OnViewScript();
                        else
                            OnFileNew();
                    } else {
                    	// We choose a calculator name from the list.
	                    KMLScriptItem scriptItem = kmlScriptsForCurrentModel.get(which);
                        if(changeKML) {
                        	// We only change the KML script here.
                            int result = NativeLib.onViewScript(scriptItem.filename, scriptItem.folder);
                            if(result > 0) {
	                            settings.putString("settings_kml_folder_embedded", scriptItem.folder);
                                displayKMLTitle();
                                showKMLLog();
                            } else
                                showKMLLogForce();
                            updateNavigationDrawerItems();
                        } else
	                        // We create a new calculator with a specific KML script.
                            newFileFromKML(scriptItem.filename, scriptItem.folder);
                    }
                }).show();
    }

    private void OnCreateRAMCard() {
        String[] stringArrayRAMCards = getResources().getStringArray(R.array.ram_cards);
        new AlertDialog.Builder(MainActivity.this)
                .setTitle(getResources().getString(R.string.create_ram_card_title))
                .setItems(stringArrayRAMCards, (dialog, which) -> {
                    Intent intent = new Intent(Intent.ACTION_CREATE_DOCUMENT);
                    intent.addCategory(Intent.CATEGORY_OPENABLE);
                    intent.setType("*/*");
                    String sizeTitle = "2mb";
                    selectedRAMSize = -1;
                    switch (which) {
                        case 0: // 32kb (1 port: 2)
                            sizeTitle = "32kb";
                            selectedRAMSize = 32;
                            break;
                        case 1: // 128kb (1 port: 2)
                            sizeTitle = "128kb";
                            selectedRAMSize = 128;
                            break;
                        case 2: // 256kb (2 ports: 2,3)
                            sizeTitle = "256kb";
                            selectedRAMSize = 256;
                            break;
                        case 3: // 512kb (4 ports: 2 through 5)
                            sizeTitle = "512kb";
                            selectedRAMSize = 512;
                            break;
                        case 4: // 1mb (8 ports: 2 through 9)
                            sizeTitle = "1mb";
                            selectedRAMSize = 1024;
                            break;
                        case 5: // 2mb (16 ports: 2 through 17)
                            sizeTitle = "2mb";
                            selectedRAMSize = 2048;
                            break;
                        case 6: // 4mb (32 ports: 2 through 33)
                            sizeTitle = "4mb";
                            selectedRAMSize = 4096;
                            break;
                    }
                    intent.putExtra(Intent.EXTRA_TITLE, "shared-" + sizeTitle + ".bin");
	                saveWhenLaunchingActivity = false;
                    startActivityForResult(intent, INTENT_CREATE_RAM_CARD);
                }).show();
    }

    private void OnMacroRecord() {
        Intent intent = new Intent(Intent.ACTION_CREATE_DOCUMENT);
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        intent.setType("*/*");
        intent.putExtra(Intent.EXTRA_TITLE, getString(R.string.filename) + "-macro.mac");
	    saveWhenLaunchingActivity = false;
        startActivityForResult(intent, INTENT_MACRO_SAVE);
    }

    private void OnMacroPlay() {
        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        intent.setType("*/*");
        intent.putExtra(Intent.EXTRA_TITLE, getString(R.string.filename) + "-macro.mac");
	    saveWhenLaunchingActivity = false;
        startActivityForResult(intent, INTENT_MACRO_LOAD);
    }

    private void OnMacroStop() {
        runOnUiThread(() -> {
            NativeLib.onToolMacroStop();
            updateNavigationDrawerItems();
        });
    }

    private void OnTopics() {
	    new InfoWebFragment().show(getSupportFragmentManager(), "InfoWebFragment");
    }
    private void OnAbout() {
	    new InfoFragment().show(getSupportFragmentManager(), "InfoFragment");
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, @Nullable Intent data) {
        if(resultCode == Activity.RESULT_OK && data != null) {
            Uri uri = data.getData();
            String url = null;
            if (uri != null)
                url = uri.toString();
            if (url != null) {
                switch (requestCode) {
                    case INTENT_GETOPENFILENAME: {
                    	if(debug) Log.d(TAG, "onActivityResult INTENT_GETOPENFILENAME " + url);
	                    // FileOpen from the "Open..." menu.
                        onFileOpen(url, data, null);
                        break;
                    }
                    case INTENT_GETSAVEFILENAME: {
                    	if(debug) Log.d(TAG, "onActivityResult INTENT_GETSAVEFILENAME " + url);
                        if (NativeLib.onFileSaveAs(url) != 0) {
                        	settings.saveInStateFile(this, url);

                            displayFilename(url);

							showAlert(String.format(Locale.US, getString(R.string.message_state_saved), getFilenameFromURL(url)));
	                        saveLastDocument(url);
	                        Utils.makeUriPersistable(this, data, uri);
                            if (fileSaveAsCallback != null)
                                fileSaveAsCallback.run();
                        }
                        break;
                    }
                    case INTENT_OBJECT_LOAD: {
                    	if(debug) Log.d(TAG, "onActivityResult INTENT_OBJECT_LOAD " + url);
                        NativeLib.onObjectLoad(url);
                        break;
                    }
                    case INTENT_OBJECT_SAVE: {
                    	if(debug) Log.d(TAG, "onActivityResult INTENT_OBJECT_SAVE " + url);
                        NativeLib.onObjectSave(url, objectsToSaveItemChecked);
                        objectsToSaveItemChecked = null;
                        break;
                    }
                    case INTENT_PICK_KML_FOLDER_FOR_NEW_FILE:
                    case INTENT_PICK_KML_FOLDER_FOR_CHANGING:
                    case INTENT_PICK_KML_FOLDER_FOR_SECURITY:
	                case INTENT_PICK_KML_FOLDER_FOR_KML_NOT_FOUND:
                    {
                    	if(debug) Log.d(TAG, "onActivityResult INTENT_PICK_KML_FOLDER_X " + url);
	                    changeKMLFolder(url);
                        Utils.makeUriPersistableReadOnly(this, data, uri);

                        switch (requestCode) {
                            case INTENT_PICK_KML_FOLDER_FOR_NEW_FILE:
                                OnFileNew();
                                break;
                            case INTENT_PICK_KML_FOLDER_FOR_CHANGING:
                                OnViewScript();
                                break;
	                        case INTENT_PICK_KML_FOLDER_FOR_SECURITY:
	                        case INTENT_PICK_KML_FOLDER_FOR_KML_NOT_FOUND:
	                        	if(intentPickKmlFolderForUrlToOpen != null)
			                        onFileOpen(intentPickKmlFolderForUrlToOpen, null, url);
		                        else
			                        new AlertDialog.Builder(this)
					                        .setTitle(getString(R.string.message_open_security_retry))
					                        .setMessage(getString(R.string.message_open_security_retry_description))
					                        .setPositiveButton(android.R.string.ok, (dialog, which) -> {
					                        }).show();
		                        break;
                        }
                        break;
                    }
                    case INTENT_CREATE_RAM_CARD: {
                    	if(debug) Log.d(TAG, "onActivityResult INTENT_CREATE_RAM_CARD " + url);
                        if(selectedRAMSize > 0) {
                            int size = 2 * selectedRAMSize;
                            FileOutputStream fileOutputStream;
                            try {
                                ParcelFileDescriptor pfd = getContentResolver().openFileDescriptor(uri, "w");
                                if(pfd != null) {
                                    fileOutputStream = new FileOutputStream(pfd.getFileDescriptor());
                                    byte[] zero = new byte[1024];
                                    Arrays.fill(zero, (byte) 0);
                                    for (int i = 0; i < size; i++)
                                        fileOutputStream.write(zero);
                                    fileOutputStream.flush();
                                    fileOutputStream.close();
                                }
                            } catch (IOException e) {
                                e.printStackTrace();
                            }
                            selectedRAMSize = -1;
                        }

                        break;
                    }
                    case INTENT_MACRO_LOAD: {
                    	if(debug) Log.d(TAG, "onActivityResult INTENT_MACRO_LOAD " + url);
                        NativeLib.onToolMacroPlay(url);
                        updateNavigationDrawerItems();
                        break;
                    }
                    case INTENT_MACRO_SAVE: {
                    	if(debug) Log.d(TAG, "onActivityResult INTENT_MACRO_SAVE " + url);
                        NativeLib.onToolMacroNew(url);
                        updateNavigationDrawerItems();
                        break;
                    }
                    default:
                        break;
                }
            }
        }
	    saveWhenLaunchingActivity = true;
        fileSaveAsCallback = null;
	    super.onActivityResult(requestCode, resultCode, data);
    }

    private void saveLastDocument(String url) {
	    settings.putString("lastDocument", url);

        if(url != null && !url.isEmpty())
            mruLinkedHashMap.put(url, null);
        navigationView.post(this::updateMRU);
    }

    private void showCalculatorView(boolean show) {
        if(show) {
            mainScreenView.setEnablePanAndScale(true);
            mainScreenView.setVisibility(View.VISIBLE);
            int background = mainScreenView.getBackgroundColor();
	        double luminance = (0.2126 * (background >> 16 & 0xFF) + 0.7152 * (background >> 8 & 0xFF) + 0.0722 * (background & 0xFF)) / 256.0;
	        imageButtonMenu.setColorFilter(luminance < 0.5 ? Color.argb(255, 255, 255, 255) : Color.argb(255, 0, 0, 0));
        } else {
            mainScreenView.setEnablePanAndScale(false);
            mainScreenView.setVisibility(View.GONE);
	        imageButtonMenu.setColorFilter(Utils.getThemedColor(this, android.R.attr.colorForeground));
        }
    }

	private void migrateDeprecatedSettings() {
		if(!settings.hasValue("settings_haptic_feedback_duration") && settings.hasValue("settings_haptic_feedback")) {
			settings.removeValue("settings_haptic_feedback");
			settings.putInt("settings_haptic_feedback_duration", 25);
		}
	}

    private void onFileOpen(String url, Intent intent, String openWithKMLScriptFolder) {

    	// Eventually, close the previous state file
	    NativeLib.onFileClose();
	    showCalculatorView(false);
	    displayFilename("");
	    intentPickKmlFolderForUrlToOpen = null;

	    // Pre-Load the embedded settings from the end of the classic state file
	    settings.setIsDefaultSettings(false);
	    settings.clearEmbeddedStateSettings();
	    settings.loadFromStateFile(this, url);

	    migrateDeprecatedSettings();

	    if(intent != null)
		    // Make this file persistable to allow a next access to this same file.
		    Utils.makeUriPersistable(this, intent, Uri.parse(url));

	    String kmlScriptFolder;
	    String embeddedKMLScriptFolder = settings.getString("settings_kml_folder_embedded", null);

	    if(openWithKMLScriptFolder != null)
		    kmlScriptFolder = openWithKMLScriptFolder;
	    else
		    kmlScriptFolder = embeddedKMLScriptFolder;

	    if(kmlScriptFolder != null && !kmlScriptFolder.startsWith("assets/")) {
		    // Change the default KMLFolder to allow getFirstKMLFilenameForType() to find in the specific folder!
		    Uri kmlFolderUri = Uri.parse(kmlScriptFolder);
		    DocumentFile kmlFolderDocumentFile = DocumentFile.fromTreeUri(this, kmlFolderUri);
		    if(kmlFolderDocumentFile != null) {
			    // Check the permission of the folder kmlScriptFolder
			    if (!kmlFolderDocumentFile.canRead()) {
				    // Permission denied, switch to the default asset folder
				    changeKMLFolder(null);
				    kmlScriptFolder = null;
				    showAlert(getString(R.string.message_open_kml_folder_permission_lost), true);
			    } else if (kmlFolderDocumentFile.exists())
				    changeKMLFolder(kmlScriptFolder);
		    } else {
		    	// We are using the API < 21, so we
			    changeKMLFolder(null);
			    kmlScriptFolder = null;
		    }
	    }

	    onFileOpenNative(url, kmlScriptFolder);
    }

	private void onFileOpenNative(String url, String kmlScriptFolder) {
		// Update the Emu VM with the new settings.
		updateFromPreferences(null, false);

		// Load the genuine state file
		int result = NativeLib.onFileOpen(url, kmlScriptFolder);
		if(result > 0) {
			// Copy the port1 settings from the state file to the settings (for the UI).
	        setPort1Settings(NativeLib.getPort1Plugged(), NativeLib.getPort1Writable());
			// State file successfully opened.
			String currentKml = NativeLib.getCurrentKml();
		    if(kmlScriptFolder == null || currentKml.startsWith("document:")) {
			    // Needed for compatibility:
			    // The KML folder is not in the JSON settings embedded in the state file,
			    // so, we need to extract it and change the variable szCurrentKml.
			    Pattern patternKMLDocumentURL = Pattern.compile("document:([^|]+)\\|(.+)");
			    Matcher m = patternKMLDocumentURL.matcher(currentKml);
			    if (m.find() && m.groupCount() == 2) {
				    String kmlScriptFolderInDocument = m.group(1);
				    String kmlScriptFilenameInDocument = m.group(2);
				    kmlScriptFolder = kmlScriptFolderInDocument;
				    NativeLib.setCurrentKml(kmlScriptFilenameInDocument);
			    } else {
				    // Should not happen... But in case.
				    kmlScriptFolder = NativeLib.getEmuDirectory();
			    }
		    }
			settings.putString("settings_kml_folder_embedded", kmlScriptFolder);
			changeKMLFolder(kmlScriptFolder);
	        showCalculatorView(true);
	        displayFilename(url);
			saveLastDocument(url);
	        showKMLLog();
	    } else {
		    // Because the state file failed to load, we switch to the default settings.
		    settings.setIsDefaultSettings(true);
		    settings.clearEmbeddedStateSettings();
		    if(Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP // >= API 21
		    && (result < 0 || kmlScriptFolder == null)) {
			    // If the KML file takes an access denied (-2) or the KML file has not been found (-3) or kmlScriptFolder is null,
			    // we must prompt the use to choose one (after onFileOpen to know the model)!
			    if(result == -2)
				    // For security reason, you must select the folder where are the KML and ROM files and then, reopen this file!
				    new AlertDialog.Builder(this)
						    .setTitle(getString(R.string.message_open_security))
						    .setMessage(getString(R.string.message_open_security_description))
						    .setPositiveButton(android.R.string.ok, (dialog, which) -> {
							    intentPickKmlFolderForUrlToOpen = url;
							    saveWhenLaunchingActivity = false;
							    startActivityForResult(new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE), INTENT_PICK_KML_FOLDER_FOR_SECURITY);
						    }).setNegativeButton(android.R.string.cancel, (dialog, which) -> removeMRU(url)).show();
			    else if(result == -3 || kmlScriptFolder == null) {
				    // KML file (or a compatible KML file) has not been found, you must select the folder where are the KML and ROM files and then, reopen this file!
				    String currentKml = NativeLib.getCurrentKml();
						String description;
						if(currentKml != null) {
							if(kmlScriptFolder != null)
								description = String.format(Locale.US, getString(R.string.message_open_kml_not_found_description3), currentKml, kmlScriptFolder);
							else
								description = String.format(Locale.US, getString(R.string.message_open_kml_not_found_description2), currentKml);
						} else
							description = getString(R.string.message_open_kml_not_found_description);
				    new AlertDialog.Builder(this)
						    .setTitle(getString(R.string.message_open_kml_not_found))
						    .setMessage(description)
						    .setPositiveButton(android.R.string.ok, (dialog, which) -> {
							    intentPickKmlFolderForUrlToOpen = url;
							    saveWhenLaunchingActivity = false;
							    startActivityForResult(new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE), INTENT_PICK_KML_FOLDER_FOR_KML_NOT_FOUND);
						    }).setNegativeButton(android.R.string.cancel, (dialog, which) -> removeMRU(url)).show();
			    } else
				    removeMRU(url);
		    } else {
		    	// Show the KML error
			    showKMLLogForce();
			    removeMRU(url);
		    }
	    }
		updateNavigationDrawerItems();
	}

	private void onFileSave() {
		if (NativeLib.onFileSave() == 1) {
			String currentFilenameUrl = NativeLib.getCurrentFilename();
			settings.saveInStateFile(this, currentFilenameUrl);
			String currentFilename = getFilenameFromURL(currentFilenameUrl);
			showAlert(String.format(Locale.US, getString(R.string.message_state_saved), currentFilename));
		}
	}

    private void displayFilename(String stateFileURL) {
        String displayName = getFilenameFromURL(stateFileURL == null ? "" : stateFileURL);

        View headerView = displayKMLTitle();
        if(headerView != null) {
            TextView textViewSubtitle = headerView.findViewById(R.id.nav_header_subtitle);
            if (textViewSubtitle != null)
                textViewSubtitle.setText(displayName);
        }
    }

    private String getFilenameFromURL(String url) {
        String displayName = "";
        try {
            displayName = Utils.getFileName(this, url);
        } catch(Exception e) {
            // Do nothing
        }
        return displayName;
    }

    private View displayKMLTitle() {
        View headerView = null;
        NavigationView navigationView = findViewById(R.id.nav_view);
        if (navigationView != null) {
            headerView = navigationView.getHeaderView(0);
            if (headerView != null) {
                TextView textViewTitle = headerView.findViewById(R.id.nav_header_title);
                if (textViewTitle != null)
                    textViewTitle.setText(NativeLib.getKMLTitle());
                changeHeaderIcon();
            }
        }
        return headerView;
    }

    private void changeHeaderIcon() {
        NavigationView navigationView = findViewById(R.id.nav_view);
        View headerView = navigationView.getHeaderView(0);
        if (headerView != null) {
            ImageView imageViewIcon = headerView.findViewById(R.id.nav_header_icon);
            if (imageViewIcon != null) {
                if (bitmapIcon != null)
                    imageViewIcon.setImageBitmap(bitmapIcon);
                else
                    imageViewIcon.setImageDrawable(null);
            }
        }
    }

    private void showKMLLog() {
        if(settings.getBoolean("settings_alwaysdisplog", false)) {
            showKMLLogForce();
        }
    }

    private void showKMLLogForce() {
        String kmlLog = NativeLib.getKMLLog();
        new AlertDialog.Builder(this)
                .setTitle(getString(R.string.message_kml_script_compilation_result))
                .setMessage(kmlLog)
                .setPositiveButton(android.R.string.ok, (dialog, which) -> {
                }).show();
    }

    // Method used from JNI!

	@SuppressWarnings("UnusedDeclaration")
    public int updateCallback(int type, int param1, int param2, String param3, String param4) {

        mainScreenView.updateCallback(type, param1, param2, param3, param4);
        return -1;
    }


    final int GENERIC_READ   = 1;
    final int GENERIC_WRITE  = 2;
    SparseArray<ParcelFileDescriptor> parcelFileDescriptorPerFd = null;
    public int openFileFromContentResolver(String fileURL, int writeAccess) {
        //https://stackoverflow.com/a/31677287
        Uri uri = Uri.parse(fileURL);
        ParcelFileDescriptor filePfd;
        try {
            String mode = "";
            if((writeAccess & GENERIC_READ) == GENERIC_READ)
                mode += "r";
            if((writeAccess & GENERIC_WRITE) == GENERIC_WRITE)
                mode += "w";
            filePfd = getContentResolver().openFileDescriptor(uri, mode);
        } catch (SecurityException e) {
            e.printStackTrace();
            return -2;
        } catch (Exception e) {
            e.printStackTrace();
            return -1;
        }
        int fd = filePfd != null ? filePfd.getFd() : 0;
        if(parcelFileDescriptorPerFd == null) {
            parcelFileDescriptorPerFd = new SparseArray<>();
        }
        parcelFileDescriptorPerFd.put(fd, filePfd);
        return fd;
    }


    String folderURLCached = null;
    HashMap<String, String> folderCache = new HashMap<>();

    public void clearFolderCache() {
        folderURLCached = null;
        folderCache.clear();
    }

    @SuppressWarnings("UnusedDeclaration")
    public int openFileInFolderFromContentResolver(String filename, String folderURL, int writeAccess) {
    	if(filename != null) {
    		if(filename.startsWith("content://"))
			    return openFileFromContentResolver(filename, writeAccess);
		    if (folderURLCached == null || !folderURLCached.equals(folderURL)) {
			    folderURLCached = folderURL;
			    folderCache.clear();
			    Uri folderURI = Uri.parse(folderURL);
			    DocumentFile folderDocumentFile = DocumentFile.fromTreeUri(this, folderURI);
			    if (folderDocumentFile != null)
				    for (DocumentFile file : folderDocumentFile.listFiles())
					    folderCache.put(file.getName(), file.getUri().toString());
		    }
		    String filenameUrl = folderCache.get(filename);
		    if (filenameUrl != null)
			    return openFileFromContentResolver(filenameUrl, writeAccess);
	    }
        return -1;
    }

    @SuppressWarnings("UnusedDeclaration")
    public int closeFileFromContentResolver(int fd) {
        if(parcelFileDescriptorPerFd != null) {
            ParcelFileDescriptor filePfd = parcelFileDescriptorPerFd.get(fd);
            if(filePfd != null) {
                try {
                    filePfd.close();
                    parcelFileDescriptorPerFd.remove(fd);
                    return 0;
                } catch (IOException e) {
                    e.printStackTrace();
                }
            }
        }
        return -1;
    }

	public void showAlert(String text) {
		showAlert(text, false);
	}

	public void showAlert(String text, boolean lengthLong) {
        Utils.showAlert(this, text, lengthLong);
    }

    @SuppressWarnings("UnusedDeclaration")
    public void sendMenuItemCommand(int menuItem) {
        switch (menuItem) {
            case 1: // FILE_NEW
                OnFileNew();
                break;
            case 2: // FILE_OPEN
                OnFileOpen();
                break;
            case 3: // FILE_SAVE
                OnFileSave();
                break;
            case 4: // FILE_SAVEAS
                OnFileSaveAs();
                break;
            case 5: // FILE_EXIT
                break;
            case 6: // EDIT_COPY_SCREEN
                OnViewCopy();
                break;
            case 7: // FILE_SETTINGS
                OnSettings();
                break;
            case 8: // EDIT_RESET
                OnViewReset();
                break;
            case 9: // EDIT_LOAD_OBJECT
                OnObjectLoad();
                break;
            case 10: // EDIT_SAVE_OBJECT
                OnObjectSave();
                break;
            case 11: // HELP_ABOUT
                OnAbout();
                break;
            case 12: // HELP_TOPICS
                OnTopics();
                break;
            case 13: // FILE_CLOSE
                OnFileClose();
                break;
            case 14: // EDIT_BACKUP_SAVE
                OnBackupSave();
                break;
            case 15: // EDIT_BACKUP_RESTORE
                OnBackupRestore();
                break;
            case 16: // EDIT_BACKUP_DELETE
                OnBackupDelete();
                break;
            case 17: // VIEW_SCRIPT
                OnViewScript();
                break;
            case 18: // EDIT_PORT_CONFIGURATION
                break;
            case 19: // EDIT_COPY_STRING
                OnStackCopy();
                break;
            case 20: // EDIT_PASTE_STRING
                OnStackPaste();
                break;
            case 21: // TOOL_DISASM
                break;
            case 22: // TOOL_DEBUG
                break;
            case 23: // TOOL_MACRO_RECORD
                break;
            case 24: // TOOL_MACRO_PLAY
                break;
            case 25: // TOOL_MACRO_STOP
                OnMacroStop();
                break;
            case 26: // TOOL_MACRO_SETTINGS
                break;
            default:
                break;
        }
    }

    @SuppressWarnings("UnusedDeclaration")
    public int getFirstKMLFilenameForType(char chipsetType) {
    	if(!kmlFolderUseDefault) {
		    extractKMLScripts();

		    for (int i = 0; i < kmlScripts.size(); i++) {
			    KMLScriptItem kmlScriptItem = kmlScripts.get(i);
			    if (kmlScriptItem.model.charAt(0) == chipsetType) {
				    showAlert(String.format(Locale.US, getString(R.string.message_open_kml_not_found_alert_trying_other1), kmlScriptItem.filename), true);
				    NativeLib.setCurrentKml(kmlScriptItem.filename);
				    NativeLib.setEmuDirectory(kmlFolderURL);
				    return 1;
			    }
		    }

		    // Not found, so we search in the default KML asset folder
		    kmlFolderUseDefault = true;
		    kmlFolderChange = true;
	    }

        extractKMLScripts();

        for (int i = 0; i < kmlScripts.size(); i++) {
            KMLScriptItem kmlScriptItem = kmlScripts.get(i);
            if (kmlScriptItem.model.charAt(0) == chipsetType) {
	            showAlert(String.format(Locale.US, getString(R.string.message_open_kml_not_found_alert_trying_other2), kmlScriptItem.filename), true);
	            NativeLib.setCurrentKml(kmlScriptItem.filename);
	            NativeLib.setEmuDirectory("assets/calculators/");
                return 1;
            }
        }
	    showAlert(getString(R.string.message_open_kml_not_found_alert), true);
        return 0;
    }

    @SuppressWarnings("UnusedDeclaration")
    public void clipboardCopyText(String text) {
        // Gets a handle to the clipboard service.
        ClipboardManager clipboard = (ClipboardManager)getSystemService(Context.CLIPBOARD_SERVICE);
        if(clipboard != null) {
            ClipData clip = ClipData.newPlainText("simple text", text);
            // Set the clipboard's primary clip.
            clipboard.setPrimaryClip(clip);
        }
    }
    @SuppressWarnings("UnusedDeclaration")
    public String clipboardPasteText() {
        ClipboardManager clipboard = (ClipboardManager)getSystemService(Context.CLIPBOARD_SERVICE);
        if (clipboard != null && clipboard.hasPrimaryClip()) {
            ClipData clipData = clipboard.getPrimaryClip();
            if(clipData != null) {
                ClipData.Item item = clipData.getItemAt(0);
                // Gets the clipboard as text.
                CharSequence pasteData = item.getText();
                if (pasteData != null)
                    return pasteData.toString();
            }
        }
        return "";
    }

    @SuppressWarnings("UnusedDeclaration")
    public void performHapticFeedback() {
	    Utils.vibrate(vibrator, settings.getInt("settings_haptic_feedback_duration", 25));
    }

	@SuppressWarnings("UnusedDeclaration")
    public void sendByteUdp(int byteSent) {
        printerSimulator.write(byteSent);
    }

    @SuppressWarnings("UnusedDeclaration")
    public synchronized void setKMLIcon(int imageWidth, int imageHeight, byte[] pixels) {
        if(imageWidth > 0 && imageHeight > 0 && pixels != null) {
            try {
                bitmapIcon = Bitmap.createBitmap(imageWidth, imageHeight, Bitmap.Config.ARGB_8888);
                ByteBuffer buffer = ByteBuffer.wrap(pixels);
                bitmapIcon.copyPixelsFromBuffer(buffer);
            } catch (Exception ex) {
                // Cannot load the icon
                bitmapIcon = null;
            }
        } else if(bitmapIcon != null) {
            bitmapIcon = null;
        }
        if(bitmapIcon == null)
            // Try to load the app icon
            bitmapIcon = getApplicationIconBitmap();
        changeHeaderIcon();
    }

    private Bitmap getApplicationIconBitmap() {
        Drawable drawable = getApplicationInfo().loadIcon(getPackageManager());
        if (drawable instanceof BitmapDrawable) {
            BitmapDrawable bitmapDrawable = (BitmapDrawable) drawable;
            if (bitmapDrawable.getBitmap() != null)
                return bitmapDrawable.getBitmap();
        }

        if (drawable.getIntrinsicWidth() <= 0 || drawable.getIntrinsicHeight() <= 0)
            return Bitmap.createBitmap(1, 1, Bitmap.Config.ARGB_8888); // Single color bitmap will be created of 1x1 pixel
        else
            return Bitmap.createBitmap(drawable.getIntrinsicWidth(), drawable.getIntrinsicHeight(), Bitmap.Config.ARGB_8888);
    }

    private void setPort1Settings(boolean port1Plugged, boolean port1Writable) {
    	if(this.getClass().getPackage().getName().equals("org.emulator.forty.eight")) {
		    settings.putBoolean("settings_port1en", port1Plugged);
		    settings.putBoolean("settings_port1wr", port1Writable);
		    updateFromPreferences("settings_port1", true);
	    }
    }

    private void updateFromPreferences(String key, boolean isDynamic) {
        int isDynamicValue = isDynamic ? 1 : 0;
        if(key == null) {
            String[] settingKeys = {
                    "settings_realspeed", "settings_grayscale", "settings_rotation", "settings_auto_layout", "settings_allow_pinch_zoom", "settings_lcd_overlapping_mode", "settings_lcd_pixel_borders",
                    "settings_hide_bar", "settings_hide_button_menu", "settings_sound_volume", "settings_haptic_feedback",
                    "settings_background_kml_color", "settings_background_fallback_color",
					"settings_printer_model", "settings_macro",
                    "settings_kml", "settings_port1", "settings_port2",
                    "settings_flash_port2" };
			for (String settingKey : settingKeys)
                updateFromPreferences(settingKey, false);
        } else {
            switch (key) {
                case "settings_realspeed":
	            case "settings_grayscale":
					NativeLib.setConfiguration(key, isDynamicValue, settings.getBoolean(key, false) ? 1 : 0, 0, null);
                    break;

                case "settings_rotation":
                    int rotationMode = 0;
                    try {
						rotationMode = Integer.parseInt(settings.getString("settings_rotation", "0"));
                    } catch (NumberFormatException ex) {
                        // Catch bad number format
                    }
                    mainScreenView.setRotationMode(rotationMode, isDynamic);
                    break;
                case "settings_auto_layout":
					int autoLayoutMode = 1;
                    try {
						autoLayoutMode = Integer.parseInt(settings.getString("settings_auto_layout", "2"));
                    } catch (NumberFormatException ex) {
                        // Catch bad number format
                    }
                    mainScreenView.setAutoLayout(autoLayoutMode, isDynamic);
                    break;
                case "settings_allow_pinch_zoom":
					mainScreenView.setAllowPinchZoom(settings.getBoolean("settings_allow_pinch_zoom", true));
                    break;
                case "settings_lcd_overlapping_mode":
                    int overlappingLCDMode = LCDOverlappingView.OVERLAPPING_LCD_MODE_NONE;
                    try {
						overlappingLCDMode = Integer.parseInt(settings.getString("settings_lcd_overlapping_mode", "0"));
                    } catch (NumberFormatException ex) {
                        // Catch bad number format
                    }
                    lcdOverlappingView.setOverlappingLCDMode(overlappingLCDMode);
                    break;
	            case "settings_lcd_pixel_borders":
					boolean usePixelBorders = settings.getBoolean("settings_lcd_pixel_borders", false);
		            mainScreenView.setUsePixelBorders(usePixelBorders);
		            lcdOverlappingView.setUsePixelBorders(usePixelBorders);
		            break;
                case "settings_hide_bar":
                case "settings_hide_bar_status":
                case "settings_hide_bar_nav":
					if (settings.getBoolean("settings_hide_bar_status", false)
							|| settings.getBoolean("settings_hide_bar_nav", false))
                        hideSystemUI();
                    else
                        showSystemUI();
                    break;
                case "settings_hide_button_menu":
					imageButtonMenu.setVisibility(settings.getBoolean("settings_hide_button_menu", false) ? View.GONE : View.VISIBLE);
                    break;

                case "settings_sound_volume": {
					int volumeOption = settings.getInt("settings_sound_volume", 64);
                    NativeLib.setConfiguration("settings_sound_volume", isDynamicValue, volumeOption, 0, null);
                    break;
                }
                case "settings_haptic_feedback":
	            case "settings_haptic_feedback_duration":
                    // Nothing to do
                    break;

                case "settings_background_kml_color":
					mainScreenView.setBackgroundKmlColor(settings.getBoolean("settings_background_kml_color", false));
                    break;
                case "settings_background_fallback_color":
                    try {
						mainScreenView.setBackgroundFallbackColor(Integer.parseInt(settings.getString("settings_background_fallback_color", "0")));
                    } catch (NumberFormatException ex) {
                        // Catch bad number format
                    }
                    break;
                case "settings_printer_model":
                    try {
						printerSimulator.setPrinterModel82240A(Integer.parseInt(settings.getString("settings_printer_model", "1")) == 0);
                    } catch (NumberFormatException ex) {
                        // Catch bad number format
                    }
                    break;

                case "settings_kml":
                case "settings_kml_default":
                case "settings_kml_folder":
					loadKMLFolder();
                    break;

                case "settings_macro":
                case "settings_macro_real_speed":
                case "settings_macro_manual_speed":
					boolean macroRealSpeed = settings.getBoolean("settings_macro_real_speed", true);
					int macroManualSpeed = settings.getInt("settings_macro_manual_speed", 500);
                    NativeLib.setConfiguration("settings_macro", isDynamicValue, macroRealSpeed ? 1 : 0, macroManualSpeed, null);
                    break;

                case "settings_port1":
                case "settings_port1en":
                case "settings_port1wr":
                    NativeLib.setConfiguration("settings_port1", isDynamicValue,
							settings.getBoolean("settings_port1en", false) ? 1 : 0,
							settings.getBoolean("settings_port1wr", false) ? 1 : 0,
                            null);
                    break;
                case "settings_port2":
                case "settings_port2en":
                case "settings_port2wr":
                case "settings_port2load":
                    NativeLib.setConfiguration("settings_port2", isDynamicValue,
							settings.getBoolean("settings_port2en", false) ? 1 : 0,
							settings.getBoolean("settings_port2wr", false) ? 1 : 0,
							settings.getString("settings_port2load", ""));
                    break;
            }
        }
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus && (
            settings.getBoolean("settings_hide_bar_status", false)
            || settings.getBoolean("settings_hide_bar_nav", false)
        )) {
            hideSystemUI();
        }
    }

    private void hideSystemUI() {
        int flags = View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY;
        if(settings.getBoolean("settings_hide_bar_status", false))
            flags |= View.SYSTEM_UI_FLAG_FULLSCREEN;
        if(settings.getBoolean("settings_hide_bar_nav", false))
            flags |= View.SYSTEM_UI_FLAG_HIDE_NAVIGATION;

        getWindow().getDecorView().setSystemUiVisibility(flags);
    }

    private void showSystemUI() {
        getWindow().getDecorView().setSystemUiVisibility(0);
    }
}
