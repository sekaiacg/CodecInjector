# Release shrinking (R8). Nothing is renamed: JNI function names and log stack traces stay as they
# are; R8 only removes what is unused, mostly the parts of kotlin-stdlib (pulled in by DexKit) that
# DexKit does not call.
-dontobfuscate

# The LSPosed entry, named in META-INF/xposed/java_init.list (invisible to R8).
-keep class io.github.wxxsfxyzm.codecinjector.CodecInjectorModule { *; }

# Native methods implemented in libcodecinjector.so (Java_<class>_<method> symbols).
-keepclasseswithmembernames,includedescriptorclasses class * {
    native <methods>;
}

# DexKit: its own consumer rules keep the natives libdexkit.so binds to (it looks up no Java class
# by name), so the rest of its Java API is shrunk like our code.

# libxposed and the hidden framework APIs are provided at runtime.
-dontwarn io.github.libxposed.**
-dontwarn kotlin.**
