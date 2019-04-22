package org.emulator.forty.two;

import android.app.Activity;
import android.content.Intent;
import android.content.SharedPreferences;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.view.MenuItem;

import java.util.HashSet;

import androidx.annotation.Nullable;
import androidx.appcompat.app.ActionBar;
import androidx.appcompat.app.AppCompatActivity;
import androidx.preference.PreferenceFragmentCompat;
import androidx.preference.Preference;
import androidx.preference.PreferenceManager;

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
        public void onCreate(Bundle savedInstanceState) {
            super.onCreate(savedInstanceState);

            addPreferencesFromResource(R.xml.pref_general);
            setHasOptionsMenu(true);

            // Sound settings

            Preference preferenceAllowSound = findPreference("settings_allow_sound");
            if(preferenceAllowSound != null && !NativeLib.getSoundEnabled()) {
                preferenceAllowSound.setSummary("Cannot initialize the sound engine.");
                preferenceAllowSound.setEnabled(false);
            }

            // Background color settings

            Preference preferenceBackgroundFallbackColor = findPreference("settings_background_fallback_color");
//            final ColorPickerPreferenceCompat preferenceBackgroundCustomColor = (ColorPickerPreferenceCompat)findPreference("settings_background_custom_color");
            if(preferenceBackgroundFallbackColor != null /*&& preferenceBackgroundCustomColor != null*/) {
                final String[] stringArrayBackgroundFallbackColor = getResources().getStringArray(R.array.settings_background_fallback_color_item);
                Preference.OnPreferenceChangeListener onPreferenceChangeListenerBackgroundFallbackColor = new Preference.OnPreferenceChangeListener() {
                    @Override
                    public boolean onPreferenceChange(Preference preference, Object value) {
                        if(value != null) {
                            String stringValue = value.toString();
                            int backgroundFallbackColor = -1;
                            try {
                                backgroundFallbackColor = Integer.parseInt(stringValue);
                            } catch (NumberFormatException ex) {}
                            if(backgroundFallbackColor >= 0 && backgroundFallbackColor < stringArrayBackgroundFallbackColor.length)
                                preference.setSummary(stringArrayBackgroundFallbackColor[backgroundFallbackColor]);
//                            preferenceBackgroundCustomColor.setEnabled(backgroundFallbackColor == 2);
                        }
                        return true;
                    }
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

        }

        @Override
        public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {

        }
    }
}
