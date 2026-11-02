"""Build or clean the SC Ghost editor using the existing project layout."""

import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import urllib.request
import xml.etree.ElementTree as ET
import zipfile


ROOT = Path(__file__).resolve().parent
RUNTIME = ROOT / "install"
MODULES = ("entity", "image", "imagepng", "map", "mapxml", "model",
           "shaders", "surface", "vfspk3")
LIBRARIES = ("cmdlib", "mathlib", "l_net", "synapse", "picomodel", "ddslib")
ARCHIVES = {
    "gtk-2.24.10": "gtk-bundle-2.24.10-GtkRadiant.zip",
    "gtkglext-1.2.0": "gtkglext-1.2.0-3-win32.zip",
    "jpeg-9": "jpeg-9-MSVC2014-GtkRadiant.zip",
    "libxml2-2.9.2": "libxml2-2.9.2-GtkRadiant-2.zip",
}


def run(args, **kwargs):
    print("+", subprocess.list2cmdline([str(arg) for arg in args]), flush=True)
    return subprocess.run([str(arg) for arg in args], cwd=ROOT, check=True, **kwargs)


def path(value):
    if os.name == "nt" and value.startswith("/"):
        value = subprocess.check_output(["cygpath", "-w", value], text=True).strip()
    return Path(value).expanduser().resolve()


def copy_missing_tree(source, target):
    for filename in source.rglob("*"):
        if filename.is_file():
            destination = target / filename.relative_to(source)
            if not destination.exists():
                destination.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(filename, destination)


def windows_dependencies():
    for directory, archive in ARCHIVES.items():
        target = ROOT.parent / directory
        if target.is_dir():
            continue
        filename = ROOT / archive
        if not filename.is_file():
            temporary = filename.with_suffix(".part")
            urllib.request.urlretrieve("https://s3.amazonaws.com/GtkRadiant/" + archive, temporary)
            temporary.replace(filename)
        with zipfile.ZipFile(filename) as bundle:
            for member in bundle.namelist():
                if not (ROOT.parent / member).resolve().is_relative_to(target.resolve()):
                    raise RuntimeError(f"Unsafe archive member: {member}")
            bundle.extractall(ROOT.parent)
        if not target.is_dir():
            raise RuntimeError(f"{archive} did not provide {directory}")
    for source, relative in windows_runtime_files():
        destination = RUNTIME / relative
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, destination)


def windows_runtime_files():
    gtk = ROOT.parent / "gtk-2.24.10"
    patterns = (
        (gtk, "bin/*.dll"), (gtk, "bin/gspawn-*.exe"), (gtk, "lib/*.dll"),
        (gtk, "lib/gtk-2.0/**/*.dll"),
        (ROOT.parent / "gtkglext-1.2.0", "bin/*.dll"),
    )
    for directory, pattern in patterns:
        for filename in directory.glob(pattern):
            yield filename, Path(filename.name)
    for name in ("etc", "share"):
        directory = gtk / name
        if not directory.is_dir():
            raise FileNotFoundError(f"Missing GTK runtime directory: {directory}")
        for filename in directory.rglob("*"):
            if filename.is_file():
                yield filename, Path(name) / filename.relative_to(directory)


def find_visual_studio():
    finder = Path(os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)")) / "Microsoft Visual Studio" / "Installer" / "vswhere.exe"
    installation = subprocess.check_output(
        [str(finder), "-latest", "-products", "*", "-requires",
         "Microsoft.VisualStudio.Component.VC.Tools.x86.x64", "-property", "installationPath"],
        text=True,
    ).strip()
    if not installation:
        raise RuntimeError("Install Visual Studio 2022 with the C++ workload and a Windows SDK.")
    return Path(installation)


def windows_build(args, clean=False):
    environment = find_visual_studio() / "Common7" / "Tools" / "VsDevCmd.bat"
    projects = ("radiant",) + MODULES + LIBRARIES
    targets = ";".join(name + (":Clean" if clean else "") for name in projects)
    with tempfile.TemporaryDirectory(prefix="radiant-build-") as temporary:
        command = Path(temporary) / "build.cmd"
        command.write_text(
            f'@echo off\ncall "{environment}" -arch=x86 -host_arch=x64\n'
            'if errorlevel 1 exit /b %errorlevel%\n'
            f'cd /d "{ROOT}"\n'
            f'msbuild radiant.sln "/t:{targets}" /p:Configuration={args.configuration} '
            f'/p:Platform=Win32 /m:{args.jobs} /nologo\nexit /b %errorlevel%\n',
            encoding="utf-8",
        )
        run([os.environ.get("COMSPEC", "cmd.exe"), "/d", "/c", command])


def resolve_sdk(args):
    profile = RUNTIME / "scghost.game"
    defaults = ET.parse(profile).getroot().attrib if profile.exists() else {}
    sdk = args.sdk_dir or defaults.get("nod_editorspath")
    if not sdk:
        candidates = list((ROOT.parent / "nodsdk-scghost").glob("OpenNOD-SDK-*-*"))
        if len(candidates) == 1:
            sdk = str(candidates[0])
    if not sdk:
        raise RuntimeError("Provide --sdk-dir DIR or set NOD_SDK_DIR.")
    sdk = path(sdk)
    suffix = ".exe" if os.name == "nt" else ""
    for name in ("NODTemplates", "NOTEditor", "NSDEditor"):
        if not (sdk / (name + suffix)).is_file():
            raise RuntimeError(f"Missing SDK executable: {sdk / (name + suffix)}")
    return sdk


