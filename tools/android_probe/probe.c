// Savage Wind on Android, a first look: what a phone has of what the Linux runtime needs, shown on screen by the probe
// app (tools/android_probe/build.sh). Each check mirrors the runtime: the guest address window (runtime/rt.h,
// plat_reserve_guest in runtime/platform/linux.c), the Vulkan device it creates (runtime/nvn/vulkan/device.c), memory.
// Verdicts: [ok], [work] (code to write for this phone), [tight], [NO] (this phone can't run it as things are).
#include <errno.h>
#include <jni.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <vulkan/vulkan.h>

#ifndef MAP_FIXED_NOREPLACE
#define MAP_FIXED_NOREPLACE 0x100000
#endif
#define GUEST_LO 0x7000000000ull  // runtime/rt.h
#define GUEST_HI 0x7320000000ull
#define GUEST_TOP (1ull << 39)    // the Switch's address space: wherever the window goes, it stays below this
#define GB(x) ((double)(x) / (1ull << 30))

typedef struct { char s[16384]; size_t n; } Out;
static Out verdict, detail;

static void say(Out *o, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int k = vsnprintf(o->s + o->n, sizeof o->s - o->n, fmt, ap);
    va_end(ap);
    if (k > 0) o->n = o->n + (size_t)k < sizeof o->s ? o->n + (size_t)k : sizeof o->s - 1;
}

// Guest addresses are host addresses, so the runtime needs this exact window. Every app is forked from the zygote,
// whose libraries sit at a spot picked at random each boot (anywhere in roughly 0x6f..-0x7f.. with 39-bit addresses),
// and that can be in the window: list what's there, and the largest free stretch below 2^39 it could move to.
static void address(void) {
    uint64_t prev = 0x100000000ull, gap_lo = 0, gap_sz = 0, top = 0;  // (below 4 GB: ART's heap)
    int in = 0;
    char line[512];
    FILE *f = fopen("/proc/self/maps", "r");
    while (f && fgets(line, sizeof line, f)) {
        unsigned long long a, b;
        if (sscanf(line, "%llx-%llx", &a, &b) != 2) continue;
        uint64_t end = a < GUEST_TOP ? a : GUEST_TOP;
        if (end > prev && end - prev > gap_sz) gap_lo = prev, gap_sz = end - prev;
        if (a < GUEST_HI && b > GUEST_LO && in++ < 8) say(&detail, "  in the window: %s", line);
        if (b > prev) prev = b;
        top = b;
    }
    if (f) fclose(f);
    if (prev < GUEST_TOP && GUEST_TOP - prev > gap_sz) gap_lo = prev, gap_sz = GUEST_TOP - prev;
    int bits = 0;
    while (bits < 64 && (1ull << bits) < top) bits++;
    say(&detail, "addresses: %d-bit; largest free stretch below 2^39: %#llx-%#llx (%.1f GB)\n", bits,
        (unsigned long long)gap_lo, (unsigned long long)(gap_lo + gap_sz), GB(gap_sz));

    void *p = mmap((void *)(uintptr_t)GUEST_LO, GUEST_HI - GUEST_LO, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE | MAP_FIXED_NOREPLACE, -1, 0);
    int err = errno;
    if (p != MAP_FAILED) munmap(p, GUEST_HI - GUEST_LO);
    uint64_t base = (gap_lo + 0x3fffffff) & ~0x3fffffffull;
    if (p == (void *)(uintptr_t)GUEST_LO)
        say(&verdict, "[ok]    guest window %#llx-%#llx free\n", GUEST_LO, GUEST_HI);
    else if (base + (GUEST_HI - GUEST_LO) <= gap_lo + gap_sz)
        say(&verdict, "[work]  guest window taken (%s, %d mappings): move it, e.g. to %#llx\n",
            p == MAP_FAILED ? strerror(err) : "the kernel put it elsewhere", in, (unsigned long long)base);
    else say(&verdict, "[NO]    guest window taken, and no 12.5 GB free below 2^39\n");
}

