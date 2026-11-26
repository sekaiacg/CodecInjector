package io.github.wxxsfxyzm.galaxybudsport;

import io.github.libxposed.api.XposedModule;
import io.github.libxposed.api.XposedModuleInterface;
import io.github.wxxsfxyzm.galaxybudsport.aeld.AeldFeature;
import io.github.wxxsfxyzm.galaxybudsport.core.PortRuntime;
import io.github.wxxsfxyzm.galaxybudsport.ssc.SscFeature;
import java.util.List;

/** LSPosed entry: the core with the codec modules this build includes. */
public final class GalaxyBudsPortModule extends XposedModule {
    private final PortRuntime runtime =
            new PortRuntime(this, "codecinjector", List.of(new SscFeature(), new AeldFeature()));

    @Override public void onModuleLoaded(XposedModuleInterface.ModuleLoadedParam param) {
        runtime.onModuleLoaded(param);
    }

    @Override public void onPackageReady(XposedModuleInterface.PackageReadyParam param) {
        runtime.onPackageReady(param);
    }
}