def configure(args):
    sdk = resolve_sdk(args)
    profile = RUNTIME / "scghost.game"
    defaults = ET.parse(profile).getroot().attrib if profile.exists() else {}
    assets = args.asset_root or defaults.get("nod_assetpath")
    if not assets:
        raise RuntimeError("Provide --asset-root DIR or set NOD_ASSET_ROOT.")
    assets = path(assets)
    if not (assets / "Materials").is_dir():
        raise RuntimeError(f"Asset root must be the parent of Materials: {assets}")
    platform = "win32" if os.name == "nt" else "linux"
    workspace = args.workspace or defaults.get("enginepath_" + platform)
    workspace = path(workspace) if workspace else RUNTIME / "workspace"
    if workspace == assets or workspace.is_relative_to(assets) or assets.is_relative_to(workspace):
        raise RuntimeError("Authoring workspace and original asset root must not overlap.")
    return sdk, assets, workspace


def stage_profile(sdk, assets, workspace):
    pack = ROOT / "gamepacks" / "SCGhostPack"
    tools = RUNTIME / "installs" / "SCGhostPack" / "game"
    shutil.copytree(pack / "game", tools, dirs_exist_ok=True)
    copy_missing_tree(pack / "install", workspace)
    filename = RUNTIME / "scghost.game"
    profile = ET.parse(filename) if filename.exists() else ET.ElementTree(ET.Element("game"))
    platform = "win32" if os.name == "nt" else "linux"
    profile.getroot().attrib.update({
        "name": "StarCraft Ghost (NOD authoring)", "basegame": "nod", "prefix": ".scghost",
        "enginepath_" + platform: str(workspace), "gametools_" + platform: str(tools),
        "engine_" + platform: "", "mp_engine_" + platform: "",
        "nod_assetpath": str(assets), "nod_editorspath": str(sdk),
        "no_patch": "true", "no_maps_in_home": "true", "default_scale": "1.0",
        "caulk_shader": "textures/nod/placeholder",
    })
    profile.write(filename, encoding="utf-8", xml_declaration=True)
    preferences = RUNTIME / "global.pref"
    if not preferences.exists():
        root = ET.Element("qpref", version="1")
        ET.SubElement(root, "epair", name="gamefile").text = "scghost.game"
        ET.SubElement(root, "epair", name="autoload").text = "true"
        ET.ElementTree(root).write(preferences, encoding="utf-8", xml_declaration=True)
    catalog = run([sdk / ("NODTemplates.exe" if os.name == "nt" else "NODTemplates"),
                   "--workspace", workspace / "nod", "--asset-root", assets, "--details", "1"],
                  stdout=subprocess.PIPE, text=True)
    templates = ET.fromstring(catalog.stdout).findall("template")
    count = sum(item.get("placeable") == "1" for item in templates)
    if not count or any(item.get("details") != "1" for item in templates):
        raise RuntimeError("SDK did not return a detailed, placeable NOT entity catalog.")
    print(f"Verified {count} placeable NOT templates.")


def build(args):
    packaging = getattr(args, "package", False)
    if packaging:
        sdk = resolve_sdk(args)
    else:
        sdk, assets, workspace = configure(args)
    run([sys.executable, "dependencies.py"])
    run([sys.executable, "makeversion.py"])
    RUNTIME.mkdir(parents=True, exist_ok=True)
    if os.name == "nt":
        windows_dependencies()
        windows_build(args)
    else:
        run([sys.executable, "-m", "SCons", "op=clear", "target=radiant",
             f"config={args.configuration.lower()}", "--no-packs", f"-j{args.jobs}"])
    extension = ".dll" if os.name == "nt" else ".so"
    executable = RUNTIME / ("radiant.exe" if os.name == "nt" else "radiant.bin")
    for filename in [executable] + [RUNTIME / "modules" / (name + extension) for name in MODULES]:
        if not filename.is_file():
            raise RuntimeError(f"Build output is missing: {filename}")
    if packaging:
        from package_release import package_release
        package_release(ROOT, sdk, MODULES, windows_runtime_files(), find_visual_studio())
    else:
        stage_profile(sdk, assets, workspace)
        print(f"Build complete. Run {executable.name} from {RUNTIME}")


def clean(args):
    if os.name == "nt":
        windows_build(args, clean=True)
    else:
        run([sys.executable, "-m", "SCons", "op=clear", "target=radiant",
             f"config={args.configuration.lower()}", "--no-packs", "-c"])
    print("Compiler outputs cleaned. Dependencies, profiles, preferences and maps kept.")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", choices=("build", "clean"))
    parser.add_argument("--sdk-dir", default=os.environ.get("NOD_SDK_DIR"))
    parser.add_argument("--asset-root", default=os.environ.get("NOD_ASSET_ROOT"))
    parser.add_argument("--workspace", default=os.environ.get("NOD_WORKSPACE"))
    parser.add_argument("--configuration", choices=("Release", "Debug"), default="Release")
    parser.add_argument("--jobs", type=int, default=os.cpu_count() or 1)
    parser.add_argument("--package", action="store_true",
                        help="Build a Windows Release staging folder and ZIP in dist; no game assets required.")
    args = parser.parse_args()
    if args.jobs < 1:
        parser.error("--jobs must be positive")
    if sys.platform not in ("win32", "linux"):
        parser.error("Supported platforms are Windows (Git Bash) and Linux.")
    if args.package and (args.command != "build" or args.configuration != "Release" or os.name != "nt"):
        parser.error("--package requires a Windows Release build.")
    if args.command == "build":
        build(args)
    else:
        clean(args)


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, RuntimeError, subprocess.CalledProcessError, ET.ParseError,
            zipfile.BadZipFile) as error:
        print(f"SC Ghost build: {error}", file=sys.stderr)
        sys.exit(1)
