package io.github.wxxsfxyzm.galaxybudsport.ssc;

import android.app.ActivityThread;
import android.app.BroadcastOptions;
import android.bluetooth.BluetoothDevice;
import android.content.Context;
import android.content.Intent;
import android.util.Log;
import io.github.libxposed.api.XposedInterface;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.List;
import java.lang.reflect.Modifier;
import org.luckypray.dexkit.DexKitBridge;
import org.luckypray.dexkit.query.FindMethod;
import org.luckypray.dexkit.query.matchers.MethodMatcher;
import org.luckypray.dexkit.result.FieldData;
import org.luckypray.dexkit.result.MethodData;
import org.luckypray.dexkit.result.MethodDataList;
import org.luckypray.dexkit.result.UsingFieldData;

/**
 * Reuses the Galaxy Buds manager's 360 audio / head tracking settings UI on non-Samsung phones,
 * with our own behavior behind the switches. The obfuscated targets are located by DexKit from
 * the strings and signatures they use, never by name.
 */
final class BudsSettingsBridge {
    static final String ACTION_SPATIAL = "io.github.wxxsfxyzm.galaxybudsport.action.BUDS_SPATIAL";
    static final String EXTRA_ENABLED = "enabled";
    static final String ACTION_UHQ = "io.github.wxxsfxyzm.galaxybudsport.action.BUDS_UHQ";
    static final String EXTRA_ADDRESS = "address";
    private static final String PREFS = "ssc_port";

    /** Per-device preference key (Samsung keeps the UHQ switch per device as well). */
    static String uhqKey(String address) {
        return "uhq:" + address;
    }
    private static final String TAG = "SscPortBuds";

    private BudsSettingsBridge() {
    }

    private static boolean spatial = true;
    private static boolean headTracking;
    private static Boolean sentSpatial;

    static void install(XposedInterface xposed, String apkPath, ClassLoader loader) {
        System.loadLibrary("dexkit");
        Method support;
        Method create;
        Method values;
        Method tracking;
        try (DexKitBridge bridge = DexKitBridge.create(apkPath)) {
            // SpatialSensorManager.isSupported(Context): what the settings view models ask.
            support = single(bridge, MethodMatcher.create()
                    .usingStrings("SpatialSensorManager", "isSupported() : ")
                    .modifiers(Modifier.STATIC)
                    .returnType("boolean")
                    .paramTypes("android.content.Context"), loader, "support");
            if (support == null) return;
            String manager = support.getDeclaringClass().getName();
            // handleDeviceExtendedStatusReady(address): creates Samsung's sensor object, whose
            // whole lifecycle throws on non-Samsung phones; it must not be created.
            create = single(bridge, MethodMatcher.create()
                    .declaredClass(manager)
                    .usingStrings("handleDeviceExtendedStatusReady()")
                    .returnType("void")
                    .paramTypes("java.lang.String"), loader, "create");
            // setSpatialValue(address, is360Audio, isHeadTracking): settings UI, service and the
            // per-model SPP status handlers all report the state here.
            values = single(bridge, MethodMatcher.create()
                    .declaredClass(manager)
                    .returnType("void")
                    .paramTypes("java.lang.String", "boolean", "boolean"), loader, "values");
            // setSpatialHeadTrackingValue(address, isHeadTracking): the head tracking switch.
            tracking = single(bridge, MethodMatcher.create()
                    .declaredClass(manager)
                    .usingStrings("setSpatialHeadTrackingValue() : isHeadTracking=")
                    .returnType("void")
                    .paramTypes("java.lang.String", "boolean"), loader, "tracking");
        }
        if (create == null || values == null || tracking == null) return;
        installUhq(xposed, apkPath, loader);

        xposed.hook(support).setExceptionMode(XposedInterface.ExceptionMode.PROTECTIVE)
                .intercept(chain -> true);
        xposed.hook(create).setExceptionMode(XposedInterface.ExceptionMode.PROTECTIVE)
                .intercept(chain -> null);
        xposed.hook(values).setExceptionMode(XposedInterface.ExceptionMode.PROTECTIVE)
                .intercept(chain -> {
                    apply((Boolean) chain.getArg(1), (Boolean) chain.getArg(2), "values");
                    return null;
                });
        xposed.hook(tracking).setExceptionMode(XposedInterface.ExceptionMode.PROTECTIVE)
                .intercept(chain -> {
                    apply(spatial, (Boolean) chain.getArg(1), "tracking");
                    return null;
                });
        Log.w(TAG, "stage=buds_settings event=installed support=" + describe(support)
                + " create=" + describe(create) + " values=" + describe(values)
                + " tracking=" + describe(tracking));
    }

