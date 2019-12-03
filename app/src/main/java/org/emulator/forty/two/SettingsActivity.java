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
import android.content.DialogInterface;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.res.Configuration;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.text.InputType;
import android.view.MenuItem;
import android.widget.EditText;
import android.widget.LinearLayout;

import java.util.HashSet;
import java.util.Locale;
import java.util.Objects;

import androidx.annotation.Nullable;
import androidx.appcompat.app.ActionBar;
import androidx.appcompat.app.AlertDialog;
import androidx.appcompat.app.AppCompatActivity;
import androidx.preference.Preference;
import androidx.preference.PreferenceFragmentCompat;
import androidx.preference.PreferenceManager;
import androidx.preference.SeekBarPreference;

import org.emulator.calculator.NativeLib;
import org.emulator.calculator.Utils;

public class SettingsActivity extends AppCompatActivity implements SharedPreferences.OnSharedPreferenceChangeListener {

    //private static final String TAG = "SettingsActivity";
    private static SharedPreferences sharedPreferences;
    private HashSet<String> settingsKeyChanged = new HashSet<>();

    private GeneralPreferenceFragment generalPreferenceFragment;


    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        sharedPreferences = PreferenceManager.getDefaultSharedPreferences(getApplicationContext());
        sharedPreferences.registerOnSharedPreferenceChangeListener(this);

        ActionBar actionBar = getSupportActionBar();
        if (actionBar != null) {
            // Show the Up button in the action bar.
            actionBar.setDisplayHomeAsUpEnabled(true);
        }

