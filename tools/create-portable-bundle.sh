#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Run inside a real validation VM. Never installs libraries on the user's host.
set -euo pipefail
export LC_ALL=C

die() { printf 'Portable bundle: %s\n' "$*" >&2; exit 1; }
[[ $# -eq 2 ]] || die 'usage: create-portable-bundle.sh RELEASE_BUILD_DIRECTORY OUTPUT_DIRECTORY'

# A container or chroot is not a substitute for the requested VM verification.
if command -v systemd-detect-virt >/dev/null 2>&1 && systemd-detect-virt --container --quiet; then
    die 'refusing to package in a container; run this command in the validation VM'
fi
vm=false
if command -v systemd-detect-virt >/dev/null 2>&1 && systemd-detect-virt --vm --quiet; then vm=true; fi
dmi=$(cat /sys/class/dmi/id/sys_vendor /sys/class/dmi/id/product_name 2>/dev/null || true)
case "$dmi" in *QEMU*|*KVM*|*VirtualBox*|*VMware*|*'Virtual Machine'*|*Xen*) vm=true ;; esac
[[ "$vm" == true ]] || die 'a real virtual-machine identity was not detected'
[[ $(uname -m) == x86_64 ]] || die 'this portable bundle currently supports Linux x86_64 only'

for command in bash awk basename cat chmod cmp cp dirname env find grep gzip ldd mkdir mktemp readlink rm sed sha256sum sort strip tar xargs; do
    command -v "$command" >/dev/null 2>&1 || die "missing packaging command: $command"
done

build=$(cd -- "$1" && pwd -P)
[[ -f "$build/CMakeCache.txt" ]] || die 'the Release build directory has no CMake cache'
grep -qx 'CMAKE_BUILD_TYPE:STRING=Release' "$build/CMakeCache.txt" || die 'a Release build is required'
[[ -x "$build/syspulse" ]] || die 'the Release SysPulse executable was not found'
version_header="$build/generated/tux/version.hpp"
[[ -r "$version_header" ]] || die 'the generated release-version header is missing'
version=$(sed -n 's/.*Version = "\([^"]*\)".*/\1/p' "$version_header")
[[ "$version" =~ ^[0-9]+\.[0-9]+\.[0-9]+[A-Za-z0-9.+~-]*$ ]] || die 'cannot determine a valid release version'
project=$(cd -- "$(dirname -- "$0")/.." && pwd -P)
[[ -r "$project/LICENSE" ]] || die 'the application license file is missing'
mkdir -p -- "$2"
output=$(cd -- "$2" && pwd -P)
name="syspulse-${version}-linux-x86_64-portable"
[[ ! -e "$output/$name.tar.gz" && ! -e "$output/$name.tar.gz.sha256" ]] || die 'the output archive already exists; use a fresh output directory'
staging=$(mktemp -d "$output/.syspulse-bundle-XXXXXX")
trap 'rm -rf -- "$staging"' EXIT
bundle="$staging/$name"
mkdir -p "$bundle/bin" "$bundle/lib" "$bundle/plugins/platforms" "$bundle/licenses/packages"
cp -- "$build/syspulse" "$bundle/bin/syspulse"
strip --strip-unneeded "$bundle/bin/syspulse"
cp -- "$project/LICENSE" "$bundle/licenses/SysPulse-GPL-3.0.txt"
[[ -d /usr/share/common-licenses ]] || die 'Debian common-license texts are missing'
cp -RL -- /usr/share/common-licenses "$bundle/licenses/common-licenses"

declare -a queue=()
declare -A seen=()
declare -A copied=()
declare -A noticed=()
printf 'file\tpackage\tbinary_version\tsource_package\tsource_version\tprovenance\n' > "$bundle/DEPENDENCIES.tsv"
missing="$staging/missing-notices.txt"
: > "$missing"

# This fallback maps known Debian SONAMEs to the package that supplies their
# actual copyright notice. It does not invent an installed version. If neither
# exact dpkg ownership nor a mapped notice exists, packaging fails closed.
notice_candidates() {
    local name=$1 abi
    case "$name" in
        ld-linux-x86-64.so.*|libc.so.*|libm.so.*|libdl.so.*|libpthread.so.*|librt.so.*|libresolv.so.*|libutil.so.*|libanl.so.*) printf '%s\n' libc6 ;;
        libQt5Core.so.*) printf '%s\n' libqt5core5t64 libqt5core5 ;;
        libQt5Gui.so.*|libQt5XcbQpa.so.*|libqxcb.so|libqoffscreen.so|libqminimal.so) printf '%s\n' libqt5gui5t64 libqt5gui5 ;;
        libQt5Widgets.so.*) printf '%s\n' libqt5widgets5t64 libqt5widgets5 ;;
        libQt5Network.so.*) printf '%s\n' libqt5network5t64 libqt5network5 ;;
        libQt5DBus.so.*) printf '%s\n' libqt5dbus5t64 libqt5dbus5 ;;
        libgcc_s.so.*) printf '%s\n' libgcc-s1 ;;
        libstdc++.so.*) printf '%s\n' libstdc++6 ;;
        libatomic.so.*) printf '%s\n' libatomic1 ;;
        libz.so.*) printf '%s\n' zlib1g ;;
        libpcre2-16.so.*) printf '%s\n' libpcre2-16-0 ;;
        libpcre2-8.so.*) printf '%s\n' libpcre2-8-0 ;;
        libdouble-conversion.so.*) printf '%s\n' libdouble-conversion3 ;;
        libicu*.so.*) abi=${name##*.so.}; printf 'libicu%s\n' "${abi%%.*}" ;;
        libmd4c.so.*) printf '%s\n' libmd4c0 ;;
        libharfbuzz.so.*) printf '%s\n' libharfbuzz0b ;;
        libpng16.so.*) printf '%s\n' libpng16-16t64 libpng16-16 ;;
        libfreetype.so.*) printf '%s\n' libfreetype6 ;;
        libfontconfig.so.*) printf '%s\n' libfontconfig1 ;;
        libglib-2.0.so.*|libgobject-2.0.so.*|libgmodule-2.0.so.*|libgio-2.0.so.*|libgthread-2.0.so.*) printf '%s\n' libglib2.0-0t64 libglib2.0-0 ;;
        libgraphite2.so.*) printf '%s\n' libgraphite2-3 ;;
        libbrotlidec.so.*|libbrotlicommon.so.*|libbrotlienc.so.*) printf '%s\n' libbrotli1 ;;
        libexpat.so.*) printf '%s\n' libexpat1 ;;
        libbz2.so.*) printf '%s\n' libbz2-1.0 ;;
        libxcb.so.*) printf '%s\n' libxcb1 ;;
        libxcb-dri2.so.*) printf '%s\n' libxcb-dri2-0 ;;
        libxcb-dri3.so.*) printf '%s\n' libxcb-dri3-0 ;;
        libxcb-icccm.so.*) printf '%s\n' libxcb-icccm4 ;;
        libxcb-keysyms.so.*) printf '%s\n' libxcb-keysyms1 ;;
        libxcb-sync.so.*) printf '%s\n' libxcb-sync1 ;;
        libxcb-util.so.*) printf '%s\n' libxcb-util1 ;;
        libxcb-xkb.so.*) printf '%s\n' libxcb-xkb1 ;;
        libxcb-*.so.*) name=${name%%.so.*}; printf '%s0\n' "$name" ;;
        libxkbcommon-x11.so.*) printf '%s\n' libxkbcommon-x11-0 ;;
        libxkbcommon.so.*) printf '%s\n' libxkbcommon0 ;;
        libSM.so.*) printf '%s\n' libsm6 ;;
        libICE.so.*) printf '%s\n' libice6 ;;
        libX11-xcb.so.*) printf '%s\n' libx11-xcb1 ;;
        libX11.so.*) printf '%s\n' libx11-6 ;;
        libXext.so.*) printf '%s\n' libxext6 ;;
        libXau.so.*) printf '%s\n' libxau6 ;;
        libXdmcp.so.*) printf '%s\n' libxdmcp6 ;;
        libXrender.so.*) printf '%s\n' libxrender1 ;;
        libXfixes.so.*) printf '%s\n' libxfixes3 ;;
        libXcursor.so.*) printf '%s\n' libxcursor1 ;;
        libXi.so.*) printf '%s\n' libxi6 ;;
        libXrandr.so.*) printf '%s\n' libxrandr2 ;;
        libXss.so.*) printf '%s\n' libxss1 ;;
        libGL.so.*) printf '%s\n' libgl1 ;;
        libGLX.so.*) printf '%s\n' libglx0 ;;
        libGLdispatch.so.*) printf '%s\n' libglvnd0 ;;
        libOpenGL.so.*) printf '%s\n' libopengl0 ;;
        libEGL.so.*) printf '%s\n' libegl1 ;;
        libdbus-1.so.*) printf '%s\n' libdbus-1-3 ;;
        libsystemd.so.*) printf '%s\n' libsystemd0 ;;
        libcap.so.*) printf '%s\n' libcap2 ;;
        libgcrypt.so.*) printf '%s\n' libgcrypt20 ;;
        libgpg-error.so.*) printf '%s\n' libgpg-error0 ;;
        liblzma.so.*) printf '%s\n' liblzma5 ;;
        liblz4.so.*) printf '%s\n' liblz4-1 ;;
        libzstd.so.*) printf '%s\n' libzstd1 ;;
        libbsd.so.*) printf '%s\n' libbsd0 ;;
        libmd.so.*) printf '%s\n' libmd0 ;;
        libuuid.so.*) printf '%s\n' libuuid1 ;;
        libmount.so.*) printf '%s\n' libmount1 ;;
        libblkid.so.*) printf '%s\n' libblkid1 ;;
        libselinux.so.*) printf '%s\n' libselinux1 ;;
        libffi.so.*) printf '%s\n' libffi8 ;;
        libdrm.so.*) printf '%s\n' libdrm2 ;;
        libjpeg.so.*) printf '%s\n' libjpeg62-turbo ;;
        libnsl.so.*) printf '%s\n' libnsl2 ;;
        libtirpc.so.*) printf '%s\n' libtirpc3t64 libtirpc3 ;;
        libproxy.so.*) printf '%s\n' libproxy1v5 ;;
        libgssapi_krb5.so.*) printf '%s\n' libgssapi-krb5-2 ;;
        libkrb5.so.*) printf '%s\n' libkrb5-3 ;;
        libk5crypto.so.*) printf '%s\n' libk5crypto3 ;;
        libkrb5support.so.*) printf '%s\n' libkrb5support0 ;;
        libcom_err.so.*) printf '%s\n' libcom-err2 ;;
        libkeyutils.so.*) printf '%s\n' libkeyutils1 ;;
    esac
}

