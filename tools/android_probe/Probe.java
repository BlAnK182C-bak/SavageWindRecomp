// The probe app's one screen: the phone, then probe.c's report (selectable, to copy it).
package savagewind.probe;

import android.app.Activity;
import android.graphics.Typeface;
import android.os.Build;
import android.os.Bundle;
import android.widget.ScrollView;
import android.widget.TextView;

public class Probe extends Activity {
    static { System.loadLibrary("probe"); }
    static native String run();

    @Override protected void onCreate(Bundle state) {
        super.onCreate(state);
        String soc = Build.VERSION.SDK_INT >= 31 ? Build.SOC_MANUFACTURER + " " + Build.SOC_MODEL : Build.HARDWARE;
        TextView t = new TextView(this);
        t.setTypeface(Typeface.MONOSPACE);
        t.setTextSize(11);
        t.setTextIsSelectable(true);
        t.setPadding(32, 32, 32, 32);
        t.setText(Build.MANUFACTURER + " " + Build.MODEL + ", " + soc + ", Android " + Build.VERSION.RELEASE
                  + " (API " + Build.VERSION.SDK_INT + ")\n\n" + run());
        ScrollView s = new ScrollView(this);
        s.addView(t);
        setContentView(s);
    }
}