// BotW has 3.2 GB of the Switch's 4; the runtime, the GPU's copies and Android come on top
static void memory(void) {
    long page = sysconf(_SC_PAGESIZE);
    unsigned long long total = 0, avail = 0;
    char line[256];
    FILE *f = fopen("/proc/meminfo", "r");
    while (f && fgets(line, sizeof line, f)) sscanf(line, "MemTotal: %llu", &total), sscanf(line, "MemAvailable: %llu", &avail);
    if (f) fclose(f);
    double t = GB(total << 10);
    say(&verdict, t >= 7 ? "[ok]    %.1f GB RAM\n" : t >= 5 ? "[tight] %.1f GB RAM\n" : "[NO]    %.1f GB RAM\n", t);
    say(&detail, "RAM: %.1f GB, %.1f GB available now\n", t, GB(avail << 10));
    if (page == 4096) say(&verdict, "[ok]    4 KB pages\n");
    else say(&verdict, "[work]  %ld KB pages: the runtime's 4 KB page protections need a look\n", page / 1024);
}

static void cpu(void) {
    long n = sysconf(_SC_NPROCESSORS_CONF);
    unsigned khz[16];
    int cnt[16], k = 0;
    for (long i = 0; i < n && i < 64; i++) {
        char path[96];
        unsigned v = 0;
        snprintf(path, sizeof path, "/sys/devices/system/cpu/cpu%ld/cpufreq/cpuinfo_max_freq", i);
        FILE *f = fopen(path, "r");
        if (!f) continue;
        if (fscanf(f, "%u", &v) != 1) v = 0;
        fclose(f);
        int j = 0;
        while (j < k && khz[j] != v) j++;
        if (j == k && k < 16) khz[k] = v, cnt[k++] = 0;
        if (j < k) cnt[j]++;
    }
    say(&detail, "CPU: %ld cores", n);
    for (int j = 0; j < k; j++) say(&detail, "%s%dx %.2f GHz", j ? " + " : ", ", cnt[j], khz[j] / 1e6);
    say(&detail, " (the Switch: 4x 1.02 GHz Cortex-A57)\n");
}

static bool has_ext(const VkExtensionProperties *e, uint32_t n, const char *name) {
    for (uint32_t i = 0; i < n; i++)
        if (!strcmp(e[i].extensionName, name)) return true;
    return false;
}