add_notice() {
    local source=$1 destination=$2 owner='' ownership='' candidate='' notice='' package='' provenance='notice-mapping' documentation_root=''
    local binary_version='unavailable' source_package='unavailable' source_version='unavailable' values=''
    if command -v dpkg-query >/dev/null 2>&1; then
        for candidate in "$source" "$(readlink -f -- "$source")"; do
            ownership=$(dpkg-query -S "$candidate" 2>/dev/null || true)
            owner=$(printf '%s\n' "$ownership" | awk -F': /' 'NF >= 2 && !found {print $1; found=1}')
            [[ -n "$owner" ]] && break
        done
        if [[ -n "$owner" ]]; then
            candidate=${owner%%:*}
            if [[ -r "/usr/share/doc/$candidate/copyright" ]]; then
                package=$candidate
                documentation_root=/usr/share/doc
                provenance='dpkg-ownership'
                values=$(dpkg-query -W -f='${Version}\t${source:Package}\t${source:Version}' "$owner" 2>/dev/null || true)
                if [[ -n "$values" ]]; then IFS=$'\t' read -r binary_version source_package source_version <<< "$values"; fi
            fi
        fi
    fi
    if [[ -z "$package" ]]; then
        while IFS= read -r candidate; do
            [[ -n "$candidate" ]] || continue
            for documentation_root in /usr/share/doc /opt/builddeps/usr/share/doc; do
                if [[ -r "$documentation_root/$candidate/copyright" ]]; then
                    package=$candidate
                    break 2
                fi
            done
        done < <(notice_candidates "$(basename -- "$source")")
    fi
    if [[ -z "$package" ]]; then
        printf '%s\n' "$(basename -- "$source")" >> "$missing"
        return
    fi
    notice="$documentation_root/$package/copyright"
    if [[ ! -v "noticed[$package]" ]]; then
        mkdir -p "$bundle/licenses/packages/$package"
        cp -L -- "$notice" "$bundle/licenses/packages/$package/copyright"
        noticed[$package]=1
    fi
    printf '%s\t%s\t%s\t%s\t%s\t%s\n' "$destination" "$package" "$binary_version" "$source_package" "$source_version" "$provenance" >> "$bundle/DEPENDENCIES.tsv"
}

