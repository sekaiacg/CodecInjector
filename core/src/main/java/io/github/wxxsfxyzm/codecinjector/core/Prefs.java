package io.github.wxxsfxyzm.codecinjector.core;

import android.content.Context;
import android.content.SharedPreferences;
import java.util.Map;
import java.util.Set;

/**
 * Preferences the module keeps in its host apps (Bluetooth, Settings, the Buds managers). Files of
 * the project's earlier names are moved over once, the first time a renamed file is opened.
 */
public final class Prefs {
    private Prefs() {
    }

    public static SharedPreferences open(Context context, String name, String formerName) {
        SharedPreferences prefs = context.getSharedPreferences(name, Context.MODE_PRIVATE);
        if (formerName == null || !prefs.getAll().isEmpty()) return prefs;
        SharedPreferences former = context.getSharedPreferences(formerName, Context.MODE_PRIVATE);
        Map<String, ?> values = former.getAll();
        if (values.isEmpty()) return prefs;
        SharedPreferences.Editor editor = prefs.edit();
        for (Map.Entry<String, ?> entry : values.entrySet()) {
            Object value = entry.getValue();
            String key = entry.getKey();
            if (value instanceof Boolean) editor.putBoolean(key, (Boolean) value);
            else if (value instanceof Integer) editor.putInt(key, (Integer) value);
            else if (value instanceof Long) editor.putLong(key, (Long) value);
            else if (value instanceof Float) editor.putFloat(key, (Float) value);
            else if (value instanceof String) editor.putString(key, (String) value);
            else if (value instanceof Set) {
                @SuppressWarnings("unchecked") Set<String> set = (Set<String>) value;
                editor.putStringSet(key, set);
            }
        }
        if (editor.commit()) {
            former.edit().clear().commit();
            context.deleteSharedPreferences(formerName);
        }
        return prefs;
    }
}
