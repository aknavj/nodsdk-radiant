"""Assemble an installer payload from trusted runtime files, not a developer install."""

import json
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import tempfile
import zipfile


SDK_TOOLS = (
    "NODTemplates", "NOTEditor", "NSDEditor", "MAPtoNIL", "MAPtoNSD",
    "MAPtoNPD", "NILtoMAP", "NODtoNMB", "NODscene", "NODScript", "NODAssetViewer",
)


def pe_machine(filename):
    with filename.open("rb") as stream:
        header = stream.read(64)
        if len(header) != 64 or header[:2] != b"MZ":
            raise ValueError(f"Not a Windows executable: {filename}")
        stream.seek(struct.unpack_from("<I", header, 60)[0])
        signature = stream.read(6)
    if len(signature) != 6 or signature[:4] != b"PE\0\0":
        raise ValueError(f"Invalid Windows executable: {filename}")
    machine = struct.unpack_from("<H", signature, 4)[0]
    if machine not in (0x14c, 0x8664):
        raise ValueError(f"Unsupported Windows architecture: {filename}")
    return machine


def copy_binary(source, destination, machine):
    if pe_machine(source) != machine:
        raise ValueError(f"Binary architecture mismatch: {source}")
    data = source.read_bytes().lower()
    if any(name in data for name in (b"msvcp140d.dll", b"vcruntime140d.dll", b"ucrtbased.dll")):
        raise ValueError(f"Debug runtime dependency cannot be distributed: {source}")
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, destination)


def validate_dependencies(folder, studio):
    tools = list(studio.glob("VC/Tools/MSVC/*/bin/Hostx64/x64/dumpbin.exe"))
    if not tools:
        raise FileNotFoundError("Visual Studio dumpbin is required to validate release dependencies.")
    dumpbin = max(tools, key=lambda filename: tuple(map(int, filename.parents[3].name.split("."))))
    for binary in folder.rglob("*"):
        if binary.suffix.lower() not in (".dll", ".exe"):
            continue
        machine = pe_machine(binary)
        system = Path(os.environ["SystemRoot"])
        system = system / ("SysWOW64" if machine == 0x14c and (system / "SysWOW64").is_dir() else "System32")
        output = subprocess.check_output([str(dumpbin), "/nologo", "/dependents", str(binary)],
                                         text=True, errors="replace")
        for name in re.findall(r"^\s+([\w.+-]+\.dll)\s*$", output, re.MULTILINE | re.IGNORECASE):
            if name.lower().startswith(("api-ms-", "ext-ms-")):
                continue
            local = (binary.parent / name, folder / name)
            if any(candidate.is_file() and pe_machine(candidate) == machine for candidate in local):
                continue
            if not (system / name).is_file():
                raise FileNotFoundError(f"Missing runtime dependency: {binary.relative_to(folder)} needs {name}")