        generalPreferenceFragment = new GeneralPreferenceFragment();
        getSupportFragmentManager().beginTransaction().replace(android.R.id.content, generalPreferenceFragment).commit();
    }

    @Override
    protected void onDestroy() {
        sharedPreferences.unregisterOnSharedPreferenceChangeListener(this);
        super.onDestroy();
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        int id = item.getItemId();
        if (id == android.R.id.home) {
            onBackPressed();
            return true;
        }
        return super.onOptionsItemSelected(item);
    }

    @Override
    public void onBackPressed() {
        Intent resultIntent = new Intent();
        resultIntent.putExtra("changedKeys", settingsKeyChanged.toArray(new String[0]));
        setResult(Activity.RESULT_OK, resultIntent);
        finish();
        //super.onBackPressed();
    }

    @Override
    public void onSharedPreferenceChanged(SharedPreferences sharedPreferences, String key) {
        settingsKeyChanged.add(key);
    }

    /**
     * This fragment shows general preferences only. It is used when the
     * activity is showing a two-pane settings UI.
     */
    public static class GeneralPreferenceFragment extends PreferenceFragmentCompat {

        Preference preferencePort2load = null;

        @Override
        public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
            addPreferencesFromResource(R.xml.pref_general);
            setHasOptionsMenu(true);

            // Sound settings

//            Preference preferenceAllowSound = findPreference("settings_allow_sound");
//            if(preferenceAllowSound != null && !NativeLib.getSoundEnabled()) {
//                preferenceAllowSound.setSummary("Cannot initialize the sound engine.");
//                preferenceAllowSound.setEnabled(false);
//            }
            SeekBarPreference preferenceSoundVolume = findPreference("settings_sound_volume");
            if(preferenceSoundVolume != null) {
                if(!NativeLib.getSoundEnabled()) {
                    preferenceSoundVolume.setSummary("Cannot initialize the sound engine.");
                    preferenceSoundVolume.setEnabled(false);
                } else {
                    preferenceSoundVolume.setOnPreferenceClickListener(preference -> {
                        AlertDialog.Builder alert = new AlertDialog.Builder(Objects.requireNonNull(getContext()));
                        alert.setTitle(R.string.settings_sound_volume_dialog_title);
                        final EditText input = new EditText(getContext());
                        input.setInputType(InputType.TYPE_CLASS_NUMBER);
                        input.setRawInputType(Configuration.KEYBOARD_12KEY);
                        input.setFocusable(true);
                        input.setText(String.format(Locale.US,"%d", preferenceSoundVolume.getValue()));
//                        input.setMaxEms(3);
//                        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(LinearLayout.LayoutParams.WRAP_CONTENT, LinearLayout.LayoutParams.WRAP_CONTENT);
//                        input.setLayoutParams(lp);
                        alert.setView(input);
                        alert.setPositiveButton(R.string.message_ok, (dialog, whichButton) -> {
                            String newValueText = input.getText().toString();
                            try {
                                int newValue = Integer.parseInt(newValueText);
                                if(newValue >= preferenceSoundVolume.getMin() && newValue <= preferenceSoundVolume.getMax())
                                    preferenceSoundVolume.setValue(newValue);
                            } catch (NumberFormatException ignored) {}
                        });
                        alert.setNegativeButton(R.string.message_cancel, (dialog, whichButton) -> {});
                        alert.show();
                        return true;
                    });
                }
            }

            // Background color settings

            Preference preferenceBackgroundFallbackColor = findPreference("settings_background_fallback_color");
//            final ColorPickerPreferenceCompat preferenceBackgroundCustomColor = (ColorPickerPreferenceCompat)findPreference("settings_background_custom_color");
            if(preferenceBackgroundFallbackColor != null /*&& preferenceBackgroundCustomColor != null*/) {
                final String[] stringArrayBackgroundFallbackColor = getResources().getStringArray(R.array.settings_background_fallback_color_item);
                Preference.OnPreferenceChangeListener onPreferenceChangeListenerBackgroundFallbackColor = (preference, value) -> {
                    if(value != null) {
                        String stringValue = value.toString();
                        int backgroundFallbackColor = -1;
                        try {
                            backgroundFallbackColor = Integer.parseInt(stringValue);
                        } catch (NumberFormatException ignored) {}
                        if(backgroundFallbackColor >= 0 && backgroundFallbackColor < stringArrayBackgroundFallbackColor.length)
                            preference.setSummary(stringArrayBackgroundFallbackColor[backgroundFallbackColor]);
//                            preferenceBackgroundCustomColor.setEnabled(backgroundFallbackColor == 2);
                    }
                    return true;
                };
                preferenceBackgroundFallbackColor.setOnPreferenceChangeListener(onPreferenceChangeListenerBackgroundFallbackColor);
                onPreferenceChangeListenerBackgroundFallbackColor.onPreferenceChange(preferenceBackgroundFallbackColor,
                        sharedPreferences.getString(preferenceBackgroundFallbackColor.getKey(), "0"));


                //preferenceBackgroundCustomColor.setColorValue(customColor);

//                Preference.OnPreferenceChangeListener onPreferenceChangeListenerBackgroundCustomColor = new Preference.OnPreferenceChangeListener() {
//                    @Override
//                    public boolean onPreferenceChange(Preference preference, Object value) {
//                        if(value != null) {
//                            int customColor = (Integer)value;
//                        }
//                        return true;
//                    }
//                };
//                preferenceBackgroundCustomColor.setOnPreferenceChangeListener(onPreferenceChangeListenerBackgroundCustomColor);
//                onPreferenceChangeListenerBackgroundCustomColor.onPreferenceChange(preferenceBackgroundCustomColor, sharedPreferences.getBoolean(preferenceBackgroundCustomColor.getKey(), false));
            }

            // Macro

            Preference preferenceMacroRealSpeed = findPreference("settings_macro_real_speed");
            Preference preferenceMacroManualSpeed = findPreference("settings_macro_manual_speed");
            if(preferenceMacroRealSpeed != null && preferenceMacroManualSpeed != null) {
                Preference.OnPreferenceChangeListener onPreferenceChangeListenerMacroRealSpeed = (preference, value) -> {
                    if(value != null)
                        preferenceMacroManualSpeed.setEnabled(!(Boolean) value);
                    return true;
                };
                preferenceMacroRealSpeed.setOnPreferenceChangeListener(onPreferenceChangeListenerMacroRealSpeed);
                onPreferenceChangeListenerMacroRealSpeed.onPreferenceChange(preferenceMacroRealSpeed, sharedPreferences.getBoolean(preferenceMacroRealSpeed.getKey(), true));
            }

        }
    }
}
