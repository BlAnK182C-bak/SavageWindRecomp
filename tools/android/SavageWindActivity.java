// Savage Wind's activity (tools/android/build.sh): SDL's, which loads libSDL3.so and libmain.so (the runtime) and calls
// its SDL_main. It asks for "All files access" on each start until it has it, so that the data folder can be
// /sdcard/SavageWind, where a file manager or a PC over USB puts the game files (runtime/platform/linux.c
// android_data_root) - unless they're in the app's own folder, the one it uses without.
package savagewind.app;

import android.content.ActivityNotFoundException;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.os.Environment;
import android.provider.Settings;

import java.io.File;

import org.libsdl.app.SDLActivity;

public class SavageWindActivity extends SDLActivity {
    private boolean asked;  // (asked in this run: the runtime may have chosen its data folder without the permission)

    @Override protected String[] getLibraries() { return new String[] {"SDL3", "main"}; }

    @Override protected void onCreate(Bundle state) {
        super.onCreate(state);
        if (Environment.isExternalStorageManager() || new File(getExternalFilesDir(null), "extracted/exefs/main").exists())
            return;
        asked = true;
        try {
            startActivity(new Intent(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION,
                                     Uri.parse("package:" + getPackageName())));
        } catch (ActivityNotFoundException e) {  // (a phone without the page for one app: the list of them)
            startActivity(new Intent(Settings.ACTION_MANAGE_ALL_FILES_ACCESS_PERMISSION));
        }
    }

    // back with the permission: start again, for the data folder to be /sdcard/SavageWind
    @Override protected void onResume() {
        super.onResume();
        if (asked && Environment.isExternalStorageManager()) {
            startActivity(Intent.makeRestartActivityTask(getComponentName()));
            android.os.Process.killProcess(android.os.Process.myPid());
        }
    }
}
