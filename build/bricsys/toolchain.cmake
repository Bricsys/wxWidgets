set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)  # Optional: disable compiler-specific extensions

# Ensure the GDK Wayland backend is visible to all translation units.
# On Linux, GDK headers normally define this when the Wayland backend is
# present; declaring it here makes it explicit for the wxWidgets build.
if(UNIX AND NOT APPLE)
    add_compile_definitions(GDK_WINDOWING_WAYLAND)
endif()

# Enable metafile support
set(wxUSE_METAFILE ON CACHE BOOL "Enable metafile support" FORCE)
set(wxUSE_WIN_METAFILES_ALWAYS ON CACHE BOOL "Enable Windows metafiles support" FORCE)

# Keep enhanced metafiles enabled (default)
set(wxUSE_ENH_METAFILE ON CACHE BOOL "Enable enhanced metafiles" FORCE)
