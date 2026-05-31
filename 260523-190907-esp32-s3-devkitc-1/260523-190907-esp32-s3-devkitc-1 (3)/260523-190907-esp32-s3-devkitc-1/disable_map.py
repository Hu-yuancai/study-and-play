Import("env")

# Remove any map file flag — the ESP32 linker can't handle Unicode paths on Windows
link_flags = env["LINKFLAGS"]
env["LINKFLAGS"] = [f for f in link_flags if not str(f).startswith("-Wl,-Map=")]