    /**
     * The "advanced sound quality" (UHQ, 96 kHz SSC) switch. On Samsung phones the manager asks
     * the system through hidden BluetoothA2dp methods (semIsCodecSupported / semIsCodecEnabled /
     * semSetCodecEnabled with codec type 8); here the state lives in our preferences and the
     * Bluetooth process renegotiates SSC at 96 or 48 kHz.
     */
    private static void installUhq(XposedInterface xposed, String apkPath, ClassLoader loader) {
        Method supported;
        Method enabled;
        Method set;
        List<PriorityGuard> guards = new ArrayList<>();
        try (DexKitBridge bridge = DexKitBridge.create(apkPath)) {
            supported = single(bridge, MethodMatcher.create()
                    .usingStrings("semIsCodecSupported")
                    .modifiers(Modifier.STATIC)
                    .returnType("boolean")
                    .paramTypes("android.bluetooth.BluetoothDevice"), loader, "uhq_supported");
            enabled = single(bridge, MethodMatcher.create()
                    .usingStrings("semIsCodecEnabled")
                    .modifiers(Modifier.STATIC)
                    .returnType("boolean")
                    .paramTypes("android.bluetooth.BluetoothDevice"), loader, "uhq_enabled");
            set = single(bridge, MethodMatcher.create()
                    .usingStrings("semSetCodecEnabled")
                    .modifiers(Modifier.STATIC)
                    .returnType("void")
                    .paramTypes("boolean", "android.bluetooth.BluetoothDevice"), loader, "uhq_set");
            if (supported != null && enabled != null) {
                guards = findPriorityCallers(bridge, loader, supported, enabled);
            }
        }
        if (supported == null || enabled == null || set == null) return;
        for (PriorityGuard guard : guards) guard.install(xposed);
        xposed.hook(supported).setExceptionMode(XposedInterface.ExceptionMode.PROTECTIVE)
                .intercept(chain -> {
                    syncUhq((BluetoothDevice) chain.getArg(0));
                    return true;
                });
        xposed.hook(enabled).setExceptionMode(XposedInterface.ExceptionMode.PROTECTIVE)
                .intercept(chain -> {
                    BluetoothDevice device = (BluetoothDevice) chain.getArg(0);
                    syncUhq(device);
                    return uhq(device);
                });
        xposed.hook(set).setExceptionMode(XposedInterface.ExceptionMode.PROTECTIVE)
                .intercept(chain -> {
                    boolean on = (Boolean) chain.getArg(0);
                    BluetoothDevice device = (BluetoothDevice) chain.getArg(1);
                    Context context = ActivityThread.currentApplication();
                    if (device == null || context == null) return null;
                    context.getSharedPreferences(PREFS, Context.MODE_PRIVATE).edit()
                            .putBoolean(uhqKey(device.getAddress()), on).apply();
                    Log.w(TAG, "stage=buds_settings event=uhq_switch enabled=" + on);
                    sendUhq(device, on);
                    return null;
                });
        Log.w(TAG, "stage=buds_settings event=uhq_installed supported=" + describe(supported)
                + " enabled=" + describe(enabled) + " set=" + describe(set)
                + " priority_guards=" + guards.size());
    }

    private static final String SEM_GET_PRIORITY =
            "Landroid/bluetooth/BluetoothA2dp;->semGetPriority(Landroid/bluetooth/BluetoothDevice;)I";