copy_library() {
    local source=$1 base
    [[ -r "$source" ]] || die 'a runtime dependency is unreadable'
    base=$(basename -- "$source")
    if [[ -v "copied[$base]" ]]; then
        cmp -s -- "$source" "$bundle/lib/$base" || die "different runtime files share the name $base"
        return
    fi
    cp -L -- "$source" "$bundle/lib/$base"
    copied[$base]=1
    queue+=("$source")
    add_notice "$source" "lib/$base"
}

runtime_library() {
    local soname=$1 path
    for path in "/usr/lib/x86_64-linux-gnu/$soname" "/lib/x86_64-linux-gnu/$soname" "/usr/lib/$soname" "/opt/builddeps/usr/lib/x86_64-linux-gnu/$soname"; do
        if [[ -r "$path" ]]; then printf '%s\n' "$path"; return; fi
    done
    return 1
}

queue+=("$build/syspulse")
for component in Core Gui Widgets Network; do
    library=$(runtime_library "libQt5${component}.so.5") || die "Qt5 $component runtime is missing"
    copy_library "$library"
done
plugin_directory=''
for directory in /usr/lib/x86_64-linux-gnu/qt5/plugins/platforms /usr/lib/qt5/plugins/platforms /opt/builddeps/usr/lib/x86_64-linux-gnu/qt5/plugins/platforms; do
    if [[ -r "$directory/libqxcb.so" ]]; then plugin_directory=$directory; break; fi