def package_release(root, sdk, modules, runtime_files, studio):
    version = (root / "include" / "version.default").read_text().strip()
    if not re.fullmatch(r"\d+\.\d+\.\d+", version):
        raise ValueError("Invalid release version.")
    sdk_machine = pe_machine(sdk / "NODTemplates.exe")
    architecture = "x64" if sdk_machine == 0x8664 else "x86"
    name = f"OpenNOD-Radiant-{version}-windows-{architecture}"
    dist = root / "dist"
    if dist.is_symlink() or (dist.exists() and getattr(dist.stat(), "st_file_attributes", 0) & 0x400):
        raise RuntimeError(f"Release output cannot be a linked directory: {dist}")
    dist.mkdir(exist_ok=True)
    destination, archive = dist / name, dist / (name + ".zip")
    if destination.exists() or archive.exists():
        raise FileExistsError(f"Release output already exists. Move or remove {destination} and {archive} before repackaging.")

    with tempfile.TemporaryDirectory(prefix=".package-", dir=dist) as temporary:
        staging = Path(temporary) / name
        staging.mkdir()
        copy_binary(root / "install" / "radiant.exe", staging / "radiant.exe", 0x14c)
        for module in modules:
            relative = Path("modules") / (module + ".dll")
            copy_binary(root / "install" / relative, staging / relative, 0x14c)

        for source, relative in runtime_files:
            # Optional gettext C++ wrapper; Radiant does not use its MinGW C++ dependency.
            if relative.name == "libasprintf-0.dll":
                continue
            if relative.suffix in (".immodules", ".modules"):
                continue
            if relative.parts[0] == "share" and relative.parts[1] not in (
                    "doc", "locale", "themes", "gtk-2.0", "glib-2.0"):
                continue
            target = staging / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            if source.suffix.lower() in (".exe", ".dll"):
                copy_binary(source, target, 0x14c)
            else:
                shutil.copy2(source, target)

        files = subprocess.check_output([
            "git", "-C", str(root), "ls-files", "-z", "--",
            "install/base", "install/bitmaps", "install/modules/bitmaps", "gamepacks/SCGhostPack",
        ]).decode("utf-8").split("\0")
        for filename in filter(None, files):
            relative = Path(filename)
            target = (Path("installs") / relative.relative_to("gamepacks")
                      if relative.parts[0] == "gamepacks" else relative.relative_to("install"))
            if target.suffix in (".pref", ".game", ".log", ".pdb"):
                raise ValueError(f"Personal configuration cannot be packaged: {relative}")
            (staging / target).parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(root / relative, staging / target)

        tools = staging / "nod-editors"
        for tool in SDK_TOOLS:
            copy_binary(sdk / (tool + ".exe"), tools / (tool + ".exe"), sdk_machine)
        for filename in sdk.glob("*.dll"):
            copy_binary(filename, tools / filename.name, sdk_machine)
        for filename in ("README.md", "CHANGELOG.md"):
            shutil.copy2(sdk / filename, tools / filename)
        for filename in ("LICENSE", "LICENSE.txt", "COPYING"):
            if (sdk / filename).is_file():
                shutil.copy2(sdk / filename, tools / filename)
        if (sdk / "licenses").is_dir():
            shutil.copytree(sdk / "licenses", tools / "licenses")

        versions = [directory for directory in (studio / "VC" / "Redist" / "MSVC").iterdir()
                    if re.fullmatch(r"\d+(?:\.\d+)+", directory.name)]
        if not versions:
            raise FileNotFoundError("Visual Studio MSVC redistributable files are missing.")
        redist = max(versions, key=lambda directory: tuple(map(int, directory.name.split("."))))
        for machine, target in ((0x14c, staging), (sdk_machine, tools)):
            platform = "x64" if machine == 0x8664 else "x86"
            libraries = list((redist / platform / "Microsoft.VC143.CRT").glob("*.dll"))
            if not libraries:
                raise FileNotFoundError(f"Missing redistributable C++ runtime for {platform}.")
            for filename in libraries:
                copy_binary(filename, target / filename.name, machine)

        for filename in ("README.md", "CHANGELOG.md", "LICENSE", "GPL", "GPL-3.0", "LGPL", "BSD", "CONTRIBUTORS"):
            shutil.copy2(root / filename, staging / filename)
        notices = staging / "licenses"
        dependencies = root / "libs" / "nodsdk-scghost"
        for filename in dependencies.rglob("*"):
            if filename.is_file() and filename.name.upper().startswith(("LICENSE", "COPYING")):
                target = notices / "OpenNOD-SDK" / filename.relative_to(dependencies)
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(filename, target)
        glm_notice = root / "libs" / "glm" / "copying.txt"
        notices.mkdir(exist_ok=True)
        shutil.copy2(glm_notice, notices / "GLM.txt")
        metadata = {"product": "OpenNOD Radiant", "version": version,
                    "editor_architecture": "x86", "sdk_architecture": architecture}
        (staging / "release.json").write_text(json.dumps(metadata, indent=2) + "\n", encoding="utf-8")
        (staging / "START-HERE.txt").write_text(
            f"OpenNOD Radiant {version} - Windows {architecture}\n\n"
            "Install/extract into a writable per-user folder. Set shortcut working directory to this folder.\n"
            "Launch radiant.exe, choose Configure games > StarCraft Ghost (NOD authoring).\n"
            "Select a separate writable authoring workspace and your own asset root containing Materials.\n"
            "Bundled SDK tools are in nod-editors; no SDK installation is required.\n"
            "No proprietary assets, personal profiles, or user maps are included.\n\n"
            "Source: https://github.com/aknavj/nodsdk-radiant\n"
            "SDK source: https://github.com/aknavj/nodsdk-scghost\n"
            "Redistributors must provide corresponding source under the applicable GPL/LGPL terms.\n",
            encoding="utf-8",
        )
        validate_dependencies(staging, studio)
        temporary_zip = Path(temporary) / archive.name
        with zipfile.ZipFile(temporary_zip, "w", compression=zipfile.ZIP_DEFLATED) as output:
            for filename in sorted(staging.rglob("*")):
                if filename.is_file():
                    output.write(filename, filename.relative_to(staging.parent).as_posix())
        with zipfile.ZipFile(temporary_zip) as output:
            if output.testzip() is not None:
                raise RuntimeError("Release ZIP failed integrity validation.")
        staging.replace(destination)
        temporary_zip.replace(archive)
    print(f"Installer staging folder: {destination}\nRelease ZIP: {archive}")
    return destination, archive