    /**
     * Once UHQ is supported, the manager's sound quality screens call Samsung's
     * BluetoothA2dp.semGetPriority directly (not by reflection): the UHQ switch is enabled when it
     * returns 100 (the device's A2DP connection is allowed). The method does not exist here and
     * throws NoSuchMethodError. Each call is guarded by a null check of the manager's cached
     * BluetoothA2dp proxy, so the proxy field is cleared while such a method runs (the branch is
     * skipped), and the result is then set to "allowed": the screen state's {@code isDimUhq}, or
     * the settings search item's {@code enabled} for "uhq_title". The fields are found through the
     * labels of the state classes' own toString, not by name. The methods are located by DexKit
     * as those calling semGetPriority and the UHQ support or enabled check.
     */
    private static List<PriorityGuard> findPriorityCallers(DexKitBridge bridge, ClassLoader loader,
            Method supported, Method enabled) {
        List<PriorityGuard> guards = new ArrayList<>();
        java.util.Set<String> uhqChecks = new java.util.HashSet<>(
                List.of(descriptor(supported), descriptor(enabled)));
        MethodDataList callers = bridge.findMethod(FindMethod.create().matcher(MethodMatcher.create()
                .addInvoke(SEM_GET_PRIORITY)));
        for (MethodData caller : callers) {
            boolean uhq = false;
            for (MethodData invoked : caller.getInvokes()) {
                if (uhqChecks.contains(invoked.getDescriptor())) {
                    uhq = true;
                    break;
                }
            }
            if (!uhq) continue;
            FieldData proxy = null;
            for (UsingFieldData using : caller.getUsingFields()) {
                if ("android.bluetooth.BluetoothA2dp".equals(using.getField().getTypeName())) {
                    proxy = using.getField();
                    break;
                }
            }
            if (proxy == null) continue;
            // Static accessor of the object holding the proxy field (the manager's profile cache).
            MethodData holder = null;
            for (MethodData invoked : caller.getInvokes()) {
                if (Modifier.isStatic(invoked.getModifiers()) && invoked.getParamTypeNames().isEmpty()
                        && proxy.getDeclaredClassName().equals(invoked.getReturnTypeName())) {
                    holder = invoked;
                    break;
                }
            }
            try {
                Field field = proxy.getFieldInstance(loader);
                field.setAccessible(true);
                Method accessor = Modifier.isStatic(field.getModifiers()) || holder == null
                        ? null : holder.getMethodInstance(loader);
                if (!Modifier.isStatic(field.getModifiers()) && accessor == null) continue;
                if (accessor != null) accessor.setAccessible(true);
                guards.add(new PriorityGuard(caller.getMethodInstance(loader), field, accessor));
            } catch (ReflectiveOperationException error) {
                Log.e(TAG, "stage=buds_settings event=resolve_failed target=priority_guard");
            }
        }
        return guards;
    }

    private static String descriptor(Method method) {
        StringBuilder out = new StringBuilder("L")
                .append(method.getDeclaringClass().getName().replace('.', '/'))
                .append(";->").append(method.getName()).append('(');
        for (Class<?> type : method.getParameterTypes()) out.append(typeDescriptor(type));
        return out.append(')').append(typeDescriptor(method.getReturnType())).toString();
    }

    private static String typeDescriptor(Class<?> type) {
        if (type == boolean.class) return "Z";
        if (type == void.class) return "V";
        if (type == int.class) return "I";
        return "L" + type.getName().replace('.', '/') + ";";
    }

    private static final class PriorityGuard {
        private final Method method;
        private final Field proxy;
        private final Method holder;

        PriorityGuard(Method method, Field proxy, Method holder) {
            this.method = method;
            this.proxy = proxy;
            this.holder = holder;
        }

        void install(XposedInterface xposed) {
            xposed.hook(method).setExceptionMode(XposedInterface.ExceptionMode.PROTECTIVE)
                    .intercept(chain -> {
                        Object owner = holder == null ? null : holder.invoke(null);
                        Object saved = holder != null && owner == null ? null : proxy.get(owner);
                        if (saved == null) return chain.proceed();
                        proxy.set(owner, null);
                        Object result;
                        try {
                            result = chain.proceed();
                        } finally {
                            if (proxy.get(owner) == null) proxy.set(owner, saved);
                        }
                        allowUhq(chain.getThisObject(), result, chain.getArgs());
                        return result;
                    });
        }
    }

    private static final String UHQ_SEARCH_KEY = "uhq_title";

    /** Sets the semGetPriority-derived "connection allowed" flag in what the method produced. */
    private static void allowUhq(Object target, Object result, List<Object> args) {
        try {
            if (result != null && args.contains(UHQ_SEARCH_KEY)) {
                setLabelledBoolean(result, "enabled=", true);
                return;
            }
            if (target == null) return;
            for (Class<?> c = target.getClass(); c != null && c != Object.class; c = c.getSuperclass()) {
                for (Field field : c.getDeclaredFields()) {
                    if (Modifier.isStatic(field.getModifiers()) || field.getType().isPrimitive()) continue;
                    field.setAccessible(true);
                    Object holder = field.get(target);
                    if (holder == null) continue;
                    Method value;
                    try {
                        value = holder.getClass().getMethod("getValue");
                    } catch (NoSuchMethodException notAState) {
                        continue;
                    }
                    Object state = value.invoke(holder);
                    if (state != null && setLabelledBoolean(state, "isDimUhq=", true)) return;
                }
            }
        } catch (ReflectiveOperationException | RuntimeException error) {
            Log.e(TAG, "stage=buds_settings event=uhq_allow_failed error=" + error);
        }
    }