// What device.c can't do without: Vulkan 1.3 (it skips older GPUs) and BC textures (uploaded as they are, with no
// decoder: Adreno has them, Mali doesn't). The rest it does without, logging what's missing, as listed here.
static void gpu(VkPhysicalDevice pd, uint32_t inst_api) {
    VkPhysicalDeviceProperties p;
    vkGetPhysicalDeviceProperties(pd, &p);
    uint32_t api = p.apiVersion < inst_api ? p.apiVersion : inst_api;
    VkPhysicalDeviceVulkan13Features f13 = {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
    VkPhysicalDeviceVulkan12Features f12 = {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
                                            .pNext = api >= VK_API_VERSION_1_3 ? &f13 : NULL};
    VkPhysicalDeviceVulkan11Features f11 = {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES, .pNext = &f12};
    VkPhysicalDeviceFeatures2 f = {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
                                   .pNext = api >= VK_API_VERSION_1_2 ? &f11 : NULL};
    VkPhysicalDeviceDriverProperties dp = {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES};
    VkPhysicalDeviceProperties2 p2 = {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
                                      .pNext = api >= VK_API_VERSION_1_2 ? &dp : NULL};
    if (api >= VK_API_VERSION_1_1) {
        vkGetPhysicalDeviceFeatures2(pd, &f);
        vkGetPhysicalDeviceProperties2(pd, &p2);
    } else vkGetPhysicalDeviceFeatures(pd, &f.features);

    say(&detail, "\nGPU: %s, Vulkan %u.%u.%u", p.deviceName, VK_API_VERSION_MAJOR(p.apiVersion),
        VK_API_VERSION_MINOR(p.apiVersion), VK_API_VERSION_PATCH(p.apiVersion));
    if (dp.driverName[0]) say(&detail, ", driver %s %s", dp.driverName, dp.driverInfo);
    say(&detail, "\n");
    if (api >= VK_API_VERSION_1_3) say(&verdict, "[ok]    Vulkan 1.3 (%s)\n", p.deviceName);
    else say(&verdict, "[NO]    Vulkan %u.%u on %s: the renderer needs 1.3 (a newer driver could have it)\n",
             VK_API_VERSION_MAJOR(api), VK_API_VERSION_MINOR(api), p.deviceName);
    if (f.features.textureCompressionBC) say(&verdict, "[ok]    BC textures\n");
    else say(&verdict, "[work]  no BC textures: needs a BC decoder, like the ASTC one\n");

    say(&detail, "ASTC textures: %s\n", f.features.textureCompressionASTC_LDR ? "yes" : "no");
    VkPhysicalDeviceMemoryProperties mp;
    vkGetPhysicalDeviceMemoryProperties(pd, &mp);
    for (uint32_t i = 0; i < mp.memoryHeapCount; i++)
        if (mp.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) say(&detail, "GPU heap %u: %.1f GB\n", i, GB(mp.memoryHeaps[i].size));

    say(&detail, "features missing (the renderer does without):");
#define OPT(s, x) if (!s.x) say(&detail, " " #x)
    OPT(f.features, independentBlend); OPT(f.features, dualSrcBlend); OPT(f.features, fillModeNonSolid);
    OPT(f.features, samplerAnisotropy); OPT(f.features, shaderInt64); OPT(f.features, depthBiasClamp);
    OPT(f.features, depthClamp); OPT(f.features, occlusionQueryPrecise); OPT(f.features, largePoints);
    OPT(f.features, wideLines); OPT(f.features, imageCubeArray); OPT(f.features, sampleRateShading);
    OPT(f.features, fragmentStoresAndAtomics); OPT(f.features, vertexPipelineStoresAndAtomics);
    OPT(f.features, robustBufferAccess); OPT(f.features, shaderClipDistance);
    OPT(f11, storageBuffer16BitAccess);
    OPT(f12, timelineSemaphore); OPT(f12, shaderInt8); OPT(f12, samplerMirrorClampToEdge);
    OPT(f12, uniformAndStorageBuffer8BitAccess); OPT(f12, storageBuffer8BitAccess); OPT(f12, scalarBlockLayout);
    OPT(f13, dynamicRendering); OPT(f13, synchronization2); OPT(f13, shaderDemoteToHelperInvocation);
    OPT(f13, shaderTerminateInvocation); OPT(f13, maintenance4);
#undef OPT
    static const char *const exts[] = {"VK_EXT_custom_border_color", "VK_EXT_vertex_attribute_divisor",
                                       "VK_EXT_external_memory_host", "VK_KHR_maintenance5",
                                       "VK_EXT_graphics_pipeline_library", "VK_EXT_attachment_feedback_loop_layout",
                                       "VK_KHR_dynamic_rendering_local_read"};
    uint32_t ne = 0;
    vkEnumerateDeviceExtensionProperties(pd, NULL, &ne, NULL);
    VkExtensionProperties *ep = calloc(ne ? ne : 1, sizeof *ep);
    vkEnumerateDeviceExtensionProperties(pd, NULL, &ne, ep);
    say(&detail, "\nextensions missing (optional):");
    for (size_t i = 0; i < sizeof exts / sizeof *exts; i++)
        if (!has_ext(ep, ne, exts[i])) say(&detail, " %s", exts[i]);
    say(&detail, "\n");
    free(ep);
}

static void vulkan(void) {
    uint32_t iv = VK_API_VERSION_1_0;
    PFN_vkEnumerateInstanceVersion ev = (PFN_vkEnumerateInstanceVersion)vkGetInstanceProcAddr(NULL, "vkEnumerateInstanceVersion");
    if (ev) ev(&iv);
    VkApplicationInfo ai = {.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO, .pApplicationName = "Savage Wind probe",
                            .apiVersion = iv < VK_API_VERSION_1_3 ? iv : VK_API_VERSION_1_3};
    VkInstanceCreateInfo ici = {.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, .pApplicationInfo = &ai};
    VkInstance inst;
    VkResult r = vkCreateInstance(&ici, NULL, &inst);
    if (r != VK_SUCCESS) {
        say(&verdict, "[NO]    no Vulkan (vkCreateInstance: %d)\n", r);
        return;
    }
    VkPhysicalDevice pd[4];
    uint32_t n = 4;
    vkEnumeratePhysicalDevices(inst, &n, pd);
    if (!n) say(&verdict, "[NO]    no Vulkan GPU\n");
    for (uint32_t i = 0; i < n; i++) gpu(pd[i], ai.apiVersion);
    vkDestroyInstance(inst, NULL);
}

JNIEXPORT jstring JNICALL Java_savagewind_probe_Probe_run(JNIEnv *env, jclass cls) {
    (void)cls;
    static char all[sizeof verdict.s + sizeof detail.s + 2];
    verdict.n = detail.n = 0;
    address();  // before Vulkan loads the GPU driver, as in the runtime
    memory();
    cpu();
    vulkan();
    snprintf(all, sizeof all, "%s\n%s", verdict.s, detail.s);
    return (*env)->NewStringUTF(env, all);
}
