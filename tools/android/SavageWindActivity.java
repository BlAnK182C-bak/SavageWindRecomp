// Savage Wind's activity (tools/android/build.sh): SDL's, which loads libSDL3.so and libmain.so (the runtime) and calls
// its SDL_main. It asks once for "All files access", so that the data folder can be /sdcard/SavageWind, where a file
// manager or a PC over USB puts the game files (runtime/platform/linux.c android_data_root).
package savagewind.app;

import android.content.Intent;
import android.content.SharedPreferences;
import android.net.Uri;
import android.os.Bundle;
import android.os.Environment;
import android.provider.Settings;

import org.libsdl.app.SDLActivity;

public class SavageWindActivity extends SDLActivity {
    @Override protected String[] getLibraries() { return new String[] {"SDL3", "main"}; }

    @Override protected void onCreate(Bundle state) {
        super.onCreate(state);
        SharedPreferences p = getPreferences(MODE_PRIVATE);
        if (!Environment.isExternalStorageManager() && !p.getBoolean("asked_files", false)) {
            p.edit().putBoolean("asked_files", true).apply();
            startActivity(new Intent(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION,
                                     Uri.parse("package:" + getPackageName())));
        }
    }
}