    /**
     * Sets the boolean field that {@code object.toString()} prints after {@code label}: each boolean
     * field is flipped once to see which one changes that value.
     */
    private static boolean setLabelledBoolean(Object object, String label, boolean wanted)
            throws IllegalAccessException {
        String before = labelled(object.toString(), label);
        if (before == null) return false;
        for (Class<?> c = object.getClass(); c != null && c != Object.class; c = c.getSuperclass()) {
            for (Field field : c.getDeclaredFields()) {
                if (field.getType() != boolean.class || Modifier.isStatic(field.getModifiers())) continue;
                field.setAccessible(true);
                boolean current = field.getBoolean(object);
                field.setBoolean(object, !current);
                String after = labelled(object.toString(), label);
                field.setBoolean(object, current);
                if (!before.equals(after)) {
                    field.setBoolean(object, wanted);
                    return true;
                }
            }
        }
        return false;
    }

    private static String labelled(String text, String label) {
        int start = text.indexOf(label);
        if (start < 0) return null;
        start += label.length();
        int end = start;
        while (end < text.length() && text.charAt(end) != ',' && text.charAt(end) != ')') end++;
        return text.substring(start, end);
    }

    private static final java.util.Set<String> uhqSynced = new java.util.HashSet<>();

    private static boolean uhq(BluetoothDevice device) {
        Context context = ActivityThread.currentApplication();
        return device != null && context != null
                && context.getSharedPreferences(PREFS, Context.MODE_PRIVATE)
                        .getBoolean(uhqKey(device.getAddress()), false);
    }

    /**
     * The manager's stored switch is the source of truth; the Bluetooth process follows it.
     * Sent once per device and manager process.
     */
    private static synchronized void syncUhq(BluetoothDevice device) {
        if (device == null || ActivityThread.currentApplication() == null) return;
        if (!uhqSynced.add(device.getAddress())) return;
        sendUhq(device, uhq(device));
    }

    private static void sendUhq(BluetoothDevice device, boolean enabled) {
        send(new Intent(ACTION_UHQ)
                .putExtra(EXTRA_ADDRESS, device.getAddress())
                .putExtra(EXTRA_ENABLED, enabled));
    }

    private static final String KEY_SPATIAL = "spatial";
    private static final String KEY_HEAD_TRACKING = "head_tracking";
    private static boolean restored;

    /**
     * Restores the last 360 audio / head tracking switches when the manager process (re)starts:
     * the manager only reports them through the skipped Samsung sensor path or when the user
     * changes them, so a restarted manager would otherwise leave head tracking off. Called when
     * the manager's SPP session comes up.
     */
    static synchronized void restore() {
        if (restored) return;
        Context context = ActivityThread.currentApplication();
        if (context == null) return;
        restored = true;
        android.content.SharedPreferences prefs =
                context.getSharedPreferences(PREFS, Context.MODE_PRIVATE);
        if (!prefs.contains(KEY_SPATIAL)) return;
        apply(prefs.getBoolean(KEY_SPATIAL, true), prefs.getBoolean(KEY_HEAD_TRACKING, false),
                "restore");
    }

    private static synchronized void apply(boolean spatialOn, boolean tracking, String source) {
        restored = true;
        spatial = spatialOn;
        headTracking = tracking;
        Context context = ActivityThread.currentApplication();
        if (context != null) {
            context.getSharedPreferences(PREFS, Context.MODE_PRIVATE).edit()
                    .putBoolean(KEY_SPATIAL, spatial)
                    .putBoolean(KEY_HEAD_TRACKING, headTracking)
                    .apply();
        }
        Log.w(TAG, "stage=buds_settings event=switch source=" + source + " spatial=" + spatial
                + " head_tracking=" + headTracking);
        if (sentSpatial == null || sentSpatial != spatial) {
            sentSpatial = spatial;
            sendSpatial(spatial);
        }
        BudsPoseTap.setHeadTracking(spatial && headTracking);
    }

    private static Method single(DexKitBridge bridge, MethodMatcher matcher, ClassLoader loader,
            String what) {
        MethodDataList found = bridge.findMethod(FindMethod.create().matcher(matcher));
        if (found.size() != 1) {
            Log.e(TAG, "stage=buds_settings event=lookup_failed target=" + what
                    + " matches=" + found.size());
            return null;
        }
        MethodData data = found.get(0);
        try {
            return data.getMethodInstance(loader);
        } catch (NoSuchMethodException error) {
            Log.e(TAG, "stage=buds_settings event=resolve_failed target=" + what);
            return null;
        }
    }

    private static String describe(Method method) {
        return method.getDeclaringClass().getName() + "." + method.getName();
    }

    private static void sendSpatial(boolean enabled) {
        send(new Intent(ACTION_SPATIAL).putExtra(EXTRA_ENABLED, enabled));
    }

    private static void send(Intent intent) {
        Context context = ActivityThread.currentApplication();
        if (context == null) return;
        BroadcastOptions options = BroadcastOptions.makeBasic();
        options.setShareIdentityEnabled(true);
        context.sendBroadcast(intent.setPackage("com.android.bluetooth"), null, options.toBundle());
    }

}