done
[[ -n "$plugin_directory" ]] || die 'the Qt xcb platform plugin is missing'
for plugin in libqxcb.so libqoffscreen.so libqminimal.so; do
    [[ -r "$plugin_directory/$plugin" ]] || die "the Qt platform plugin $plugin is missing"
    cp -L -- "$plugin_directory/$plugin" "$bundle/plugins/platforms/$plugin"
    queue+=("$plugin_directory/$plugin")
    add_notice "$plugin_directory/$plugin" "plugins/platforms/$plugin"
done

# Discover the entire static DT_NEEDED closure, including Qt plugin libraries.
# No host library is replaced and no environment-wide LD_LIBRARY_PATH is set.
index=0
runtime_search=/usr/lib/x86_64-linux-gnu:/lib/x86_64-linux-gnu
if [[ -d /opt/builddeps/usr/lib/x86_64-linux-gnu ]]; then
    runtime_search="$runtime_search:/opt/builddeps/usr/lib/x86_64-linux-gnu"
fi
while (( index < ${#queue[@]} )); do
    object=${queue[index]}
    index=$((index + 1))
    [[ -v "seen[$object]" ]] && continue
    seen[$object]=1
    # The ELF interpreter is self-contained; ldd may report it as statically
    # linked with a nonzero exit status even though it is the required loader.
    [[ $(basename -- "$object") == ld-linux-x86-64.so.2 ]] && continue
    dependencies=$(env -u LD_PRELOAD -u LD_AUDIT -u LD_DEBUG LD_LIBRARY_PATH="$runtime_search" ldd -- "$object" 2>&1) || die "cannot inspect dependencies of $(basename -- "$object")"
    if [[ "$dependencies" == *'not found'* ]]; then
        printf '%s\n' "$dependencies" | awk '/not found/ {print $1 " is missing"}' >&2
        die 'runtime dependency closure is incomplete'
    fi
    while IFS= read -r library; do
        [[ -n "$library" ]] && copy_library "$library"
    done < <(printf '%s\n' "$dependencies" | awk '/=>/ && $3 ~ /^\// {print $3} /^[[:space:]]*\// {print $1}')
done
[[ -r "$bundle/lib/ld-linux-x86-64.so.2" ]] || die 'a matching bundled dynamic loader was not found'
if [[ -s "$missing" ]]; then
    printf 'Missing or unresolved dependency copyright notices:\n' >&2
    sort -u "$missing" >&2
    die 'no archive was produced; supply the matching Debian package notices and retry'
fi

cat > "$bundle/syspulse" <<'LAUNCHER'
#!/bin/sh
set -eu
APPDIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P)
# Match the bundled libc with its own loader without changing helper-program
# library resolution. Do not export LD_LIBRARY_PATH to systemctl/pkexec/etc.
unset LD_LIBRARY_PATH LD_PRELOAD LD_AUDIT LD_DEBUG
QT_PLUGIN_PATH="$APPDIR/plugins"
QT_QPA_PLATFORM_PLUGIN_PATH="$APPDIR/plugins/platforms"
QT_QPA_PLATFORM=${QT_QPA_PLATFORM:-xcb}
export QT_PLUGIN_PATH QT_QPA_PLATFORM_PLUGIN_PATH QT_QPA_PLATFORM
exec "$APPDIR/lib/ld-linux-x86-64.so.2" --library-path "$APPDIR/lib" "$APPDIR/bin/syspulse" "$@"
LAUNCHER
chmod 755 "$bundle/syspulse"
cat > "$bundle/README.txt" <<EOF
SysPulse $version — experimental Linux x86_64 portable bundle
Developer: Sudip Das

Extract the archive, then run ./syspulse as your normal desktop user.
No system installation or replacement of system libraries is performed.
The launcher uses the bundled matching dynamic loader and runtime libraries.
Keep the complete directory together; bin/syspulse is not the portable launcher.

The bundle includes Qt5 Core, Gui, Widgets and Network, xcb/offscreen/minimal
platform plugins, and their recursive linked dependencies. Host X11/XWayland,
fonts, session services and administrator helpers remain necessary. Optional
graphics drivers, TLS backends, Wayland plugins and tools such as systemctl,
pkexec, apt-get and xfconf-query remain host capabilities; they are not bundled.
Use QT_QPA_PLATFORM=offscreen only for headless VM verification.

Bundling libc avoids a direct dependence on the target distribution's libc
version, but does not prove compatibility with every Mint desktop, kernel,
graphics driver or session. Linux Mint/XFCE desktop validation is still required
before this archive is described as verified for that target.

Dependency notices are in licenses/packages and licenses/common-licenses.
DEPENDENCIES.tsv identifies each library and its notice provenance. Where dpkg
ownership metadata was unavailable, a known Debian SONAME-to-package mapping
selected an existing copyright notice; package/source versions are explicitly
marked unavailable rather than guessed. These rows are not exact source-version
attestations. Build source and dependency source-version provenance must be
provided separately before any broader redistribution-compliance claim.

The application is GPL-3.0-or-later. See licenses/SysPulse-GPL-3.0.txt.
Dependency copyright files document their own licenses and source locations.
Libraries remain separate and replaceable. Use matching ABI versions when
replacing them. Source provenance and native package artifacts should accompany
the same SysPulse release.
EOF

epoch=${SOURCE_DATE_EPOCH:-0}
[[ "$epoch" =~ ^[0-9]+$ ]] || die 'SOURCE_DATE_EPOCH must contain only digits'
(
    cd -- "$bundle"
    find . -type f ! -name CONTENTS.sha256 -print0 | sort -z | xargs -0 sha256sum > CONTENTS.sha256
)
tar --sort=name --owner=0 --group=0 --numeric-owner --mtime="@$epoch" -C "$staging" -czf "$output/$name.tar.gz" "$name"
(
    cd -- "$output"
    sha256sum "$name.tar.gz" > "$name.tar.gz.sha256"
)
printf 'Created %s.tar.gz and checksum. Target Mint/XFCE compatibility remains subject to VM verification.\n' "$name"
